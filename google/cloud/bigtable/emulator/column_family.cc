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

ColumnFamily::const_iterator::const_iterator(
    ColumnFamily const& column_family, std::shared_ptr<SortedRowSet> row_set)
    : column_family_(std::cref(column_family)), row_set_(std::move(row_set)) {
  if (row_set_) {
    std::cout << "ColumnFamily::const_iterator::const_iterator():" << std::endl;
    for (auto const& range : row_set_->disjoint_ranges()) {
      std::cout << "  ";
      if (range.has_start_key_closed()) {
        std::cout << "[" << range.start_key_closed();
      } else if (range.has_start_key_open()) {
        std::cout << "(" << range.start_key_open();
      } else {
        std::cout << "(inf";
      }
      std::cout << ":";
      if (range.has_end_key_closed()) {
        std::cout << range.end_key_closed() << "]";
      } else if (range.has_end_key_open()) {
        std::cout << range.end_key_open() << ")";
      } else {
        std::cout << "inf)";
      }
      std::cout << std::endl;
    }

    row_set_pos_ = row_set_->disjoint_ranges().begin();
    row_pos_ = column_family_.get().rows_.begin();

    AdvanceToNextRange();
    EnsureIteratorValid();
  } else {
    row_pos_ = column_family_.get().rows_.end();
  }
}

void ColumnFamily::const_iterator::AdvanceToNextRange() {
  if (row_set_pos_ == row_set_->disjoint_ranges().end()) {
    // We've reached the end.
    row_pos_ = column_family_.get().rows_.end();
    return;
  }
  if (row_pos_ == column_family_.get().rows_.end()) {
    // row_pos_ is already pointing far enough.
    return;
  }
  if (!internal::RowRangeHelpers::BelowStart(*row_set_pos_, row_pos_->first)) {
    // row_pos_ is already pointing far enough.
    return;
  }
  if (row_set_pos_->has_start_key_closed()) {
    row_pos_ = column_family_.get().rows_.lower_bound(
        row_set_pos_->start_key_closed());
  } else if (row_set_pos_->has_start_key_open()) {
    row_pos_ =
        column_family_.get().rows_.upper_bound(row_set_pos_->start_key_open());
  } else {
    // Range open on the left
    row_pos_ = column_family_.get().rows_.begin();
  }
}

void ColumnFamily::const_iterator::EnsureIteratorValid() {
  // `row_pos_` may point to a row which is past the end of the range pointed by
  // row_set_pos_. Make sure this only happens when the iteration reaches its
  // end.
  while (row_pos_ != column_family_.get().rows_.end() &&
         row_set_pos_ != row_set_->disjoint_ranges().end() &&
         internal::RowRangeHelpers::AboveEnd(*row_set_pos_, row_pos_->first)) {
    ++row_set_pos_;
    AdvanceToNextRange();
  }
  // This situation indicates that there are no rows which start after
  // current (as pointed by `row_set_pos_`) range's start. Given that we're
  // traversing `row_set_` in order, there will be no such rows for
  // following ranges, i.e. we've reached the end.
}

ColumnFamily::const_iterator& ColumnFamily::const_iterator::operator++() {
  std::cout << "ColumnFamily::const_iterator::operator++ this="
            << reinterpret_cast<void*>(this) << " val before: "
            << (row_pos_ == column_family_.get().rows_.end()
                    ? std::string("end")
                    : row_pos_->first)
            << std::endl;
  ++row_pos_;
  EnsureIteratorValid();
  return *this;
}

ColumnFamily::const_iterator ColumnFamily::const_iterator::operator++(int) {
  ColumnFamily::const_iterator retval = *this;
  ++(*this);
  return retval;
}

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
