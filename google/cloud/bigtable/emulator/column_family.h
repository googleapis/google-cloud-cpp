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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_EMULATOR_COLUMN_FAMILY_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_EMULATOR_COLUMN_FAMILY_H

#include "google/cloud/bigtable/cell.h"
#include "google/cloud/bigtable/emulator/cell_view.h"
#include "google/cloud/bigtable/emulator/filter.h"
#include "google/cloud/bigtable/emulator/filtered_map.h"
#include "google/cloud/bigtable/emulator/range_set.h"
#include "absl/types/optional.h"
#include <google/bigtable/admin/v2/table.pb.h>
#include <google/bigtable/v2/data.pb.h>
#include <chrono>
#include <map>

namespace google {
namespace cloud {
namespace bigtable {
namespace emulator {

struct Cell {
  std::chrono::milliseconds timestamp;
  std::string value;
};

class ColumnRow {
 public:
  ColumnRow() = default;
  // Disable copying.
  ColumnRow(ColumnRow const &) = delete;
  ColumnRow& operator=(ColumnRow const &) = delete;

  void SetCell(std::chrono::milliseconds timestamp, std::string const& value);
  std::vector<Cell> DeleteTimeRange(
      ::google::bigtable::v2::TimestampRange const& time_range);

  bool HasCells() const { return !cells_.empty(); }
  using const_iterator =
      std::map<std::chrono::milliseconds, std::string>::const_iterator;
  const_iterator begin() const { return cells_.begin(); }
  const_iterator end() const { return cells_.end(); }
  const_iterator lower_bound(std::chrono::milliseconds timestamp) const {
    return cells_.lower_bound(timestamp);
  }
  const_iterator upper_bound(std::chrono::milliseconds timestamp) const {
    return cells_.upper_bound(timestamp);
  }

  std::map<std::chrono::milliseconds, std::string>::iterator find(
      std::chrono::milliseconds const& timestamp) {
    return cells_.find(timestamp);
  }

  void erase(
      std::map<std::chrono::milliseconds, std::string>::iterator timestamp_it) {
    cells_.erase(timestamp_it);
  }

 private:
  std::map<std::chrono::milliseconds, std::string> cells_;
};

class ColumnFamilyRow {
 public:
  ColumnFamilyRow() = default;
  // Disable copying.
  ColumnFamilyRow(ColumnFamilyRow const &) = delete;
  ColumnFamilyRow& operator=(ColumnFamilyRow const &) = delete;

  void SetCell(std::string const& column_qualifier,
               std::chrono::milliseconds timestamp, std::string const& value);
  std::vector<Cell> DeleteColumn(
      std::string const& column_qualifier,
      ::google::bigtable::v2::TimestampRange const& time_range);
  bool HasColumns() { return !columns_.empty(); }
  using const_iterator = std::map<std::string, ColumnRow>::const_iterator;
  const_iterator begin() const { return columns_.begin(); }
  const_iterator end() const { return columns_.end(); }
  const_iterator lower_bound(std::string const& column_qualifier) const {
    return columns_.lower_bound(column_qualifier);
  }
  const_iterator upper_bound(std::string const& column_qualifier) const {
    return columns_.upper_bound(column_qualifier);
  }

  std::map<std::string, ColumnRow>::iterator find(
      std::string const& column_qualifier) {
    return columns_.find(column_qualifier);
  }

  void erase(std::map<std::string, ColumnRow>::iterator column_it) {
    columns_.erase(column_it);
  }

 private:
  std::map<std::string, ColumnRow> columns_;
};

class ColumnFamily {
 public:
  ColumnFamily() = default;
  // Disable copying.
  ColumnFamily(ColumnFamily const &) = delete;
  ColumnFamily& operator=(ColumnFamily const &) = delete;


  using const_iterator = std::map<std::string, ColumnFamilyRow>::const_iterator;

  void SetCell(std::string const& row_key, std::string const& column_qualifier,
               std::chrono::milliseconds timestamp, std::string const& value);
  std::map<std::string, std::vector<Cell>> DeleteRow(std::string const& row_key);
  std::vector<Cell> DeleteColumn(
      std::string const& row_key, std::string const& column_qualifier,
      ::google::bigtable::v2::TimestampRange const& time_range);

  const_iterator begin() const { return rows_.begin(); }
  const_iterator end() const { return rows_.end(); }
  const_iterator lower_bound(std::string const& row_key) const {
    return rows_.lower_bound(row_key);
  }
  const_iterator upper_bound(std::string const& row_key) const {
    return rows_.upper_bound(row_key);
  }

  std::map<std::string, ColumnFamilyRow>::iterator find(
      std::string const& row_key) {
    return rows_.find(row_key);
  }

  void erase(std::map<std::string, ColumnFamilyRow>::iterator row_it) {
    rows_.erase(row_it);
  }

 private:
  std::map<std::string, ColumnFamilyRow> rows_;
};

class FilteredColumnFamilyStream : public AbstractCellStreamImpl {
 public:
  FilteredColumnFamilyStream(ColumnFamily const& column_family,
                             std::string column_family_name,
                             std::shared_ptr<StringRangeSet> row_set);
  bool ApplyFilter(InternalFilter const& internal_filter) override;
  bool HasValue() const override;
  CellView const& Value() const override;
  bool Next(NextMode mode) override;
  std::string const& column_family_name() const { return column_family_name_; }

 private:
  class FilterApply;

  void InitializeIfNeeded() const;
  // Returns whether we've managed to find another cell in currently pointed row
  bool PointToFirstCellAfterColumnChange() const;
  // Returns whether we've managed to find another cell
  bool PointToFirstCellAfterRowChange() const;

  std::string column_family_name_;

  std::shared_ptr<StringRangeSet const> row_ranges_;
  std::vector<std::shared_ptr<re2::RE2 const>> row_regexes_;
  mutable StringRangeSet column_ranges_;
  std::vector<std::shared_ptr<re2::RE2 const>> column_regexes_;
  mutable TimestampRangeSet timestamp_ranges_;

  RegexFiteredMapView<RangeFilteredMapView<ColumnFamily, StringRangeSet>> rows_;
  mutable absl::optional<RegexFiteredMapView<
      RangeFilteredMapView<ColumnFamilyRow, StringRangeSet>>>
      columns_;
  mutable absl::optional<RangeFilteredMapView<ColumnRow, TimestampRangeSet>>
      cells_;

  // If row_it_ == rows_.end() we've reached the end.
  // We keep the invariant that if (row_it_ != rows_.end()) then
  // cell_it_ != cells.end() && column_it_ != columns_.end()
  mutable absl::optional<RegexFiteredMapView<
      RangeFilteredMapView<ColumnFamily, StringRangeSet>>::const_iterator>
      row_it_;
  mutable absl::optional<RegexFiteredMapView<
      RangeFilteredMapView<ColumnFamilyRow, StringRangeSet>>::const_iterator>
      column_it_;
  mutable absl::optional<
      RangeFilteredMapView<ColumnRow, TimestampRangeSet>::const_iterator>
      cell_it_;
  mutable absl::optional<CellView> cur_value_;
  mutable bool initialized_;
};

}  // namespace emulator
}  // namespace bigtable
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_EMULATOR_COLUMN_FAMILY_H
