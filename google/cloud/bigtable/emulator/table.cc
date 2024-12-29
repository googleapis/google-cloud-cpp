// Copyright 2024 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <chrono>
#include "google/cloud/bigtable/emulator/table.h"
#include "google/cloud/bigtable/emulator/filter.h"
#include "google/cloud/bigtable/emulator/row_iterators.h"
#include "google/protobuf/util/field_mask_util.h"
#include "google/cloud/internal/make_status.h"

namespace google {
namespace cloud {
namespace bigtable {
namespace emulator {

namespace btadmin = ::google::bigtable::admin::v2;
namespace btproto = ::google::bigtable::v2;

StatusOr<std::shared_ptr<Table>> Table::Create(
    google::bigtable::admin::v2::Table schema) {
  std::shared_ptr<Table> res(new Table);
  auto status = res->Construct(std::move(schema));
  if (!status.ok()) {
    return status;
  }
  return res;
}

Status Table::Construct(google::bigtable::admin::v2::Table schema) {
  // Normally the constructor acts as a synchronization point. We don't have
  // that luxury here, so we need to make sure that the changes performed in
  // this member function are reflected in other threads. The simplest way to do
  // this is the mutex.
  std::lock_guard lock(mu_);
  schema_ = std::move(schema);
  if (schema_.granularity() ==
      btadmin::Table::TIMESTAMP_GRANULARITY_UNSPECIFIED) {
    schema_.set_granularity(btadmin::Table::MILLIS);
  }
  if (schema_.cluster_states_size() > 0) {
    return InvalidArgumentError(
        "`cluster_states` not empty.",
        GCP_ERROR_INFO().WithMetadata("schema", schema.DebugString()));
  }
  if (schema_.has_restore_info()) {
    return InvalidArgumentError(
        "`restore_info` not empty.",
        GCP_ERROR_INFO().WithMetadata("schema", schema.DebugString()));
  }
  if (schema_.has_change_stream_config()) {
    return UnimplementedError(
        "`change_stream_config` not empty.",
        GCP_ERROR_INFO().WithMetadata(
            "schema", schema.DebugString()));
  }
  if (schema_.has_automated_backup_policy()) {
    return UnimplementedError(
        "`automated_backup_policy` not empty.",
        GCP_ERROR_INFO().WithMetadata(
            "schema", schema.DebugString()));
  }
  for (auto const &column_family_def : schema_.column_families()) {
    column_families_.emplace(
        column_family_def.first,
        std::make_shared<ColumnFamily>());
  }
  return Status();
}

StatusOr<btadmin::Table> Table::ModifyColumnFamilies(
    btadmin::ModifyColumnFamiliesRequest const& request) {
  std::cout << "Modify column families: " << request.DebugString()
            << std::endl;
  std::unique_lock lock(mu_);
  auto new_schema = schema_;
  auto new_column_families = column_families_;
  for (auto const& modification : request.modifications()) {
    if (modification.drop()) {
      if (IsDeleteProtectedNoLock()) {
        return FailedPreconditionError(
            "The table has deletion protection.",
            GCP_ERROR_INFO().WithMetadata("modification",
                                          modification.DebugString()));
      }
      if (new_column_families.erase(modification.id()) == 0) {
        return NotFoundError(
            "No such column family.",
            GCP_ERROR_INFO().WithMetadata("modification",
                                          modification.DebugString()));
      }
      if (new_schema.mutable_column_families()->erase(modification.id()) == 0) {
        return InternalError(
            "Column family with no schema.",
            GCP_ERROR_INFO().WithMetadata("modification",
                                          modification.DebugString()));
      }
    } else if (modification.has_update()) {
      auto& cfs = *new_schema.mutable_column_families();
      auto cf_it = cfs.find(modification.id());
      if (cf_it == cfs.end()) {
        return NotFoundError(
            "No such column family.",
            GCP_ERROR_INFO().WithMetadata("modification",
                                          modification.DebugString()));
      }
      using google::protobuf::util::FieldMaskUtil;

      using google::protobuf::util::FieldMaskUtil;
      google::protobuf::FieldMask effective_mask;
      if (modification.has_update_mask()) {
        effective_mask = modification.update_mask();
        if (!FieldMaskUtil::IsValidFieldMask<
                google::bigtable::admin::v2::ColumnFamily>(effective_mask)) {
          return InvalidArgumentError(
              "Update mask is invalid.",
              GCP_ERROR_INFO().WithMetadata("modification",
                                            modification.DebugString()));
        }
      } else {
        FieldMaskUtil::FromString("gc_rule", &effective_mask);
        if (!FieldMaskUtil::IsValidFieldMask<
                google::bigtable::admin::v2::ColumnFamily>(effective_mask)) {
          return InternalError("Default update mask is invalid.",
                               GCP_ERROR_INFO().WithMetadata(
                                   "mask", effective_mask.DebugString()));
        }
      }
      FieldMaskUtil::MergeMessageTo(modification.update(), effective_mask,
                                    FieldMaskUtil::MergeOptions(),
                                    &(cf_it->second));
    } else if (modification.has_create()) {
      if (!new_column_families
               .emplace(modification.id(), std::make_shared<ColumnFamily>())
               .second) {
        return AlreadyExistsError(
            "Column family already exists.",
            GCP_ERROR_INFO().WithMetadata("modification",
                                          modification.DebugString()));
      }
      if (!new_schema.mutable_column_families()
               ->emplace(modification.id(), modification.create())
               .second) {
        return InternalError(
            "Column family with schema but no data.",
            GCP_ERROR_INFO().WithMetadata("modification",
                                          modification.DebugString()));
      }
    } else {
      return UnimplementedError(
          "Unsupported modification.",
          GCP_ERROR_INFO().WithMetadata("modification",
                                        modification.DebugString()));
    }
  }
  // Defer destorying potentially large objects to after releasing the lock.
  column_families_.swap(new_column_families);
  schema_ = new_schema;
  lock.unlock();
  return new_schema;
}

google::bigtable::admin::v2::Table Table::GetSchema() const {
  std::lock_guard lock(mu_);
  return schema_;
}

Status Table::Update(google::bigtable::admin::v2::Table const& new_schema,
                     google::protobuf::FieldMask const& to_update) {
  std::cout << "Update schema: " << new_schema.DebugString()
            << " mask: " << to_update.DebugString() << std::endl;
  using google::protobuf::util::FieldMaskUtil;
  google::protobuf::FieldMask allowed_mask;
  FieldMaskUtil::FromString(
      "change_stream_config,"
      "change_stream_config.retention_period,"
      "deletion_protection",
      &allowed_mask);
  if (!FieldMaskUtil::IsValidFieldMask<google::bigtable::admin::v2::Table>(
          to_update)) {
    return InvalidArgumentError(
        "Update mask is invalid.",
        GCP_ERROR_INFO().WithMetadata(
            "mask", to_update.DebugString()));
  }
  google::protobuf::FieldMask disallowed_mask;
  FieldMaskUtil::Subtract<google::bigtable::admin::v2::Table>(
      to_update, allowed_mask, &disallowed_mask);
  if (disallowed_mask.paths_size() > 0) {
    return UnimplementedError(
        "Update mask contains disallowed fields.",
        GCP_ERROR_INFO().WithMetadata(
            "mask", disallowed_mask.DebugString()));
  }
  std::lock_guard lock(mu_);
  FieldMaskUtil::MergeMessageTo(new_schema, to_update,
                                FieldMaskUtil::MergeOptions(), &schema_);
  return Status();
}

template <typename MESSAGE>
StatusOr<std::reference_wrapper<ColumnFamily>> Table::FindColumnFamily(
    MESSAGE const& message) const {
  auto column_family_it = column_families_.find(message.family_name());
  if (column_family_it == column_families_.end()) {
    return NotFoundError(
        "No such column family.",
        GCP_ERROR_INFO().WithMetadata("mutation", message.DebugString()));
  }
  return std::ref(*column_family_it->second);
}

Status Table::MutateRow(
    google::bigtable::v2::MutateRowRequest const &request) {
  // FIXME - add atomicity
  // FIXME - determine what happens when row/column family/column does not exist
  std::lock_guard lock(mu_);
  assert(request.table_name() == schema_.name());
  for (auto mutation : request.mutations()) {
    if (mutation.has_set_cell()) {
      auto const & set_cell = mutation.set_cell();
      auto maybe_column_family = FindColumnFamily(set_cell);
      if (!maybe_column_family) {
        return maybe_column_family.status();
      }
      maybe_column_family->get().SetCell(
          request.row_key(), set_cell.column_qualifier(),
          std::chrono::duration_cast<std::chrono::milliseconds>(
              std::chrono::microseconds(set_cell.timestamp_micros())),
          set_cell.value());
    } else if (mutation.has_add_to_cell()) {
      // FIXME
    } else if (mutation.has_merge_to_cell()) {
      // FIXME
    } else if (mutation.has_delete_from_column()) {
      auto const & delete_from_column = mutation.delete_from_column();
      auto maybe_column_family =
          FindColumnFamily(delete_from_column);
      if (!maybe_column_family) {
        return maybe_column_family.status();
      }
      if (maybe_column_family->get().DeleteColumn(
              request.row_key(), delete_from_column.column_qualifier(),
              delete_from_column.time_range()) == 0) {
        // FIXME no such row or column
      }
    } else if (mutation.has_delete_from_family()) {
      auto maybe_column_family =
          FindColumnFamily(mutation.delete_from_family());
      if (!maybe_column_family) {
        return maybe_column_family.status();
      }
      if (maybe_column_family->get().DeleteRow(request.row_key())) {
        // FIXME no such row existed in that column family
      }
    } else if (mutation.has_delete_from_row()) {
      bool row_existed = false;
      for (auto& column_family : column_families_) {
        row_existed |= column_family.second->DeleteRow(request.row_key());
      }
      if (!row_existed) {
        // FIXME no such row existed
      }
    } else {
      return UnimplementedError(
          "Unsupported mutation type.",
          GCP_ERROR_INFO().WithMetadata("mutation", mutation.DebugString()));
    }
  }
  return Status();
}

class ExtendWithColumnFamilyName {
 public:
  using ExtendedType = std::tuple<std::string const&, std::string const&,
                                  ColumnFamilyRow const&> const;

