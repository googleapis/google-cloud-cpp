// Copyright 2017 Google Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef GOOGLE_CLOUD_CPP_BIGTABLE_CLIENT_CELL_H_
#define GOOGLE_CLOUD_CPP_BIGTABLE_CLIENT_CELL_H_

#include "bigtable/client/version.h"

#include <vector>

namespace bigtable {
inline namespace BIGTABLE_CLIENT_NS {
/**
 * The in-memory representation of a Bigtable cell.
 *
 * Bigtable stores data in rows, indexes by row keys.  Each row may contain
 * multiple column families, each column family might contain multiple columns,
 * and each column has multiple cells indexed by timestamp.  Notice that the
 * storage is sparse, column families, columns, and timestamps might contain
 * zero cells.
 *
 * The Cell class owns all its data.
 */
class Cell {
 public:
  /// Create a Cell and fill it with data.
  Cell(std::string row_key, std::string family_name,
       std::string column_qualifier, int64_t timestamp, std::string value,
       std::vector<std::string> labels)
      : row_key_(std::move(row_key)),
        family_name_(std::move(family_name)),
        column_qualifier_(std::move(column_qualifier)),
        timestamp_(timestamp),
        value_(std::move(value)),
        labels_(std::move(labels)) {}

  /// Return the row key this cell belongs to. The returned value is not valid
  /// after this object is deleted.
  std::string const& row_key() const { return row_key_; }

  /// Return the family this cell belongs to. The returned value is not valid
  /// after this object is deleted.
  std::string const& family_name() const { return family_name_; }

  /// Return the column this cell belongs to. The returned value is not valid
  /// after this object is deleted.
  std::string const& column_qualifier() const { return column_qualifier_; }

  /// Return the timestamp of this cell.
  int64_t timestamp() const { return timestamp_; }

  /// Return the contents of this cell. The returned value is not valid after
  /// this object is deleted.
  std::string const& value() const { return value_; }

  /// Return the labels applied to this cell by label transformer read filters.
  std::vector<std::string> const& labels() const { return labels_; }

 private:
  std::string row_key_;
  std::string family_name_;
  std::string column_qualifier_;
  int64_t timestamp_;
  std::string value_;
  std::vector<std::string> labels_;
};

}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable

#endif  // GOOGLE_CLOUD_CPP_BIGTABLE_CLIENT_ROW_H_
