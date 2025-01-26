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

#include "google/cloud/bigtable/emulator/column_family.h"
#include <chrono>

namespace google {
namespace cloud {
namespace bigtable {
namespace emulator {

void ColumnRow::SetCell(std::chrono::milliseconds timestamp,
                        std::string const& value) {
  if (timestamp <= std::chrono::milliseconds::zero()) {
    timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch());
  }
  cells_[timestamp] = std::move(value);
}

std::size_t ColumnRow::DeleteTimeRange(
    ::google::bigtable::v2::TimestampRange const& time_range) {
  std::size_t num_erased = 0;
  for (auto cell_it = cells_.lower_bound(
           std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::microseconds(time_range.start_timestamp_micros())));
       cell_it != cells_.end() &&
       (time_range.end_timestamp_micros() == 0 ||
        cell_it->first < std::chrono::duration_cast<std::chrono::milliseconds>(
                             std::chrono::microseconds(
                                 time_range.end_timestamp_micros())));) {
    cells_.erase(cell_it++);
    ++num_erased;
  }
  return num_erased;
}

void ColumnFamilyRow::SetCell(std::string const& column_qualifier,
                              std::chrono::milliseconds timestamp,
                              std::string const& value) {
  columns_[column_qualifier].SetCell(timestamp, value);
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
                           std::chrono::milliseconds timestamp,
                           std::string const& value) {
  rows_[row_key].SetCell(column_qualifier, timestamp, value);
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



}  // namespace emulator
}  // namespace bigtable
}  // namespace cloud
}  // namespace google
