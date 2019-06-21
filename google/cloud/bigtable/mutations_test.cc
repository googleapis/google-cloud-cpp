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

#include "google/cloud/bigtable/mutations.h"
#include "google/cloud/internal/big_endian.h"
#include "google/cloud/testing_util/assert_ok.h"
#include "google/cloud/testing_util/chrono_literals.h"
#include <google/rpc/error_details.pb.h>
#include <gmock/gmock.h>

namespace bigtable = google::cloud::bigtable;
using namespace google::cloud::testing_util::chrono_literals;

/// @test Verify that SetCell() works as expected.
TEST(MutationsTest, SetCell) {
  auto actual = bigtable::SetCell("family", "col", 1234_ms, "value");
  ASSERT_TRUE(actual.op.has_set_cell());
  EXPECT_EQ("family", actual.op.set_cell().family_name());
  EXPECT_EQ("col", actual.op.set_cell().column_qualifier());
  EXPECT_EQ(1234000, actual.op.set_cell().timestamp_micros());
  EXPECT_EQ("value", actual.op.set_cell().value());

  auto server_set = bigtable::SetCell("fam", "col", "v");
  ASSERT_TRUE(server_set.op.has_set_cell());
  EXPECT_EQ("fam", server_set.op.set_cell().family_name());
  EXPECT_EQ("col", server_set.op.set_cell().column_qualifier());
  EXPECT_EQ("v", server_set.op.set_cell().value());
  EXPECT_EQ(bigtable::ServerSetTimestamp(),
            server_set.op.set_cell().timestamp_micros());
}

TEST(MutationsTest, SetCellNumericValue) {
  using google::cloud::internal::DecodeBigEndian;
  using google::cloud::internal::EncodeBigEndian;
  auto actual = bigtable::SetCell("family", "col", 1234_ms,
                                  EncodeBigEndian(std::int64_t{9876543210}));
  ASSERT_TRUE(actual.op.has_set_cell());
  EXPECT_EQ("family", actual.op.set_cell().family_name());
  EXPECT_EQ("col", actual.op.set_cell().column_qualifier());
  EXPECT_EQ(1234000, actual.op.set_cell().timestamp_micros());
  auto decoded = bigtable::internal::DecodeBigEndianCellValue<std::int64_t>(
      actual.op.set_cell().value());
  EXPECT_STATUS_OK(decoded);
  EXPECT_EQ(9876543210, *decoded);

  auto server_set =
      bigtable::SetCell("fam", "col", EncodeBigEndian(std::int64_t{32234401}));
  ASSERT_TRUE(server_set.op.has_set_cell());
  EXPECT_EQ("fam", server_set.op.set_cell().family_name());
  EXPECT_EQ("col", server_set.op.set_cell().column_qualifier());
  decoded = bigtable::internal::DecodeBigEndianCellValue<std::int64_t>(
      server_set.op.set_cell().value());
  EXPECT_STATUS_OK(decoded);
  EXPECT_EQ(32234401, *decoded);
  EXPECT_EQ(bigtable::ServerSetTimestamp(),
            server_set.op.set_cell().timestamp_micros());
}

/// @test Verify that DeleteFromColumn() does not validates inputs.
TEST(MutationsTest, DeleteFromColumnNoValidation) {
  auto reversed = bigtable::DeleteFromColumn("family", "col", 20_us, 0_us);
  EXPECT_TRUE(reversed.op.has_delete_from_column());
  auto empty = bigtable::DeleteFromColumn("family", "col", 1000_us, 1000_us);
  EXPECT_TRUE(empty.op.has_delete_from_column());
}

/// @test Verify that DeleteFromColumn() and friends work as expected.
TEST(MutationsTest, DeleteFromColumn) {
  auto actual = bigtable::DeleteFromColumn("family", "col", 1234_us, 1235_us);
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
  auto end = bigtable::DeleteFromColumnEndingAt("family", "col", 1235_us);
  ASSERT_TRUE(end.op.has_delete_from_column());
  {
    auto const& mut = end.op.delete_from_column();
    EXPECT_EQ("family", mut.family_name());
    EXPECT_EQ("col", mut.column_qualifier());
    EXPECT_EQ(0, mut.time_range().start_timestamp_micros());
    EXPECT_EQ(1235, mut.time_range().end_timestamp_micros());
  }
  auto start = bigtable::DeleteFromColumnStartingFrom("family", "col", 1234_us);
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
                                  {bigtable::SetCell("f", "c", 0_ms, "val")});

  // Create an overly complicated detail status, the idea is to make
  // sure it works in this case.
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

  bigtable::FailedMutation fm(std::move(status), 27);
  EXPECT_EQ(google::cloud::StatusCode::kFailedPrecondition, fm.status().code());
  EXPECT_EQ("something failed", fm.status().message());
  EXPECT_FALSE(fm.status().message().empty());
  EXPECT_EQ(27, fm.original_index());
}

