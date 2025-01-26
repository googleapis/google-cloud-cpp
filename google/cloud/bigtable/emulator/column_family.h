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

#include <google/bigtable/admin/v2/table.pb.h>
#include <google/bigtable/v2/data.pb.h>
#include "google/cloud/bigtable/emulator/string_range_set.h"
#include <map>

namespace google {
namespace cloud {
namespace bigtable {
namespace emulator {


class ColumnRow {
 public:
  void SetCell(std::chrono::milliseconds timestamp, std::string const& value);
  std::size_t DeleteTimeRange(
      ::google::bigtable::v2::TimestampRange const& time_range);

  bool HasCells() const { return !cells_.empty(); }
  using const_iterator =
      std::map<std::chrono::milliseconds, std::string>::const_iterator;
  const_iterator begin() const { return cells_.begin(); }
  const_iterator end() const { return cells_.end(); }

 private:
  std::map<std::chrono::milliseconds, std::string> cells_;
};
  
class ColumnFamilyRow {
 public:
  void SetCell(std::string const& column_qualifier,
               std::chrono::milliseconds timestamp, std::string const& value);
  std::size_t DeleteColumn(
      std::string const& column_qualifier,
      ::google::bigtable::v2::TimestampRange const& time_range);
  bool HasColumns() { return !columns_.empty(); }
  using const_iterator = std::map<std::string, ColumnRow>::const_iterator;
  const_iterator begin() const { return columns_.begin(); }
  const_iterator end() const { return columns_.end(); }

 private:
  std::map<std::string, ColumnRow> columns_;
};

class ColumnFamily {
 public:
  using const_iterator = std::map<std::string, ColumnFamilyRow>::const_iterator;

  void SetCell(std::string const& row_key, std::string const& column_qualifier,
               std::chrono::milliseconds timestamp, std::string const& value);
  bool DeleteRow(std::string const& row_key);
  std::size_t DeleteColumn(
      std::string const& row_key, std::string const& column_qualifier,
      ::google::bigtable::v2::TimestampRange const& time_range);

  const_iterator begin() const {
    return rows_.begin();
  }
  const_iterator end() const { 
    return rows_.end();
  }
  const_iterator lower_bound(std::string const& row_key) const {
    return rows_.lower_bound(row_key);
  }
  const_iterator upper_bound(std::string const& row_key) const {
    return rows_.lower_bound(row_key);
  }

 private:
  std::map<std::string, ColumnFamilyRow> rows_;
};


}  // namespace emulator
}  // namespace bigtable
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_EMULATOR_COLUMN_FAMILY_H
