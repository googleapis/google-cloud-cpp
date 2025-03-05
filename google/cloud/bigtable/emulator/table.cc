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

#include "google/cloud/bigtable/emulator/table.h"
#include "google/cloud/bigtable/emulator/column_family.h"
#include "google/cloud/bigtable/emulator/filter.h"
#include "google/cloud/bigtable/emulator/filtered_map.h"
#include "google/cloud/bigtable/emulator/row_iterators.h"
#include "google/cloud/bigtable/internal/google_bytes_traits.h"
#include "google/cloud/internal/make_status.h"
#include "google/protobuf/util/field_mask_util.h"
#include <absl/strings/str_format.h>
#include <re2/re2.h>
#include <chrono>
#include <type_traits>

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
        GCP_ERROR_INFO().WithMetadata("schema", schema.DebugString()));
  }
  if (schema_.has_automated_backup_policy()) {
    return UnimplementedError(
        "`automated_backup_policy` not empty.",
        GCP_ERROR_INFO().WithMetadata("schema", schema.DebugString()));
  }
  for (auto const& column_family_def : schema_.column_families()) {
    column_families_.emplace(column_family_def.first,
                             std::make_shared<ColumnFamily>());
  }
  return Status();
}

StatusOr<btadmin::Table> Table::ModifyColumnFamilies(
    btadmin::ModifyColumnFamiliesRequest const& request) {
  std::cout << "Modify column families: " << request.DebugString() << std::endl;
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
        return NotFoundError("No such column family.",
                             GCP_ERROR_INFO().WithMetadata(
                                 "modification", modification.DebugString()));
      }
      if (new_schema.mutable_column_families()->erase(modification.id()) == 0) {
        return InternalError("Column family with no schema.",
                             GCP_ERROR_INFO().WithMetadata(
                                 "modification", modification.DebugString()));
      }
    } else if (modification.has_update()) {
      auto& cfs = *new_schema.mutable_column_families();
      auto cf_it = cfs.find(modification.id());
      if (cf_it == cfs.end()) {
        return NotFoundError("No such column family.",
                             GCP_ERROR_INFO().WithMetadata(
                                 "modification", modification.DebugString()));
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
        return InternalError("Column family with schema but no data.",
                             GCP_ERROR_INFO().WithMetadata(
                                 "modification", modification.DebugString()));
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
        GCP_ERROR_INFO().WithMetadata("mask", to_update.DebugString()));
  }
  google::protobuf::FieldMask disallowed_mask;
  FieldMaskUtil::Subtract<google::bigtable::admin::v2::Table>(
      to_update, allowed_mask, &disallowed_mask);
  if (disallowed_mask.paths_size() > 0) {
    return UnimplementedError(
        "Update mask contains disallowed fields.",
        GCP_ERROR_INFO().WithMetadata("mask", disallowed_mask.DebugString()));
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

Status Table::MutateRow(google::bigtable::v2::MutateRowRequest const& request) {
  // FIXME - add atomicity
  // FIXME - determine what happens when row/column family/column does not exist
  std::lock_guard lock(mu_);
  assert(request.table_name() == schema_.name());

  RowTransaction row_transaction(this->get(), request);

  for (auto mutation : request.mutations()) {
    if (mutation.has_set_cell()) {
      auto const& set_cell = mutation.set_cell();
      auto status = row_transaction.SetCell(set_cell);
      if (!status.ok()) {
        return status;
      }
    } else if (mutation.has_add_to_cell()) {
      // FIXME
    } else if (mutation.has_merge_to_cell()) {
      // FIXME
    } else if (mutation.has_delete_from_column()) {
      auto const& delete_from_column = mutation.delete_from_column();
      auto maybe_column_family = FindColumnFamily(delete_from_column);
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

  // If we get here, all mutations on the row have succeeded. We can
  // commit and return which will prevent the destructor from undoing
  // the transaction.
  row_transaction.commit();

  return Status();
}

class FilteredTableStream : public MergeCellStreams {
 public:
  FilteredTableStream(
      std::vector<std::shared_ptr<FilteredColumnFamilyStream>> cf_streams)
      : MergeCellStreams(CreateCellStreams(std::move(cf_streams))) {}

  bool ApplyFilter(InternalFilter const& internal_filter) override {
    if (!absl::holds_alternative<FamilyNameRegex>(internal_filter)) {
      return MergeCellStreams::ApplyFilter(internal_filter);
    }
    for (auto stream_it = unfinished_streams_.begin();
         stream_it != unfinished_streams_.end(); ++stream_it) {
      auto* cf_stream = dynamic_cast<FilteredColumnFamilyStream const*>(
          &(*stream_it)->impl());
      assert(cf_stream);
      if (re2::RE2::PartialMatch(
              cf_stream->column_family_name(),
              *absl::get<FamilyNameRegex>(internal_filter).regex)) {
        auto last_it = std::prev(unfinished_streams_.end());
        if (stream_it == last_it) {
          unfinished_streams_.pop_back();
          break;
        }
        stream_it->swap(unfinished_streams_.back());
        unfinished_streams_.pop_back();
      }
    }
    return true;
  }

 private:
  static std::vector<CellStream> CreateCellStreams(
      std::vector<std::shared_ptr<FilteredColumnFamilyStream>> cf_streams) {
    std::vector<CellStream> res;
    res.reserve(cf_streams.size());
    for (auto& stream : cf_streams) {
      res.emplace_back(std::move(stream));
    }
    return res;
  }
};

StatusOr<StringRangeSet> CreateStringRangeSet(
    google::bigtable::v2::RowSet const& row_set) {
  StringRangeSet res;
  for (auto const& row_key : row_set.row_keys()) {
    if (row_key.empty()) {
      return InvalidArgumentError(
          "`row_key` empty",
          GCP_ERROR_INFO().WithMetadata("row_set", row_set.DebugString()));
    }
    res.Sum(StringRangeSet::Range(row_key, false, row_key, false));
  }
  for (auto const& row_range : row_set.row_ranges()) {
    auto maybe_range = StringRangeSet::Range::FromRowRange(row_range);
    if (!maybe_range) {
      return maybe_range.status();
    }
    if (maybe_range->IsEmpty()) {
      continue;
    }
    res.Sum(*std::move(maybe_range));
  }
  return res;
}

Status Table::ReadRows(google::bigtable::v2::ReadRowsRequest const& request,
                       RowStreamer& row_streamer) const {
  std::shared_ptr<StringRangeSet> row_set;
  if (request.has_rows()) {
    auto maybe_row_set = CreateStringRangeSet(request.rows());
    if (!maybe_row_set) {
      return maybe_row_set.status();
    }
    row_set = std::make_shared<StringRangeSet>(*std::move(maybe_row_set));
  } else {
    row_set = std::make_shared<StringRangeSet>(StringRangeSet::All());
  }
  std::lock_guard lock(mu_);
  std::vector<std::shared_ptr<FilteredColumnFamilyStream>> per_cf_streams;
  for (auto const& column_family : column_families_) {
    per_cf_streams.emplace_back(std::make_shared<FilteredColumnFamilyStream>(
        *column_family.second, column_family.first, row_set));
  }
  auto stream = CellStream(
      std::make_shared<FilteredTableStream>(std::move(per_cf_streams)));
  FilterContext ctx;
  if (request.has_filter()) {
    auto maybe_stream = CreateFilter(request.filter(), std::move(stream), ctx);
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
              << " column_value: " << stream->value() << " label: "
              << (stream->HasLabel() ? stream->label() : std::string("unset"))
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

Status RowTransaction::AddToCell(
    ::google::bigtable::v2::Mutation_AddToCell const& add_to_cell) {
  return UnimplementedError(
      "Unsupported mutation type.",
      GCP_ERROR_INFO().WithMetadata("mutation", add_to_cell.DebugString()));
}

Status RowTransaction::MergeToCell(
    ::google::bigtable::v2::Mutation_MergeToCell const& merge_to_cell) {
  return UnimplementedError(
      "Unsupported mutation type.",
      GCP_ERROR_INFO().WithMetadata("mutation", merge_to_cell.DebugString()));
}

Status RowTransaction::DeleteFromFamily(
    ::google::bigtable::v2::Mutation_DeleteFromFamily const&
        delete_from_family) {
  auto maybe_column_family = table_->FindColumnFamily(delete_from_family);
  if (!maybe_column_family) {
    return maybe_column_family.status();
  }

  auto table_it = table_->find(delete_from_family.family_name());
  if (table_it == table_->end()) {
    return Status(StatusCode::kNotFound,
                  absl::StrFormat("column family %s not found in table",
                                  delete_from_family.family_name()),
                  ErrorInfo());
  }

  if (auto column_family_it = table_it->second->find(request_.row_key());
      column_family_it != table_it->second->end()) {
    RestoreRow restore_row;

    restore_row.table_it_ = table_it;
    restore_row.row_key_ = request_.row_key();
    std::vector<RestoreRow::Cell> cells;
    for (auto const& column : column_family_it->second) {
      for (auto const& column_row : column.second) {
        RestoreRow::Cell cell;

        cell.column_qualifer_ = std::move(column.first);
        cell.timestamp_ = column_row.first;  // Wait, is this correct?
        cell.value_ = std::move(column_row.second);
        cells.push_back(cell);
      }
    }
    restore_row.cells_ = cells;
    table_it->second->DeleteRow(request_.row_key());  // Is certain
                                                      // to succeed
                                                      // unless we
                                                      // run out of
                                                      // memory.
    undo_.emplace(restore_row);
  } else {
    // The row does not exist
    return Status(StatusCode::kNotFound,
                  absl::StrFormat("row key %s not found in column family %s",
                                  request_.row_key(), table_it->first),
                  ErrorInfo());
  }

  return Status();
}

Status RowTransaction::SetCell(
    ::google::bigtable::v2::Mutation_SetCell const& set_cell) {
  auto maybe_column_family = table_->FindColumnFamily(set_cell);
  if (!maybe_column_family) {
    return maybe_column_family.status();
  }

  auto& column_family = maybe_column_family->get();

  bool row_existed = true;
  bool column_existed = true;
  bool cell_existed = true;

  auto row_key_it = column_family.find(request_.row_key());
  std::string value_to_restore;
  if (row_key_it == column_family.end()) {
    row_existed = false;
    column_existed = false;
    cell_existed = false;
  } else {
    auto& column_family_row = row_key_it->second;
    auto column_row_it = column_family_row.find(set_cell.column_qualifier());
    if (column_row_it == column_family_row.end()) {
      column_existed = false;
      cell_existed = false;
    } else {
      auto timestamp_it = column_row_it->second.find(
          std::chrono::duration_cast<std::chrono::milliseconds>(
              std::chrono::microseconds(set_cell.timestamp_micros())));
      if (timestamp_it == column_row_it->second.end()) {
        cell_existed = false;
      } else {
        value_to_restore = std::move(timestamp_it->second);
      }
    }
  }

  column_family.SetCell(
      request_.row_key(), set_cell.column_qualifier(),
      std::chrono::duration_cast<std::chrono::milliseconds>(
          std::chrono::microseconds(set_cell.timestamp_micros())),
      set_cell.value());

  // If we have added a row, a column or a cell, we need to recompute
  // these iterators.
  row_key_it = column_family.find(request_.row_key());
  auto& column_family_row = row_key_it->second;
  auto column_row_it = column_family_row.find(set_cell.column_qualifier());
  auto timestamp_it = column_row_it->second.find(
      std::chrono::duration_cast<std::chrono::milliseconds>(
          std::chrono::microseconds(set_cell.timestamp_micros())));

  if (!row_existed) {
    DeleteRow delete_row = {row_key_it, column_family};
    undo_.emplace(delete_row);
  }

  if (!column_existed) {
    DeleteColumn delete_column_row = {column_row_it, column_family_row};
    undo_.emplace(delete_column_row);
  }

  if (!cell_existed) {
    DeleteValue delete_value = {column_row_it, timestamp_it->first};
    undo_.emplace(delete_value);
  } else {
    RestoreValue restore_value = {column_row_it, timestamp_it->first,
                                  std::move(value_to_restore)};
    undo_.emplace(restore_value);
  }

  return Status();
}

void RowTransaction::Undo() {
  while (!undo_.empty()) {
    auto op = undo_.top();
    undo_.pop();

    if (auto* restore_value = absl::get_if<RestoreValue>(&op)) {
      auto& column_row = restore_value->column_row_it_->second;
      column_row.find(restore_value->timestamp_)->second =
          std::move(restore_value->value_);
      continue;
    }

    if (auto* delete_value = absl::get_if<DeleteValue>(&op)) {
      auto& column_row = delete_value->column_row_it_->second;
      auto timestamp_it = column_row.find(delete_value->timestamp_);
      column_row.erase(timestamp_it);
      continue;
    }

    if (auto* delete_row = absl::get_if<DeleteRow>(&op)) {
      delete_row->column_family.erase(delete_row->row_it);
      continue;
    }

    if (auto* delete_column = absl::get_if<DeleteColumn>(&op)) {
      delete_column->column_family_row.erase(delete_column->column_row_it);
      continue;
    }

    if (auto* restore_row = absl::get_if<RestoreRow>(&op)) {
      for (auto const& cell : restore_row->cells_) {
        // Take care to use std::move() to avoid copying potentially
        // very larg values (the column qualifier and cell values can
        // be very large.
        restore_row->table_it_->second->SetCell(
            restore_row->row_key_, std::move(cell.column_qualifer_),
            cell.timestamp_, std::move(cell.value_));
      }
    }

    // If we get here, there is an type of undo log that has not been
    // implemented!
    std::abort();
  }
}

}  // namespace emulator
}  // namespace bigtable
}  // namespace cloud
}  // namespace google
