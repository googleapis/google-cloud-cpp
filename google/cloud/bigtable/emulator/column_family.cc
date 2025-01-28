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
namespace {

StringRangeSet::Range CreateColumnRange(
    ::google::bigtable::v2::ColumnRange const& column_range) {
 StringRangeSet::Range::Value start; 
 bool start_open; 
 StringRangeSet::Range::Value end; 
 bool end_open;
 if (column_range.has_start_qualifier_closed()) {
   start = StringRangeSet::Range::Value(column_range.start_qualifier_closed());
   start_open = false;
 } else if (column_range.has_start_qualifier_open()) {
   start = StringRangeSet::Range::Value(column_range.start_qualifier_open());
   start_open = true;
 } else {
   start_open = false;
   start = StringRangeSet::Range::Value("");
 }
 if (column_range.has_end_qualifier_closed()) {
   end = StringRangeSet::Range::Value(column_range.end_qualifier_closed());
   end_open = false;
 } else if (column_range.has_end_qualifier_open()) {
   end = StringRangeSet::Range::Value(column_range.end_qualifier_open());
   end_open = true;
 } else {
   end = StringRangeSet::Range::Infinity{};
   end_open = true;
 }
 return StringRangeSet::Range(std::move(start), start_open, std::move(end),
                              end_open);
}
}  // anonymous namespace

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

class FilteredColumnFamilyStream::FilterApply {
 public:
  FilterApply(FilteredColumnFamilyStream& parent) : parent_(parent) {}

  bool operator()(google::bigtable::v2::ColumnRange const& column_range) {
    parent_.column_ranges_->Insert(CreateColumnRange(column_range));
    return true;
  }

  bool operator()(google::bigtable::v2::TimestampRange const& timestamp_range) {
    parent_.timestamp_ranges_->Insert(TimestampRangeSet::Range(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::microseconds(
                timestamp_range.start_timestamp_micros())),
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::microseconds(
                timestamp_range.end_timestamp_micros()))));
    return true;
  }

  bool operator()(RowKeyRegex const& row_key_regex) {
    parent_.row_regexes_.emplace_back(row_key_regex.regex);
    return true;
  }

  bool operator()(FamilyNameRegex const&) { return false; }

  bool operator()(ColumnRegex const& column_regex) {
    parent_.column_regexes_.emplace_back(column_regex.regex);
    return true;
  }

 private:
  FilteredColumnFamilyStream& parent_;
};

FilteredColumnFamilyStream::FilteredColumnFamilyStream(
    ColumnFamily const& column_family, std::string column_family_name,
    std::shared_ptr<StringRangeSet> row_set)
    : column_family_name_(std::move(column_family_name)),
      row_ranges_(std::move(row_set)),
      rows_(column_family, *row_ranges_),
      row_it_(rows_.begin()),
      initialized_(false) {}

absl::optional<CellView> FilteredColumnFamilyStream::Next() {
  InitializeIfNeeded();
  if (row_it_ == rows_.end()) {
    return {};
  }
  auto res =
      CellView(row_it_->first, column_family_name_, column_it_.value()->first,
               cell_it_.value()->first, cell_it_.value()->second);
  Advance();
  return res;
}

bool FilteredColumnFamilyStream::ApplyFilter(
    InternalFilter const& internal_filter) {
  return absl::visit(FilterApply(*this), internal_filter);
}

bool FilteredColumnFamilyStream::SkipColumn() {
  ++(column_it_.value());
  if (PointToFirstCellAfterColumnChange()) {
    return;
  }
  // no more cells in this row
  ++row_it_;
  PointToFirstCellAfterRowChange();
  return true;
}

bool FilteredColumnFamilyStream::SkipRow() {
  ++row_it_;
  PointToFirstCellAfterRowChange();
  return true;
}

void FilteredColumnFamilyStream::InitializeIfNeeded() {
  if (!initialized_) {
    PointToFirstCellAfterRowChange();
    initialized_ = true;
  }
}

void FilteredColumnFamilyStream::Advance() {
  assert(row_it_ != rows_.end());
  assert(column_it_.value() != columns_.value().end());
  assert(cell_it_.value() != cells_.value().end());
  ++(cell_it_.value());
  if (cell_it_.value() != cells_.value().end()) {
    return;
  }
  SkipColumn();
}

// Returns whether we've managed to find another cell in currently pointed row
bool FilteredColumnFamilyStream::PointToFirstCellAfterColumnChange() {
  for (; column_it_.value() != columns_.value().end(); ++(column_it_.value())) {
    cells_ = FilteredMapView<ColumnRow, TimestampRangeSet>(
        column_it_.value()->second, *timestamp_ranges_);
    cell_it_ = cells_.value().begin();
    if (cell_it_.value() != cells_.value().end()) {
      return true;
    }
  }
  return false;
}

// Returns whether we've managed to find another cell
bool FilteredColumnFamilyStream::PointToFirstCellAfterRowChange() {
  for (; row_it_ != rows_.end(); ++row_it_) {
    columns_ = FilteredMapView<ColumnFamilyRow, StringRangeSet>(
        row_it_->second, *column_ranges_);
    column_it_.value() = columns_.value().begin();
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
