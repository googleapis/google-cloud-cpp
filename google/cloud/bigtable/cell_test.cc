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

#include "google/cloud/bigtable/cell.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gtest/gtest.h>

namespace google {
namespace cloud {
namespace bigtable {
inline namespace BIGTABLE_CLIENT_NS {
namespace {

/// @test Verify Cell instantiation and trivial accessors.
TEST(CellTest, Simple) {
  std::string row_key = "row";
  std::string family_name = "family";
  std::string column_qualifier = "column";
  std::int64_t timestamp = 42;
  std::string value = "value";

  Cell cell(row_key, family_name, column_qualifier, timestamp, value);
  EXPECT_EQ(row_key, cell.row_key());
  EXPECT_EQ(family_name, cell.family_name());
  EXPECT_EQ(column_qualifier, cell.column_qualifier());
  EXPECT_EQ(timestamp, cell.timestamp().count());
  EXPECT_EQ(value, cell.value());
  EXPECT_EQ(0, cell.labels().size());
}

/// Test for checking numeric value in bigtable::Cell
TEST(CellTest, SimpleNumericValue) {
  std::string row_key = "row";
  std::string family_name = "family";
  std::string column_qualifier = "column";
  std::int64_t timestamp = 42;
  std::int64_t value = 343321020;
  Cell cell(row_key, family_name, column_qualifier, timestamp, value);
  EXPECT_EQ(row_key, cell.row_key());
  EXPECT_EQ(family_name, cell.family_name());
  EXPECT_EQ(column_qualifier, cell.column_qualifier());
  EXPECT_EQ(timestamp, cell.timestamp().count());
  EXPECT_EQ(0, cell.labels().size());
  auto decoded = cell.decode_big_endian_integer<std::int64_t>();
  EXPECT_STATUS_OK(decoded);
  EXPECT_EQ(value, *decoded);
}

/// Test for checking negative value in bigtable::Cell.
TEST(CellTest, SimpleNumericNegativeValue) {
  std::string row_key = "row";
  std::string family_name = "family";
  std::string column_qualifier = "column";
  std::int64_t timestamp = 42;
  std::int64_t value = -343321020;
  Cell cell(row_key, family_name, column_qualifier, timestamp, value);
  EXPECT_EQ(row_key, cell.row_key());
  EXPECT_EQ(family_name, cell.family_name());
  EXPECT_EQ(column_qualifier, cell.column_qualifier());
  EXPECT_EQ(timestamp, cell.timestamp().count());
  EXPECT_EQ(0, cell.labels().size());
  auto decoded = cell.decode_big_endian_integer<std::int64_t>();
  EXPECT_STATUS_OK(decoded);
  EXPECT_EQ(value, *decoded);
}

/// @test Verify Cell rvalue-ref accessors.
TEST(CellTest, RValueRefAccessors) {
  std::string row_key = "row";
  std::string family_name = "family";
  std::string column_qualifier = "column";
  std::int64_t timestamp = 42;
  std::string value = "value";

  Cell cell(row_key, family_name, column_qualifier, timestamp, value);

  static_assert(
      !std::is_lvalue_reference<decltype(Cell(cell).value())>::value,
      "Member function `value` is expected to return a value from an r-value "
      "reference to row.");

  auto moved_value = std::move(cell).value();
  EXPECT_EQ(value, moved_value);
}

}  // namespace
}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable
}  // namespace cloud
}  // namespace google
