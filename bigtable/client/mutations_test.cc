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
  EXPECT_EQ("family", actual.op.set_cell().family_name());
  EXPECT_EQ("col", actual.op.set_cell().column_qualifier());
  EXPECT_EQ(1234, actual.op.set_cell().timestamp_micros());
  EXPECT_EQ("value", actual.op.set_cell().value());

  auto server_set = bigtable::SetCell("fam", "col", "v");
  ASSERT_TRUE(server_set.op.has_set_cell());
  EXPECT_EQ("fam", server_set.op.set_cell().family_name());
  EXPECT_EQ("col", server_set.op.set_cell().column_qualifier());
  EXPECT_EQ("v", server_set.op.set_cell().value());
  EXPECT_EQ(bigtable::ServerSetTimestamp(),
            server_set.op.set_cell().timestamp_micros());

  std::string fam("fam2"), col("col2");
  // ... we want to make sure the strings are efficiently moved.  The
  // C++ library often implements the "small string optimization",
  // where the memory allocation costs are traded off for extra
  // copies.  Use a large string to work around that optimization and
  // test the move behavior ...
  std::string val(1000000, 'a');
  auto val_data = val.data();
  auto moved =
      bigtable::SetCell(std::move(fam), std::move(col), 2345, std::move(val));
  ASSERT_TRUE(moved.op.has_set_cell());
  EXPECT_EQ("fam2", moved.op.set_cell().family_name());
  EXPECT_EQ("col2", moved.op.set_cell().column_qualifier());
  EXPECT_EQ(val_data, moved.op.set_cell().value().data());
}

/// @test Verify that DeleteFromColumn() and friends work as expected.
TEST(MutationsTest, DeleteFromColumn) {
  // ... invalid ranges should fail ...
  EXPECT_THROW(bigtable::DeleteFromColumn("family", "col", 20, 0),
               std::range_error);
  EXPECT_THROW(bigtable::DeleteFromColumn("family", "col", 1000, 1000),
               std::range_error);

  auto actual = bigtable::DeleteFromColumn("family", "col", 1234, 1235);
  ASSERT_TRUE(actual.op.has_delete_from_column());
  {
    auto const& mut = actual.op.delete_from_column();
    EXPECT_EQ("family", mut.family_name());
    EXPECT_EQ("col", mut.column_qualifier());
    EXPECT_EQ(1234, mut.time_range().start_timestamp_micros());
    EXPECT_EQ(1235, mut.time_range().end_timestamp_micros());
  }

  auto full = bigtable::DeleteFromColumn("family", "col");
  ASSERT_TRUE(full.op.has_delete_from_column());
  {
    auto const& mut = full.op.delete_from_column();
    EXPECT_EQ("family", mut.family_name());
    EXPECT_EQ("col", mut.column_qualifier());
    EXPECT_EQ(0, mut.time_range().start_timestamp_micros());
    EXPECT_EQ(0, mut.time_range().end_timestamp_micros());
  }
  auto end = bigtable::DeleteFromColumnEndingAt("family", "col", 1235);
  ASSERT_TRUE(end.op.has_delete_from_column());
  {
    auto const& mut = end.op.delete_from_column();
    EXPECT_EQ("family", mut.family_name());
    EXPECT_EQ("col", mut.column_qualifier());
    EXPECT_EQ(0, mut.time_range().start_timestamp_micros());
    EXPECT_EQ(1235, mut.time_range().end_timestamp_micros());
  }
  auto start = bigtable::DeleteFromColumnStartingFrom("family", "col", 1234);
  ASSERT_TRUE(start.op.has_delete_from_column());
  {
    auto const& mut = start.op.delete_from_column();
    EXPECT_EQ("family", mut.family_name());
    EXPECT_EQ("col", mut.column_qualifier());
    EXPECT_EQ(1234, mut.time_range().start_timestamp_micros());
    EXPECT_EQ(0, mut.time_range().end_timestamp_micros());
  }
}

/// @test Verify that DeleteFromFamily() and friends work as expected.
TEST(MutationsTest, DeleteFromFamily) {
  auto actual = bigtable::DeleteFromFamily("family");
  ASSERT_TRUE(actual.op.has_delete_from_family());
  EXPECT_EQ("family", actual.op.delete_from_family().family_name());
}

