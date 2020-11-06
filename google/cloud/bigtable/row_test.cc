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

#include "google/cloud/bigtable/row.h"
#include <gtest/gtest.h>

namespace google {
namespace cloud {
namespace bigtable {
inline namespace BIGTABLE_CLIENT_NS {
namespace {

/// @test Verify Row instantiation and trivial accessors.
TEST(RowTest, RowInstantiation) {
  std::string row_key = "row";
  Cell cell(row_key, "family", "column", 42, "value");
  Row row(row_key, {cell});

  EXPECT_EQ(1, row.cells().size());
  EXPECT_EQ(row_key, row.cells().begin()->row_key());

  Row empty_row(row_key, {});
  EXPECT_EQ(0, empty_row.cells().size());
  EXPECT_EQ(empty_row.cells().begin(), empty_row.cells().end());

  Cell cell2(row_key, "family", "column", 43, "val");
  Row two_cells_row(row_key, {cell, cell2});
  EXPECT_EQ(2, two_cells_row.cells().size());
  EXPECT_EQ(std::next(two_cells_row.cells().begin())->value(), cell2.value());
}

TEST(RowTest, MoveOverload) {
  std::string row_key = "row";
  Cell cell(row_key, "family", "column", 42, "value");
  Row row(row_key, {cell});

  static_assert(
      !std::is_lvalue_reference<decltype(std::move(row).cells())>::value,
      "Member function `cells` is expected to return a value from an r-value "
      "reference to row.");

  std::vector<Cell> moved_cells = std::move(row).cells();
  EXPECT_EQ(1U, moved_cells.size());
  EXPECT_EQ("family", moved_cells[0].family_name());
  EXPECT_EQ("column", moved_cells[0].column_qualifier());
  EXPECT_EQ(42, moved_cells[0].timestamp().count());
  EXPECT_EQ("value", moved_cells[0].value());
}

}  // namespace
}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable
}  // namespace cloud
}  // namespace google
