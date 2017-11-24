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

#include "bigtable/client/mutations.h"
#include <google/rpc/error_details.pb.h>

#include <gmock/gmock.h>

/// @test Verify that SetCell() works as expected.
TEST(MutationsTest, SetCell) {
  auto actual = bigtable::SetCell("family", "col", 1234, "value");
  ASSERT_TRUE(actual.op.has_set_cell());
  EXPECT_EQ(actual.op.set_cell().family_name(), "family");
  EXPECT_EQ(actual.op.set_cell().column_qualifier(), "col");
  EXPECT_EQ(actual.op.set_cell().timestamp_micros(), 1234);
  EXPECT_EQ(actual.op.set_cell().value(), "value");

  std::string fam("fam2"), col("col2");
  // ... we want to make sure the strings are efficiently moved.  The
  // C++ library often implements the "small string optimization",
  // where the memory allocation costs are traded off for extra
  // copies.  Use a large string to work around that optimization and
  // test the move behavior ...
  std::string val(1000000, 'a');
  auto val_data = val.data();
  auto moved = bigtable::SetCell(std::move(fam), std::move(col), 2345, std::move(val));
  ASSERT_TRUE(moved.op.has_set_cell());
  EXPECT_EQ(moved.op.set_cell().family_name(), "fam2");
  EXPECT_EQ(moved.op.set_cell().column_qualifier(), "col2");
  EXPECT_EQ(moved.op.set_cell().value().data(), val_data);
}

/// @test Verify that DeleteFromColumn() and friends work as expected.
TEST(MutationsTest, DeleteFromColumn) {
  // ... invalid ranges should fail ...
  EXPECT_THROW(bigtable::DeleteFromColumn("family", "col", 20, 0), std::range_error);
  EXPECT_THROW(bigtable::DeleteFromColumn("family", "col", 1000, 1000), std::range_error);
  
  auto actual = bigtable::DeleteFromColumn("family", "col", 1234, 1235);
  ASSERT_TRUE(actual.op.has_delete_from_column());
  EXPECT_EQ(actual.op.delete_from_column().family_name(), "family");
  EXPECT_EQ(actual.op.delete_from_column().column_qualifier(), "col");
  EXPECT_EQ(actual.op.delete_from_column().time_range().start_timestamp_micros(), 1234);
  EXPECT_EQ(actual.op.delete_from_column().time_range().end_timestamp_micros(), 1235);

  auto full = bigtable::DeleteFromColumn("family", "col");
  ASSERT_TRUE(full.op.has_delete_from_column());
  EXPECT_EQ(full.op.delete_from_column().family_name(), "family");
  EXPECT_EQ(full.op.delete_from_column().column_qualifier(), "col");
  EXPECT_EQ(full.op.delete_from_column().time_range().start_timestamp_micros(), 0);
  EXPECT_EQ(full.op.delete_from_column().time_range().end_timestamp_micros(), 0);
  
  auto end = bigtable::DeleteFromColumnEndingAt("family", "col", 1235);
  ASSERT_TRUE(end.op.has_delete_from_column());
  EXPECT_EQ(end.op.delete_from_column().family_name(), "family");
  EXPECT_EQ(end.op.delete_from_column().column_qualifier(), "col");
  EXPECT_EQ(end.op.delete_from_column().time_range().start_timestamp_micros(), 0);
  EXPECT_EQ(end.op.delete_from_column().time_range().end_timestamp_micros(), 1235);

  auto start = bigtable::DeleteFromColumnStartingFrom("family", "col", 1234);
  ASSERT_TRUE(start.op.has_delete_from_column());
  EXPECT_EQ(start.op.delete_from_column().family_name(), "family");
  EXPECT_EQ(start.op.delete_from_column().column_qualifier(), "col");
  EXPECT_EQ(start.op.delete_from_column().time_range().start_timestamp_micros(), 1234);
  EXPECT_EQ(start.op.delete_from_column().time_range().end_timestamp_micros(), 0);
}

/// @test Verify that DeleteFromFamily() and friends work as expected.
TEST(MutationsTest, DeleteFromFamily) {
  auto actual = bigtable::DeleteFromFamily("family");
  ASSERT_TRUE(actual.op.has_delete_from_family());
  EXPECT_EQ(actual.op.delete_from_family().family_name(), "family");
}

/// @test Verify that DeleteFromFamily() and friends work as expected.
TEST(MutationsTest, DeleteFromRow) {
  auto actual = bigtable::DeleteFromRow();
  ASSERT_TRUE(actual.op.has_delete_from_row());
}
