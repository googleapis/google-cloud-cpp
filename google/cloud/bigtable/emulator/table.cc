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
#include "google/cloud/bigtable/internal/google_bytes_traits.h"
#include "google/cloud/internal/make_status.h"
#include "google/protobuf/util/field_mask_util.h"
#include <google/bigtable/v2/data.pb.h>
#include <absl/strings/str_format.h>
#include <re2/re2.h>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <memory>
#include <mutex>
#include <ratio>
#include <type_traits>

namespace google {
namespace cloud {
namespace bigtable {
namespace emulator {

namespace btadmin = ::google::bigtable::admin::v2;

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
  std::lock_guard<std::mutex> lock(mu_);
  schema_ = std::move(schema);
  if (schema_.granularity() ==
      btadmin::Table::TIMESTAMP_GRANULARITY_UNSPECIFIED) {
    schema_.set_granularity(btadmin::Table::MILLIS);
  }
  if (schema_.cluster_states_size() > 0) {
    return InvalidArgumentError(
        "`cluster_states` not empty.",
        GCP_ERROR_INFO().WithMetadata("schema", schema_.DebugString()));
  }
  if (schema_.has_restore_info()) {
    return InvalidArgumentError(
        "`restore_info` not empty.",
        GCP_ERROR_INFO().WithMetadata("schema", schema_.DebugString()));
  }
  if (schema_.has_change_stream_config()) {
    return UnimplementedError(
        "`change_stream_config` not empty.",
        GCP_ERROR_INFO().WithMetadata("schema", schema_.DebugString()));
  }
  if (schema_.has_automated_backup_policy()) {
    return UnimplementedError(
        "`automated_backup_policy` not empty.",
        GCP_ERROR_INFO().WithMetadata("schema", schema_.DebugString()));
  }
  for (auto const& column_family_def : schema_.column_families()) {
    column_families_.emplace(column_family_def.first,
                             std::make_shared<ColumnFamily>());
  }
  return Status();
}

// NOLINTBEGIN(readability-function-cognitive-complexity)
StatusOr<btadmin::Table> Table::ModifyColumnFamilies(
    btadmin::ModifyColumnFamiliesRequest const& request) {
  std::cout << "Modify column families: " << request.DebugString() << std::endl;
  std::unique_lock<std::mutex> lock(mu_);
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
  // Defer destroying potentially large objects to after releasing the lock.
  column_families_.swap(new_column_families);
  schema_ = new_schema;
  lock.unlock();
  return new_schema;
}
// NOLINTEND(readability-function-cognitive-complexity)

google::bigtable::admin::v2::Table Table::GetSchema() const {
  std::lock_guard<std::mutex> lock(mu_);
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
  std::lock_guard<std::mutex> lock(mu_);
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
  std::lock_guard<std::mutex> lock(mu_);
  assert(request.table_name() == schema_.name());

  RowTransaction row_transaction(this->get(), request);

  for (auto const& mutation : request.mutations()) {
    if (mutation.has_set_cell()) {
      auto const& set_cell = mutation.set_cell();
      auto status = row_transaction.SetCell(set_cell);
      if (!status.ok()) {
        return status;
      }
    } else if (mutation.has_add_to_cell()) {
      return UnimplementedError(
          "Unsupported mutation type.",
          GCP_ERROR_INFO().WithMetadata("mutation", mutation.DebugString()));
    } else if (mutation.has_merge_to_cell()) {
      return UnimplementedError(
          "Unsupported mutation type.",
          GCP_ERROR_INFO().WithMetadata("mutation", mutation.DebugString()));
    } else if (mutation.has_delete_from_column()) {
      auto const& delete_from_column = mutation.delete_from_column();
      auto status = row_transaction.DeleteFromColumn(delete_from_column);
      if (!status.ok()) {
        return status;
      }
    } else if (mutation.has_delete_from_family()) {
      auto const& delete_from_family = mutation.delete_from_family();
      auto status = row_transaction.DeleteFromFamily(delete_from_family);
      if (!status.ok()) {
        return status;
      }
    } else if (mutation.has_delete_from_row()) {
      auto status = row_transaction.DeleteFromRow();
      if (!status.ok()) {
        return status;
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

bool FilteredTableStream::ApplyFilter(InternalFilter const& internal_filter) {
  if (!absl::holds_alternative<FamilyNameRegex>(internal_filter)) {
    return MergeCellStreams::ApplyFilter(internal_filter);
  }
  for (auto stream_it = unfinished_streams_.begin();
       stream_it != unfinished_streams_.end(); ++stream_it) {
    auto const* cf_stream =
        dynamic_cast<FilteredColumnFamilyStream const*>(&(*stream_it)->impl());
    assert(cf_stream);
    if (!re2::RE2::PartialMatch(
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

std::vector<CellStream> FilteredTableStream::CreateCellStreams(
    std::vector<std::unique_ptr<FilteredColumnFamilyStream>> cf_streams) {
  std::vector<CellStream> res;
  res.reserve(cf_streams.size());
  for (auto& stream : cf_streams) {
    res.emplace_back(std::move(stream));
  }
  return res;
}

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
  std::lock_guard<std::mutex> lock(mu_);
  auto table_stream_ctor = [row_set = std::move(row_set), this] {
    std::vector<std::unique_ptr<FilteredColumnFamilyStream>> per_cf_streams;
    per_cf_streams.reserve(column_families_.size());
    for (auto const& column_family : column_families_) {
      per_cf_streams.emplace_back(std::make_unique<FilteredColumnFamilyStream>(
          *column_family.second, column_family.first, row_set));
    }
    return CellStream(
        std::make_unique<FilteredTableStream>(std::move(per_cf_streams)));
  };
  StatusOr<CellStream> maybe_stream;
  if (request.has_filter()) {
    maybe_stream = CreateFilter(request.filter(), table_stream_ctor);
  } else {
    maybe_stream = table_stream_ctor();
  }
  if (!maybe_stream) {
    return maybe_stream.status();
  }
  CellStream& stream = *maybe_stream;
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
  std::lock_guard<std::mutex> lock(mu_);
  return IsDeleteProtectedNoLock();
}

bool Table::IsDeleteProtectedNoLock() const {
  return schema_.deletion_protection();
}

// NOLINTBEGIN(readability-function-cognitive-complexity)
RowSampler Table::SampleRowKeys(
    google::bigtable::v2::SampleRowKeysRequest const&) {
  struct SamplingContext {
    // We pick the row key samples from just one column family (the
    // largest).
    std::string sample_src_cf;
    size_t row_index = 0;
    size_t max_num_rows = 0;
    std::map<std::string, ColumnFamilyRow>::const_iterator row_iterator;
    std::map<std::string, ColumnFamilyRow>::const_iterator row_end;
    bool table_is_empty = true;
    size_t offset_bytes = 0;
    std::once_flag once_flag;
  };

  std::shared_ptr<SamplingContext> sampling_context =
      std::make_shared<SamplingContext>();

  auto next_sample = [=]() mutable {
    // The first time the closure is called, initialize the row
    // iterators. The sampler works by advancing the iterator by
    // varying steps every time the closure it contains is called. We
    // can't initialize the iterators before the closure is first
    // called since we need to be holding the table lock first (in our
    // scheme it is grabbed in the constructor of the sampler).
    std::call_once(sampling_context->once_flag, [=]() {
      // Pick rows from just the largest column family since we are
      // just sampling. However offsets will be estimated based on the
      // size of the row across all column families.
      for (auto const& cf : column_families_) {
        if (cf.second->size() > sampling_context->max_num_rows) {
          sampling_context->table_is_empty = false;
          sampling_context->sample_src_cf = cf.first;
          sampling_context->row_iterator = cf.second->begin();
          sampling_context->row_end = cf.second->end();
          sampling_context->max_num_rows = cf.second->size();
        }
      }
    });

    // The signal that there are no more rows (an empty row key).
    if (sampling_context->table_is_empty ||
        sampling_context->row_iterator == sampling_context->row_end) {
      google::bigtable::v2::SampleRowKeysResponse resp;
      resp.set_row_key("");
      resp.set_offset_bytes(0);

      return resp;
    }

    for (auto& row = sampling_context->row_iterator;
         sampling_context->row_iterator != sampling_context->row_end;
         sampling_context->row_index++, sampling_context->row_iterator++) {
      auto add_this_row_size_to_offset = [=] {
        // First the offset due to the size of the row in the column
        // family we are sampling.
        sampling_context->offset_bytes +=
            (row->first.size() + row->second.size());

        // Then consider the size of the row data in other column families,
        // if they contain the row.
        for (auto const& cf : column_families_) {
          if (cf.first == sampling_context->sample_src_cf) {
            continue;
          }

          auto r = cf.second->find(row->first);
          if (r != cf.second->end()) {
            sampling_context->offset_bytes +=
                (row->first.size() + r->second.size());
          }
        }
      };

      // If there are any rows we need to return at least one
      // row. Always return the last one.
      if (sampling_context->row_index == sampling_context->max_num_rows - 1) {
        google::bigtable::v2::SampleRowKeysResponse resp;
        resp.set_row_key(row->first);
        resp.set_offset_bytes(sampling_context->offset_bytes);

        add_this_row_size_to_offset();

        return resp;
      }

      // Sample about one every 100 rows randomly.
      if (std::rand() % 100 == 0) {
        google::bigtable::v2::SampleRowKeysResponse resp;
        resp.set_row_key(row->first);
        resp.set_offset_bytes(sampling_context->offset_bytes);

        add_this_row_size_to_offset();

        return resp;
      }

      // This is a row we are not sampling, but we still need to
      // account for its size for accurate offsets of subsequent
      // sampled rows.
      add_this_row_size_to_offset();
    }

    google::bigtable::v2::SampleRowKeysResponse resp;
    resp.set_row_key("");
    resp.set_offset_bytes(sampling_context->offset_bytes);
    return resp;
  };

  // We acquire the table lock here (in the constructor), so that
  // every time we call row_sampler.Next() we always hold the lock,
  // and will continue to hold it until the destructor of RowSampler
  // is called.
  RowSampler row_sampler(this->get(), next_sample);

  return row_sampler;
}
// NOLINTEND(readability-function-cognitive-complexity)

// NOLINTBEGIN(readability-convert-member-functions-to-static)
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
// NOLINTEND(readability-convert-member-functions-to-static)

Status RowTransaction::DeleteFromColumn(
    ::google::bigtable::v2::Mutation_DeleteFromColumn const&
        delete_from_column) {
  auto maybe_column_family = table_->FindColumnFamily(delete_from_column);
  if (!maybe_column_family.ok()) {
    return maybe_column_family.status();
  }

  auto& column_family = maybe_column_family->get();

  auto deleted_cells = column_family.DeleteColumn(
      request_.row_key(), delete_from_column.column_qualifier(),
      delete_from_column.time_range());

  for (auto& cell : deleted_cells) {
    RestoreValue restore_value{
        column_family, delete_from_column.column_qualifier(),
        std::move(cell.timestamp), std::move(cell.value)};
    undo_.emplace(std::move(restore_value));
  }

  return Status();
}

Status RowTransaction::DeleteFromRow() {
  bool row_existed;
  for (auto& column_family : table_->column_families_) {
    auto deleted_columns = column_family.second->DeleteRow(request_.row_key());

    for (auto& column : deleted_columns) {
      for (auto& cell : column.second) {
        RestoreValue restrore_value = {*column_family.second,
                                       std::move(column.first), cell.timestamp,
                                       std::move(cell.value)};
        undo_.emplace(std::move(restrore_value));
        row_existed = true;
      }
    }
  }

  if (row_existed) {
    return Status();
  }

  return NotFoundError(
      "row not found in table",
      GCP_ERROR_INFO().WithMetadata("row", request_.row_key()));
}

Status RowTransaction::DeleteFromFamily(
    ::google::bigtable::v2::Mutation_DeleteFromFamily const&
        delete_from_family) {
  // If the request references an incorrect schema (non-existent
  // column family) then return a failure status error immediately.
  auto maybe_column_family = table_->FindColumnFamily(delete_from_family);
  if (!maybe_column_family.ok()) {
    return maybe_column_family.status();
  }

  auto column_family_it = table_->find(delete_from_family.family_name());
  if (column_family_it == table_->end()) {
    return NotFoundError(
        "column family not found in table",
        GCP_ERROR_INFO().WithMetadata("column family",
                                      delete_from_family.family_name()));
  }

  std::map<std::string, ColumnFamilyRow>::iterator column_family_row_it;
  if (column_family_it->second->find(request_.row_key()) ==
      column_family_it->second->end()) {
    // The row does not exist
    return NotFoundError(
        "row key is not found in column family",
        GCP_ERROR_INFO()
            .WithMetadata("row key", request_.row_key())
            .WithMetadata("column family", column_family_it->first));
  }

  auto deleted = column_family_it->second->DeleteRow(request_.row_key());
  for (auto const& column : deleted) {
    for (auto const& cell : column.second) {
      RestoreValue restore_value{*column_family_it->second,
                                 std::move(column.first), cell.timestamp,
                                 std::move(cell.value)};
      undo_.emplace(std::move(restore_value));
    }
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

  auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::microseconds(set_cell.timestamp_micros()));

  if (timestamp <= std::chrono::milliseconds::zero()) {
    timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch());
  }

  auto maybe_old_value =
      column_family.SetCell(request_.row_key(), set_cell.column_qualifier(),
                            timestamp, set_cell.value());

  if (!maybe_old_value) {
    DeleteValue delete_value{column_family,
                             std::move(set_cell.column_qualifier()), timestamp};
    undo_.emplace(std::move(delete_value));
  } else {
    RestoreValue restore_value{column_family,
                               std::move(set_cell.column_qualifier()),
                               timestamp, std::move(maybe_old_value.value())};
    undo_.emplace(std::move(restore_value));
  }

  return Status();
}

void RowTransaction::Undo() {
  auto row_key = request_.row_key();

  while (!undo_.empty()) {
    auto op = undo_.top();
    undo_.pop();

    auto* restore_value = absl::get_if<RestoreValue>(&op);
    if (restore_value) {
      restore_value->column_family.SetCell(
          row_key, std::move(restore_value->column_qualifier),
          restore_value->timestamp, std::move(restore_value->value));
      continue;
    }

    auto* delete_value = absl::get_if<DeleteValue>(&op);
    if (delete_value) {
      delete_value->column_family.DeleteTimeStamp(
          row_key, std::move(delete_value->column_qualifier),
          delete_value->timestamp);
      continue;
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