/// @test Verify that DeleteFromFamily() and friends work as expected.
TEST(MutationsTest, DeleteFromRow) {
  auto actual = bigtable::DeleteFromRow();
  ASSERT_TRUE(actual.op.has_delete_from_row());
}

// @test Verify that FailedMutation works as expected.
TEST(MutationsTest, FailedMutation) {
  bigtable::SingleRowMutation mut("foo",
                                  {bigtable::SetCell("f", "c", 0, "val")});

  // Create an overly complicated detail status, the idea is to make
  // sure it works in this case ...
  google::rpc::Status status;
  status.set_message("something failed");
  status.set_code(grpc::StatusCode::FAILED_PRECONDITION);
  google::rpc::RetryInfo retry;
  retry.mutable_retry_delay()->set_seconds(900);
  retry.mutable_retry_delay()->set_nanos(0);
  status.add_details()->PackFrom(retry);
  google::rpc::DebugInfo debug_info;
  debug_info.add_stack_entries("foo()");
  debug_info.add_stack_entries("bar()");
  debug_info.add_stack_entries("main()");
  debug_info.set_detail("just a test");
  status.add_details()->PackFrom(retry);
  status.add_details()->PackFrom(debug_info);

  bigtable::FailedMutation fm(std::move(mut), std::move(status));
  EXPECT_EQ(fm.status().error_code(), grpc::StatusCode::FAILED_PRECONDITION);
  EXPECT_EQ(fm.status().error_message(), "something failed");
  EXPECT_FALSE(fm.status().error_details().empty());
  EXPECT_EQ(fm.mutation().row_key(), "foo");
}

/// @test Verify that MultipleRowMutations works as expected.
TEST(MutationsTest, MutipleRowMutations) {
  bigtable::BulkMutation actual;

// ... prepare a non-empty request to verify MoveTo() does something ...
  google::bigtable::v2::MutateRowsRequest request;
  (void)request.add_entries();
  ASSERT_FALSE(request.entries().empty());

  actual.MoveTo(&request);
  ASSERT_TRUE(request.entries().empty());

  actual
      .emplace_back(bigtable::SingleRowMutation(
          "foo1", {bigtable::SetCell("f", "c", 0, "v1")}))
      .push_back(bigtable::SingleRowMutation(
          "foo2", {bigtable::SetCell("f", "c", 0, "v2")}));

  actual.MoveTo(&request);
  ASSERT_EQ(request.entries_size(), 2);
  EXPECT_EQ(request.entries(0).row_key(), "foo1");
  EXPECT_EQ(request.entries(1).row_key(), "foo2");

  std::vector<bigtable::SingleRowMutation> vec{
      bigtable::SingleRowMutation("foo1",
                                  {bigtable::SetCell("f", "c", 0, "v1")}),
      bigtable::SingleRowMutation("foo2",
                                  {bigtable::SetCell("f", "c", 0, "v2")}),
      bigtable::SingleRowMutation("foo3",
                                  {bigtable::SetCell("f", "c", 0, "v3")}),
  };
  bigtable::BulkMutation from_vec(vec.begin(), vec.end());

  from_vec.MoveTo(&request);
  ASSERT_EQ(request.entries_size(), 3);
  EXPECT_EQ(request.entries(0).row_key(), "foo1");
  EXPECT_EQ(request.entries(1).row_key(), "foo2");
  EXPECT_EQ(request.entries(2).row_key(), "foo3");

  bigtable::BulkMutation from_il{
      bigtable::SingleRowMutation("foo2",
                                  {bigtable::SetCell("f", "c", 0, "v2")}),
      bigtable::SingleRowMutation("foo3",
                                  {bigtable::SetCell("f", "c", 0, "v3")}),
  };
  from_il.MoveTo(&request);
  ASSERT_EQ(request.entries_size(), 2);
  EXPECT_EQ(request.entries(0).row_key(), "foo2");
  EXPECT_EQ(request.entries(1).row_key(), "foo3");
}
