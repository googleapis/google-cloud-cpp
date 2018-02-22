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

#ifndef GOOGLE_CLOUD_CPP_BIGTABLE_CLIENT_ROW_H_
#define GOOGLE_CLOUD_CPP_BIGTABLE_CLIENT_ROW_H_

#include "bigtable/client/version.h"

#include "bigtable/client/cell.h"

#include <vector>

namespace bigtable {
inline namespace BIGTABLE_CLIENT_NS {
/**
 * The in-memory representation of a Bigtable row.
 *
 * Notice that a row returned by the Bigtable Client may have been filtered by
 * any filtering expressions provided by the application, and may not contain
 * all the data available.
 */
class Row {
 public:
  /// Create a row from a list of cells.
  Row(std::string row_key, std::vector<Cell> cells)
      : row_key_(std::move(row_key)), cells_(std::move(cells)) {}

  /// Return the row key. The returned value is not valid
  /// after this object is deleted.
  std::string const& row_key() const { return row_key_; }

  /// Return all cells.
  std::vector<Cell> const& cells() const { return cells_; }

 private:
  std::string row_key_;
  std::vector<Cell> cells_;
};

}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable

#endif  // GOOGLE_CLOUD_CPP_BIGTABLE_CLIENT_ROW_H_
