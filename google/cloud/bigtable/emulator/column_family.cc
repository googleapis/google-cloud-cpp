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
#include "google/cloud/internal/big_endian.h"
#include <google/bigtable/v2/types.pb.h>
#include <absl/types/optional.h>
#include <array>
#include <chrono>
#include <cstdint>
#include <iterator>
#include <map>
#include <string>
#include <utility>

namespace google {
namespace cloud {
namespace bigtable {
namespace emulator {

// FIXME: Workaround our current incorrect ordering of
// timestamps. Remove when that is fixed and they are in decreasing
// order, at which point we can just pick the first element.
std::map<std::chrono::milliseconds, std::string>::iterator latest(
    std::map<std::chrono::milliseconds, std::string>& cells_not_empty) {
  assert(!cells_not_empty.empty());

  auto first_it = cells_not_empty.begin();
  auto last_it = std::prev(cells_not_empty.end());
  auto latest_it = first_it->first >= last_it->first ? first_it : last_it;

  return latest_it;
}

StatusOr<ReadModifyWriteCellResult> ColumnRow::ReadModifyWrite(
    std::int64_t inc_value) {
  auto system_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::system_clock::now().time_since_epoch());

  if (cells_.empty()) {
    std::string value = google::cloud::internal::EncodeBigEndian(inc_value);
    cells_[system_ms] = value;

    return ReadModifyWriteCellResult{system_ms, std::move(value),
                                     absl::nullopt};
  }

  // FIXME: Workaround our current incorrect ordering of
  // timestamps. Remove when that is fixed and they are in decreasing
  // order, at which point we can just pick the first element.
  auto latest_it = latest(cells_);

  auto maybe_old_value =
      google::cloud::internal::DecodeBigEndian<std::int64_t>(latest_it->second);
  if (!maybe_old_value) {
    return maybe_old_value.status();
  }

  auto value = google::cloud::internal::EncodeBigEndian(
      inc_value + maybe_old_value.value());

  if (latest_it->first < system_ms) {
    // We need to add a cell with the current system timestamp
    cells_[system_ms] = value;

    return ReadModifyWriteCellResult{system_ms, std::move(value),
                                     absl::nullopt};
  }

  // Latest timestamp is >= system time. Overwrite latest timestamp
  auto old_value = std::move(latest_it->second);
  latest_it->second = value;

  return ReadModifyWriteCellResult{latest_it->first, std::move(value),
                                   std::move(old_value)};
}

ReadModifyWriteCellResult ColumnRow::ReadModifyWrite(
    std::string const& append_value) {
  auto system_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::system_clock::now().time_since_epoch());
  if (cells_.empty()) {
    cells_[system_ms] = append_value;

    return ReadModifyWriteCellResult{system_ms, std::move(append_value),
                                     absl::nullopt};
  }

  // FIXME: Workaround our current incorrect ordering of
  // timestamps. Remove when that is fixed and they are in decreasing
  // order, at which point we can just pick the first element.
  auto latest_it = latest(cells_);

  auto value = latest_it->second + append_value;

  if (latest_it->first < system_ms) {
    // We need to add a cell with the current system timestamp
    cells_[system_ms] = value;

    return ReadModifyWriteCellResult{system_ms, std::move(value),
                                     absl::nullopt};
  }

  // Latest timestamp is >= system time. Overwrite latest timestamp
  auto old_value = std::move(latest_it->second);
  latest_it->second = value;

