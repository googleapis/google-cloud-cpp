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
#include <map>

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
  cells_[timestamp] = value;
}

std::vector<Cell> ColumnRow::DeleteTimeRange(
    ::google::bigtable::v2::TimestampRange const& time_range) {
  std::vector<Cell> deleted_cells;
  for (auto cell_it = cells_.lower_bound(
           std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::microseconds(time_range.start_timestamp_micros())));
       cell_it != cells_.end() &&
       (time_range.end_timestamp_micros() == 0 ||
        cell_it->first < std::chrono::duration_cast<std::chrono::milliseconds>(
                             std::chrono::microseconds(
                                 time_range.end_timestamp_micros())));) {
    Cell cell = {std::move(cell_it->first), std::move(cell_it->second)};
    deleted_cells.emplace_back(cell);
    cells_.erase(cell_it++);
  }
  return deleted_cells;
}

void ColumnFamilyRow::SetCell(std::string const& column_qualifier,
                              std::chrono::milliseconds timestamp,
                              std::string const& value) {
  columns_[column_qualifier].SetCell(timestamp, value);
}

std::vector<Cell> ColumnFamilyRow::DeleteColumn(
    std::string const& column_qualifier,
    ::google::bigtable::v2::TimestampRange const& time_range) {
  auto column_it = columns_.find(column_qualifier);
  if (column_it == columns_.end()) {
    return {};
  }
  auto res = column_it->second.DeleteTimeRange(time_range);
  if (!column_it->second.HasCells()) {
    columns_.erase(column_it);
  }
  return res;
}

void ColumnFamily::SetCell(std::string const& row_key,
                           std::string const& column_qualifier,
                           std::chrono::milliseconds timestamp,
                           std::string const& value) {
  rows_[row_key].SetCell(column_qualifier, timestamp, value);
}

std::map<std::string, std::vector<Cell>> ColumnFamily::DeleteRow(
    std::string const& row_key) {
  std::map<std::string, std::vector<Cell>> res;

  auto& column_family_row = rows_[row_key];

  for (auto column_it = column_family_row.begin();
       column_it != column_family_row.end();
       // Why we call column_family_row.begin() every iteration:
       // DeleteColumn can invalidate the iterator by deleting a column
       // family row's keys (the column qualifiers and their column
       // rows), therefore we need to re-calculate the beginning of the
       // map every loop. At the same time because we are removing all
       // cells of every column, we know DeleteColumn will eventually
       // remove all the columns and the row itself, so this loop will
       // terminate.
       column_it = column_family_row.begin()) {
    // Not setting start and end timestamps selects all cells for deletion.
    ::google::bigtable::v2::TimestampRange time_range;

    auto deleted_column = DeleteColumn(row_key, column_it->first, time_range);
    if (deleted_column.size() > 0) {
      res[std::move(column_it->first)] = std::move(deleted_column);
    }
  }

  return res;
}

std::vector<Cell> ColumnFamily::DeleteColumn(
    std::string const& row_key, std::string const& column_qualifier,
    ::google::bigtable::v2::TimestampRange const& time_range) {
  auto row_it = rows_.find(row_key);
  if (row_it != rows_.end()) {
    auto erased_cells =
        row_it->second.DeleteColumn(column_qualifier, time_range);
    if (!row_it->second.HasColumns()) {
      rows_.erase(row_it);
    }
    return erased_cells;
  }
  return std::vector<Cell>();
}

class FilteredColumnFamilyStream::FilterApply {
 public:
  explicit FilterApply(FilteredColumnFamilyStream& parent) : parent_(parent) {}

  bool operator()(ColumnRange const& column_range) {
    parent_.column_ranges_.Intersect(column_range.range);
    return true;
  }

  bool operator()(TimestampRange const& timestamp_range) {
    parent_.timestamp_ranges_.Intersect(timestamp_range.range);
    return true;
  }

  bool operator()(RowKeyRegex const& row_key_regex) {
    parent_.row_regexes_.emplace_back(row_key_regex.regex);
    return true;
  }

  bool operator()(FamilyNameRegex const&) { return false; }

  bool operator()(ColumnRegex const& column_regex) {
    parent_.column_regexes_.emplace_back(column_regex.regex);
    return true;  }

 private:
  FilteredColumnFamilyStream& parent_;
};

FilteredColumnFamilyStream::FilteredColumnFamilyStream(
    ColumnFamily const& column_family, std::string column_family_name,
    std::shared_ptr<StringRangeSet const> row_set)
    : column_family_name_(std::move(column_family_name)),
      row_ranges_(std::move(row_set)),
      column_ranges_(StringRangeSet::All()),
      timestamp_ranges_(TimestampRangeSet::All()),
      rows_(RangeFilteredMapView<ColumnFamily, StringRangeSet>(column_family,
                                                               *row_ranges_),
            std::cref(row_regexes_)) {}

bool FilteredColumnFamilyStream::ApplyFilter(
    InternalFilter const& internal_filter) {
  assert(!initialized_);
  return absl::visit(FilterApply(*this), internal_filter);
}

bool FilteredColumnFamilyStream::HasValue() const {
  InitializeIfNeeded();
  return *row_it_ != rows_.end();
}
CellView const& FilteredColumnFamilyStream::Value() const {
  InitializeIfNeeded();
  if (!cur_value_) {
    cur_value_ = CellView((*row_it_)->first, column_family_name_,
                          column_it_.value()->first, cell_it_.value()->first,
                          cell_it_.value()->second);
  }
  return cur_value_.value();
}

bool FilteredColumnFamilyStream::Next(NextMode mode) {
  InitializeIfNeeded();
  cur_value_.reset();
  assert(*row_it_ != rows_.end());
  assert(column_it_.value() != columns_.value().end());
  assert(cell_it_.value() != cells_.value().end());

  if (mode == NextMode::kCell) {
    ++(cell_it_.value());
    if (cell_it_.value() != cells_.value().end()) {
      return true;
    }
  }
  if (mode == NextMode::kCell || mode == NextMode::kColumn) {
    ++(column_it_.value());
    if (PointToFirstCellAfterColumnChange()) {
      return true;
    }
  }
  ++(*row_it_);
  PointToFirstCellAfterRowChange();
  return true;
}

void FilteredColumnFamilyStream::InitializeIfNeeded() const {
  if (!initialized_) {
    row_it_ = rows_.begin();
    PointToFirstCellAfterRowChange();
    initialized_ = true;
  }
}

bool FilteredColumnFamilyStream::PointToFirstCellAfterColumnChange() const {
  for (; column_it_.value() != columns_.value().end(); ++(column_it_.value())) {
    cells_ = RangeFilteredMapView<ColumnRow, TimestampRangeSet>(
        column_it_.value()->second, timestamp_ranges_);
    cell_it_ = cells_.value().begin();
    if (cell_it_.value() != cells_.value().end()) {
      return true;
    }
  }
  return false;
}

bool FilteredColumnFamilyStream::PointToFirstCellAfterRowChange() const {
  for (; (*row_it_) != rows_.end(); ++(*row_it_)) {
    columns_ = RegexFiteredMapView<
        RangeFilteredMapView<ColumnFamilyRow, StringRangeSet>>(
        RangeFilteredMapView<ColumnFamilyRow, StringRangeSet>(
            (*row_it_)->second, column_ranges_),
        column_regexes_);
    column_it_ = columns_.value().begin();
    if (PointToFirstCellAfterColumnChange()) {
      return true;
    }
  }
  return false;
}

}  // namespace emulator
}  // namespace bigtable
}  // namespace cloud
}  // namespace google