  explicit ExtendWithColumnFamilyName(std::string const& column_family_name)
      : column_family_name_(std::cref(column_family_name)) {}

  ExtendedType operator()(
      std::iterator_traits<ColumnFamily::const_iterator>::reference
          row_key_and_column) const {
    return ExtendedType(row_key_and_column.first, column_family_name_.get(),
                            row_key_and_column.second);
  }

 private:
  std::reference_wrapper<std::string const> column_family_name_;
};

struct RowKeyLess {
  bool operator()(
      TransformIterator<ColumnFamily::const_iterator,
                        ExtendWithColumnFamilyName>::value_type const& lhs,
      TransformIterator<ColumnFamily::const_iterator,
                        ExtendWithColumnFamilyName>::value_type const& rhs)
      const {
    auto row_key_cmp =
        internal::CompareRowKey(std::get<0>(lhs), std::get<0>(rhs));
    if (row_key_cmp == 0) {
      return internal::CompareColumnQualifiers(std::get<1>(lhs),
                                               std::get<1>(rhs)) < 0;
    }
    return row_key_cmp < 0;
  }
};

struct DescendToColumn {
  ColumnFamilyRow const& operator()(
      std::tuple<std::string const&, std::string const&,
                 ColumnFamilyRow const&> const& column_family_row) const {
    return std::get<2>(column_family_row);
  }
};

struct CombineColumnIterators {
  using ReturnType =
      std::tuple<std::string const&, std::string const&, std::string const&,
                 ColumnRow const&> const;
  ReturnType operator()(
      std::tuple<std::string const&, std::string const&,
                 ColumnFamilyRow const&> const& column_family_row,
      std::pair<std::string const, ColumnRow> const& column_row) const {
    return ReturnType(std::get<0>(column_family_row),
                      std::get<1>(column_family_row), column_row.first,
                      column_row.second);
  }
};


struct DescendToCell {
  ColumnRow const& operator()(
                CombineColumnIterators::ReturnType const &column_row) const {
    return std::get<3>(column_row);
  }
};

struct CombineCellIterators {
  using ReturnType = CellView;
  ReturnType operator()(CombineColumnIterators::ReturnType const& column_row,
                        std::pair<std::chrono::milliseconds const,
                                  std::string> const& cell) const {
    static_assert(
        std::is_same<std::pair<std::chrono::milliseconds const, std::string>,
                     ColumnRow::const_iterator::value_type>::value);
    return ReturnType(std::get<0>(column_row),
                      std::get<1>(column_row),
                      std::get<2>(column_row),
                      cell.first,
                      cell.second);
  }
};

CellStream Table::ReadRowsInternal(
    std::shared_ptr<SortedRowSet> row_set) const {
  using CFWithNameIt = TransformIterator<ColumnFamily::const_iterator,
                                         ExtendWithColumnFamilyName>;
  std::vector<std::pair<CFWithNameIt, CFWithNameIt>> cf_ranges;

  std::transform(
      column_families_.begin(), column_families_.end(),
      std::back_inserter(cf_ranges), [&](auto const& column_family) {
        ExtendWithColumnFamilyName transformer(column_family.first);
        return std::make_pair(
            CFWithNameIt(column_family.second->FindRows(row_set), transformer),
            CFWithNameIt(column_family.second->end(), transformer));
      });

  using CFRowsIt = MergedSortedIterator<CFWithNameIt, RowKeyLess>;
  CFRowsIt cfrows_begin(std::move(cf_ranges));
  CFRowsIt cfrows_end;

  using ColRowsIt =
      FlattenedIterator<CFRowsIt, DescendToColumn, CombineColumnIterators>;
  ColRowsIt colrows_begin(std::move(cfrows_begin), cfrows_end);
  ColRowsIt colrows_end(cfrows_end, cfrows_end);

  using CellRowsIt =
      FlattenedIterator<ColRowsIt, DescendToCell, CombineCellIterators>;
  CellRowsIt cellrows_begin(std::move(colrows_begin), colrows_end);
  CellRowsIt cellrows_end(colrows_end, colrows_end);
  std::cout << "Print start" << std::endl;

  return CellStream ([cellrows_begin, cellrows_end]() mutable
                    -> absl::optional<CellView> {
    if (cellrows_begin == cellrows_end) {
      return {};
    }
    return *cellrows_begin++;
  });
}

Status Table::ReadRows(google::bigtable::v2::ReadRowsRequest const& request,
                       RowStreamer& row_streamer) const {
  std::shared_ptr<SortedRowSet> row_set;
  if (request.has_rows()) {
    auto maybe_row_set = SortedRowSet::Create(request.rows());
    if (!maybe_row_set) {
      return maybe_row_set.status();
    }
    row_set = std::make_shared<SortedRowSet>(*std::move(maybe_row_set));
  } else {
    row_set = std::make_shared<SortedRowSet>(SortedRowSet::AllRows());
  }
  std::lock_guard lock(mu_);
  auto stream = ReadRowsInternal(std::move(row_set));
  if (request.has_filter()) {
    auto maybe_stream = CreateFilter(request.filter(), std::move(stream));
    if (!maybe_stream) {
      return maybe_stream.status();
    }
    stream = *maybe_stream;
  }
  for (; stream; ++stream) {
    std::cout << "Row: " << stream->row_key()
              << " column_family: " << stream->column_family()
              << " column_qualifier: " << stream->column_qualifier()
              << " column_timestamp: " << stream->timestamp().count()
              << " column_value: " << stream->value()
              << std::endl;
    if (!row_streamer.Stream(*stream)) {
      std::cout << "HOW?" << std::endl;
      return AbortedError("Stream closed by the client.", GCP_ERROR_INFO());
    }
  }
  if (!row_streamer.Flush(true)) {
    std::cout << "Flush failed?" << std::endl;
    return AbortedError("Stream closed by the client.", GCP_ERROR_INFO());
  }
  std::cout << "Print stop" << std::endl;
  return Status();
}

bool Table::IsDeleteProtected() const {
  std::lock_guard lock(mu_);
  return IsDeleteProtectedNoLock();
}

bool Table::IsDeleteProtectedNoLock() const {
  return schema_.deletion_protection();
}

}  // namespace emulator
}  // namespace bigtable
}  // namespace cloud
}  // namespace google