  return ReadModifyWriteCellResult{latest_it->first, value,
                                   std::move(old_value)};
}

absl::optional<std::string> ColumnRow::SetCell(
    std::chrono::milliseconds timestamp, std::string const& value) {
  absl::optional<std::string> ret = absl::nullopt;

  auto cell_it = cells_.find(timestamp);
  if (!(cell_it == cells_.end())) {
    ret = std::move(cell_it->second);
  }

  cells_[timestamp] = value;

  return ret;
}

StatusOr<absl::optional<std::string>> ColumnRow::UpdateCell(
    std::chrono::milliseconds timestamp, std::string& value,
    std::function<StatusOr<std::string>(std::string const&,
                                        std::string&&)> const& update_fn) {
  absl::optional<std::string> ret = absl::nullopt;

  auto cell_it = cells_.find(timestamp);
  if (!(cell_it == cells_.end())) {
    auto maybe_update_value = update_fn(cell_it->second, std::move(value));
    if (!maybe_update_value) {
      return maybe_update_value.status();
    }
    ret = std::move(cell_it->second);
    maybe_update_value.value().swap(cell_it->second);
    return ret;
  }

  cells_[timestamp] = value;

  return ret;
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
    deleted_cells.emplace_back(std::move(cell));
    cells_.erase(cell_it++);
  }
  return deleted_cells;
}

absl::optional<Cell> ColumnRow::DeleteTimeStamp(
    std::chrono::milliseconds timestamp) {
  absl::optional<Cell> ret = absl::nullopt;

  auto cell_it = cells_.find(timestamp);
  if (cell_it != cells_.end()) {
    Cell cell = {std::move(cell_it->first), std::move(cell_it->second)};
    ret.emplace(std::move(cell));
    cells_.erase(cell_it);
  }

  return ret;
}

absl::optional<std::string> ColumnFamilyRow::SetCell(
    std::string const& column_qualifier, std::chrono::milliseconds timestamp,
    std::string const& value) {
  return columns_[column_qualifier].SetCell(timestamp, value);
}

StatusOr<absl::optional<std::string>> ColumnFamilyRow::UpdateCell(
    std::string const& column_qualifier, std::chrono::milliseconds timestamp,
    std::string& value,
    std::function<StatusOr<std::string>(std::string const&,
                                        std::string&&)> const& update_fn) {
  return columns_[column_qualifier].UpdateCell(timestamp, value,
                                               std::move(update_fn));
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

absl::optional<Cell> ColumnFamilyRow::DeleteTimeStamp(
    std::string const& column_qualifier, std::chrono::milliseconds timestamp) {
  auto column_it = columns_.find(column_qualifier);
  if (column_it == columns_.end()) {
    return absl::nullopt;
  }

  auto ret = column_it->second.DeleteTimeStamp(timestamp);
  if (!column_it->second.HasCells()) {
    columns_.erase(column_it);
  }

  return ret;
}

absl::optional<std::string> ColumnFamily::SetCell(
    std::string const& row_key, std::string const& column_qualifier,
    std::chrono::milliseconds timestamp, std::string const& value) {
  return rows_[row_key].SetCell(column_qualifier, timestamp, value);
}

StatusOr<absl::optional<std::string>> ColumnFamily::UpdateCell(
    std::string const& row_key, std::string const& column_qualifier,
    std::chrono::milliseconds timestamp, std::string& value) {
  return rows_[row_key].UpdateCell(column_qualifier, timestamp, value,
                                   update_cell_);
}

std::map<std::string, std::vector<Cell>> ColumnFamily::DeleteRow(
    std::string const& row_key) {
  std::map<std::string, std::vector<Cell>> res;

  auto row_it = rows_.find(row_key);
  if (row_it == rows_.end()) {
    return {};
  }

  for (auto& column : row_it->second.columns_) {
    // Not setting start and end timestamps will select all cells for deletion
    ::google::bigtable::v2::TimestampRange time_range;
    auto deleted_cells = column.second.DeleteTimeRange(time_range);
    if (!deleted_cells.empty()) {
      res[column.first] = std::move(deleted_cells);
    }
  }

  rows_.erase(row_it);

  return res;
}

std::vector<Cell> ColumnFamily::DeleteColumn(
    std::string const& row_key, std::string const& column_qualifier,
    ::google::bigtable::v2::TimestampRange const& time_range) {
  auto row_it = rows_.find(row_key);

  return DeleteColumn(row_it, column_qualifier, time_range);
}

std::vector<Cell> ColumnFamily::DeleteColumn(
    std::map<std::string, ColumnFamilyRow>::iterator row_it,
    std::string const& column_qualifier,
    ::google::bigtable::v2::TimestampRange const& time_range) {
  if (row_it != rows_.end()) {
    auto erased_cells =
        row_it->second.DeleteColumn(column_qualifier, time_range);
    if (!row_it->second.HasColumns()) {
      rows_.erase(row_it);
    }
    return erased_cells;
  }
  return {};
}

absl::optional<Cell> ColumnFamily::DeleteTimeStamp(
    std::string const& row_key, std::string const& column_qualifier,
    std::chrono::milliseconds timestamp) {
  auto row_it = rows_.find(row_key);
  if (row_it == rows_.end()) {
    return absl::nullopt;
  }

  auto ret = row_it->second.DeleteTimeStamp(column_qualifier, timestamp);
  if (!row_it->second.HasColumns()) {
    rows_.erase(row_it);
  }

  return ret;
}

class FilteredColumnFamilyStream::FilterApply {
 public:
  explicit FilterApply(FilteredColumnFamilyStream& parent) : parent_(parent) {}

  bool operator()(ColumnRange const& column_range) {
    if (column_range.column_family == parent_.column_family_name_) {
      parent_.column_ranges_.Intersect(column_range.range);
    }
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
    return true;
  }

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

StatusOr<std::shared_ptr<ColumnFamily>>
ColumnFamily::ConstructAggregateColumnFamily(
    google::bigtable::admin::v2::Type value_type) {
  auto cf = std::make_shared<ColumnFamily>();

  if (value_type.has_aggregate_type()) {
    auto const& aggregate_type = value_type.aggregate_type();
    switch (aggregate_type.aggregator_case()) {
      case google::bigtable::admin::v2::Type::Aggregate::kSum:
        cf->update_cell_ = cf->SumUpdateCellBEInt64;
        break;
      case google::bigtable::admin::v2::Type::Aggregate::kMin:
        cf->update_cell_ = cf->MinUpdateCellBEInt64;
        break;
      case google::bigtable::admin::v2::Type::Aggregate::kMax:
        cf->update_cell_ = cf->MaxUpdateCellBEInt64;
        break;
      default:
        return InvalidArgumentError(
            "unsupported aggregation type",
            GCP_ERROR_INFO().WithMetadata(
                "aggregation case",
                absl::StrFormat("%d", aggregate_type.aggregator_case())));
    }

    cf->value_type_ = std::move(value_type);

    return cf;
  }

  return InvalidArgumentError(
      "no aggregate type set in the supplied value_type",
      GCP_ERROR_INFO().WithMetadata("supplied value type",
                                    value_type.DebugString()));
}
}  // namespace emulator
}  // namespace bigtable
}  // namespace cloud
}  // namespace google