/// @test Verify that MultipleRowMutations works as expected.
TEST(MutationsTest, MutipleRowMutations) {
  bigtable::BulkMutation actual;

  // Prepare a non-empty request to verify MoveTo() does something.
  google::bigtable::v2::MutateRowsRequest request;
  (void)request.add_entries();
  ASSERT_FALSE(request.entries().empty());

  actual.MoveTo(&request);
  ASSERT_TRUE(request.entries().empty());

  actual
      .emplace_back(bigtable::SingleRowMutation(
          "foo1", {bigtable::SetCell("f", "c", 0_ms, "v1")}))
      .push_back(bigtable::SingleRowMutation(
          "foo2", {bigtable::SetCell("f", "c", 0_ms, "v2")}));

  ASSERT_EQ(2, actual.size());
  actual.MoveTo(&request);
  ASSERT_EQ(2, request.entries_size());
  EXPECT_EQ("foo1", request.entries(0).row_key());
  EXPECT_EQ("foo2", request.entries(1).row_key());

  std::vector<bigtable::SingleRowMutation> vec{
      bigtable::SingleRowMutation("foo1",
                                  {bigtable::SetCell("f", "c", 0_ms, "v1")}),
      bigtable::SingleRowMutation("foo2",
                                  {bigtable::SetCell("f", "c", 0_ms, "v2")}),
      bigtable::SingleRowMutation("foo3",
                                  {bigtable::SetCell("f", "c", 0_ms, "v3")}),
  };
  bigtable::BulkMutation from_vec(vec.begin(), vec.end());

  ASSERT_EQ(3, from_vec.size());
  from_vec.MoveTo(&request);
  ASSERT_EQ(3, request.entries_size());
  EXPECT_EQ("foo1", request.entries(0).row_key());
  EXPECT_EQ("foo2", request.entries(1).row_key());
  EXPECT_EQ("foo3", request.entries(2).row_key());

  bigtable::BulkMutation from_il{
      bigtable::SingleRowMutation("foo2",
                                  {bigtable::SetCell("f", "c", 0_ms, "v2")}),
      bigtable::SingleRowMutation("foo3",
                                  {bigtable::SetCell("f", "c", 0_ms, "v3")}),
  };
  ASSERT_EQ(2, from_il.size());
  from_il.MoveTo(&request);
  ASSERT_EQ(2, request.entries_size());
  EXPECT_EQ("foo2", request.entries(0).row_key());
  EXPECT_EQ("foo3", request.entries(1).row_key());
}

/// @test Verify variadic Mutations for SingleRowMutations.
TEST(MutationsTest, SingleRowMutationMultipleVariadic) {
  std::string const row_key = "row-key-1";

  bigtable::SingleRowMutation actual(
      row_key, bigtable::SetCell("family", "c1", 1_ms, "V1000"),
      bigtable::SetCell("family", "c2", 2_ms, "V2000"));

  google::bigtable::v2::MutateRowsRequest::Entry entry;
  (void)entry.add_mutations();
  ASSERT_FALSE(entry.mutations().empty());

  actual.MoveTo(&entry);
  ASSERT_EQ(2, entry.mutations_size());
  EXPECT_EQ(row_key, entry.row_key());
}

/// @test Verify single variadic Mutation for SingleRowMutations.
TEST(MutationsTest, SingleRowMutationSingleVariadic) {
  std::string const row_key = "row-key-1";

  bigtable::SingleRowMutation actual(
      row_key, bigtable::SetCell("family", "c1", 1_ms, "V1000"));

  google::bigtable::v2::MutateRowsRequest::Entry entry;
  (void)entry.add_mutations();
  ASSERT_FALSE(entry.mutations().empty());

  actual.MoveTo(&entry);
  ASSERT_EQ(1, entry.mutations_size());
  EXPECT_EQ(row_key, entry.row_key());
}

/// @test Verify that SingleRowMutation::Clear() works.
TEST(MutationsTest, SingleRowMutationClear) {
  std::string const row_key = "row-key-1";

  bigtable::SingleRowMutation mut(
      row_key, bigtable::SetCell("family", "c1", 1_ms, "V1000"));

  mut.Clear();
  EXPECT_EQ("", mut.row_key());
  google::bigtable::v2::MutateRowsRequest::Entry entry;
  mut.MoveTo(&entry);
  EXPECT_EQ(0, entry.mutations_size());
}
