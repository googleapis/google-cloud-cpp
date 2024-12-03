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
#include "google/protobuf/util/field_mask_util.h"
#include "google/cloud/internal/make_status.h"

namespace google {
namespace cloud {
namespace bigtable {
namespace emulator {

namespace btadmin = ::google::bigtable::admin::v2;
namespace btproto = ::google::bigtable::v2;

void ColumnRow::SetCell(std::int64_t timestamp_micros, std::string const& value) {
  if (timestamp_micros == -1) {
    // Time since epoch expressed in microseconds but rounded to milliseconds.
    timestamp_micros = std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::system_clock::now().time_since_epoch())
                    .count() *
                1000LL;
  }
  cells_[timestamp_micros] = std::move(value);
}

std::size_t ColumnRow::DeleteTimeRange(
    ::google::bigtable::v2::TimestampRange const& time_range) {
  std::size_t num_erased = 0;
  for (auto cell_it = cells_.lower_bound(time_range.start_timestamp_micros());
       cell_it != cells_.end() &&
       (time_range.end_timestamp_micros() == 0 ||
        cell_it->first < time_range.end_timestamp_micros());) {
    cells_.erase(cell_it++);
    ++num_erased;
  }
  return num_erased;
}

void ColumnFamilyRow::SetCell(std::string const& column_qualifier,
                              std::int64_t timestamp_micros,
                              std::string const& value) {
  columns_[column_qualifier].SetCell(timestamp_micros, value);
}

std::size_t ColumnFamilyRow::DeleteColumn(
    std::string const& column_qualifier,
    ::google::bigtable::v2::TimestampRange const& time_range) {
  auto column_it = columns_.find(column_qualifier);
  if (column_it != columns_.end()) {
    return column_it->second.DeleteTimeRange(time_range);
  }
  if (!column_it->second.HasCells()) {
    columns_.erase(column_it);
  }
  return 0;
}

void ColumnFamily::SetCell(std::string const& row_key,
                           std::string const& column_qualifier,
                           std::int64_t timestamp_micros,
                           std::string const& value) {
  rows_[row_key].SetCell(column_qualifier, timestamp_micros, value);
}

bool ColumnFamily::DeleteRow(std::string const& row_key) {
  return rows_.erase(row_key) > 0;
}

std::size_t ColumnFamily::DeleteColumn(
    std::string const& row_key, std::string const& column_qualifier,
    ::google::bigtable::v2::TimestampRange const& time_range) {
  auto row_it = rows_.find(row_key);
  if (row_it != rows_.end()) {
    auto num_erased_cells =
        row_it->second.DeleteColumn(column_qualifier, time_range);
    if (!row_it->second.HasColumns()) {
      rows_.erase(row_it);
    }
    return num_erased_cells;
  }
  return 0;
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
          set_cell.timestamp_micros(), set_cell.value());
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
