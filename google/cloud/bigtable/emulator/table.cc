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
#include "google/cloud/bigtable/emulator/range_set.h"
#include "google/cloud/bigtable/internal/google_bytes_traits.h"
#include "google/cloud/internal/make_status.h"
#include "google/protobuf/util/field_mask_util.h"
#include <google/bigtable/v2/bigtable.pb.h>
#include <google/bigtable/v2/data.pb.h>
#include <absl/strings/str_format.h>
#include <absl/types/optional.h>
#include <re2/re2.h>
#include <chrono>
#include <cstdlib>
#include <memory>
#include <mutex>
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

  return DoMutationsWithPossibleRollback(request.row_key(),
                                         request.mutations());
}

Status Table::DoMutationsWithPossibleRollback(
    std::string const& row_key,
    google::protobuf::RepeatedPtrField<google::bigtable::v2::Mutation> const&
        mutations) {
  RowTransaction row_transaction(this->get(), row_key);

  for (auto const& mutation : mutations) {
    if (mutation.has_set_cell()) {
      auto const& set_cell = mutation.set_cell();

      absl::optional<std::chrono::milliseconds> timestamp_override =
          absl::nullopt;

      auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
          std::chrono::microseconds(set_cell.timestamp_micros()));

      if (timestamp <= std::chrono::milliseconds::zero()) {
        timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch());
        timestamp_override.emplace(std::move(timestamp));
      }

      auto status = row_transaction.SetCell(set_cell, timestamp_override);
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

StatusOr<CellStream> Table::CreateCellStream(
    std::shared_ptr<StringRangeSet> range_set,
    absl::optional<google::bigtable::v2::RowFilter> maybe_row_filter) const {
  auto table_stream_ctor = [range_set = std::move(range_set), this] {
    std::vector<std::unique_ptr<FilteredColumnFamilyStream>> per_cf_streams;
    per_cf_streams.reserve(column_families_.size());
    for (auto const& column_family : column_families_) {
      per_cf_streams.emplace_back(std::make_unique<FilteredColumnFamilyStream>(
          *column_family.second, column_family.first, range_set));
    }
    return CellStream(
        std::make_unique<FilteredTableStream>(std::move(per_cf_streams)));
  };

  if (maybe_row_filter.has_value()) {
    return CreateFilter(maybe_row_filter.value(), table_stream_ctor);
  }

  return table_stream_ctor();
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

StatusOr<google::bigtable::v2::CheckAndMutateRowResponse>
Table::CheckAndMutateRow(
    google::bigtable::v2::CheckAndMutateRowRequest const& request) {
  std::lock_guard<std::mutex> lock(mu_);

  auto const& row_key = request.row_key();
  if (row_key.empty()) {
    return InvalidArgumentError(
        "row key required",
        GCP_ERROR_INFO().WithMetadata("CheckAndMutateRowRequest",
                                      request.DebugString()));
  }

  if (request.true_mutations_size() == 0 &&
      request.false_mutations_size() == 0) {
    return InvalidArgumentError(
        "both true mutations and false mutations are empty",
        GCP_ERROR_INFO().WithMetadata("CheckAndMutateRowRequest",
                                      request.DebugString()));
  }

  auto range_set = std::make_shared<StringRangeSet>();
  range_set->Sum(StringRangeSet::Range(row_key, false, row_key, false));

  StatusOr<CellStream> maybe_stream;
  if (request.has_predicate_filter()) {
    maybe_stream =
        CreateCellStream(range_set, std::move(request.predicate_filter()));
  } else {
    maybe_stream = CreateCellStream(range_set, absl::nullopt);
  }

  if (!maybe_stream) {
    return maybe_stream.status();
  }

  bool a_cell_is_found = false;

  CellStream& stream = *maybe_stream;
  if (stream) {  // At least one cell/value found when filter is applied
    a_cell_is_found = true;
  }

  Status status;
  if (a_cell_is_found) {
    status = DoMutationsWithPossibleRollback(request.row_key(),
                                             request.true_mutations());
  } else {
    status = DoMutationsWithPossibleRollback(request.row_key(),
                                             request.false_mutations());
  }

  if (!status.ok()) {
    return status;
  }

  google::bigtable::v2::CheckAndMutateRowResponse success_response;
  success_response.set_predicate_matched(a_cell_is_found);

  return success_response;
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

  StatusOr<CellStream> maybe_stream;
  if (request.has_filter()) {
    maybe_stream = CreateCellStream(row_set, std::move(request.filter()));
  } else {
    maybe_stream = CreateCellStream(row_set, absl::nullopt);
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
      row_key_, delete_from_column.column_qualifier(),
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
    auto deleted_columns = column_family.second->DeleteRow(row_key_);

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

  return NotFoundError("row not found in table",
                       GCP_ERROR_INFO().WithMetadata("row", row_key_));
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
  if (column_family_it->second->find(row_key_) ==
      column_family_it->second->end()) {
    // The row does not exist
    return NotFoundError(
        "row key is not found in column family",
        GCP_ERROR_INFO()
            .WithMetadata("row key", row_key_)
            .WithMetadata("column family", column_family_it->first));
  }

  auto deleted = column_family_it->second->DeleteRow(row_key_);
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

// timestamp_override, if provided, will be used instead of
// set_cell.timestamp. The override is used to set the timestamp to
// the server time in case a timestamp <= 0 is provided.
Status RowTransaction::SetCell(
    ::google::bigtable::v2::Mutation_SetCell const& set_cell,
    absl::optional<std::chrono::milliseconds> timestamp_override) {
  auto maybe_column_family = table_->FindColumnFamily(set_cell);
  if (!maybe_column_family) {
    return maybe_column_family.status();
  }

  auto& column_family = maybe_column_family->get();

  auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::microseconds(set_cell.timestamp_micros()));

  if (timestamp_override.has_value()) {
    timestamp = timestamp_override.value();
  }

  auto maybe_old_value = column_family.SetCell(
      row_key_, set_cell.column_qualifier(), timestamp, set_cell.value());

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
  auto row_key = row_key_;

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
