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
#include "google/cloud/testing_util/chrono_literals.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <google/rpc/error_details.pb.h>
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace bigtable {
inline namespace BIGTABLE_CLIENT_NS {
namespace {

namespace btproto = ::google::bigtable::v2;

using ::google::cloud::testing_util::chrono_literals::operator"" _ms;
using ::google::cloud::testing_util::chrono_literals::operator"" _us;

/// @test Verify that SetCell() works as expected.
TEST(MutationsTest, SetCell) {
  auto actual = SetCell("family", "col", 1234_ms, "value");
  ASSERT_TRUE(actual.op.has_set_cell());
  EXPECT_EQ("family", actual.op.set_cell().family_name());
  EXPECT_EQ("col", actual.op.set_cell().column_qualifier());
  EXPECT_EQ(1234000, actual.op.set_cell().timestamp_micros());
  EXPECT_EQ("value", actual.op.set_cell().value());

  auto server_set = SetCell("fam", "col", "v");
  ASSERT_TRUE(server_set.op.has_set_cell());
  EXPECT_EQ("fam", server_set.op.set_cell().family_name());
  EXPECT_EQ("col", server_set.op.set_cell().column_qualifier());
  EXPECT_EQ("v", server_set.op.set_cell().value());
  EXPECT_EQ(ServerSetTimestamp(), server_set.op.set_cell().timestamp_micros());
}

TEST(MutationsTest, SetCellNumericValue) {
  auto actual = SetCell("family", "col", 1234_ms, std::int64_t{9876543210});
  ASSERT_TRUE(actual.op.has_set_cell());
  EXPECT_EQ("family", actual.op.set_cell().family_name());
  EXPECT_EQ("col", actual.op.set_cell().column_qualifier());
  EXPECT_EQ(1234000, actual.op.set_cell().timestamp_micros());
  auto decoded = internal::DecodeBigEndianCellValue<std::int64_t>(
      actual.op.set_cell().value());
  EXPECT_STATUS_OK(decoded);
  EXPECT_EQ(9876543210, *decoded);

  auto server_set = SetCell("fam", "col", std::int64_t{32234401});
  ASSERT_TRUE(server_set.op.has_set_cell());
  EXPECT_EQ("fam", server_set.op.set_cell().family_name());
  EXPECT_EQ("col", server_set.op.set_cell().column_qualifier());
  decoded = internal::DecodeBigEndianCellValue<std::int64_t>(
      server_set.op.set_cell().value());
  EXPECT_STATUS_OK(decoded);
  EXPECT_EQ(32234401, *decoded);
  EXPECT_EQ(ServerSetTimestamp(), server_set.op.set_cell().timestamp_micros());
}

TEST(MutationsTest, SetCellFromBigtableCell) {
  Cell cell("some_row_key", "family", "col", 1234000, "value");
  auto actual = SetCell(cell);
  ASSERT_TRUE(actual.op.has_set_cell());
  EXPECT_EQ("family", actual.op.set_cell().family_name());
  EXPECT_EQ("col", actual.op.set_cell().column_qualifier());
  EXPECT_EQ(1234000, actual.op.set_cell().timestamp_micros());
  EXPECT_EQ("value", actual.op.set_cell().value());
}

TEST(MutationsTest, SetCellFromMovedBigtableCell) {
  auto actual =
      SetCell(Cell("some_row_key", "family", "col", 1234000, "value"));
  ASSERT_TRUE(actual.op.has_set_cell());
  EXPECT_EQ("family", actual.op.set_cell().family_name());
  EXPECT_EQ("col", actual.op.set_cell().column_qualifier());
  EXPECT_EQ(1234000, actual.op.set_cell().timestamp_micros());
  EXPECT_EQ("value", actual.op.set_cell().value());
}

/// @test Verify that DeleteFromColumn() does not validates inputs.
TEST(MutationsTest, DeleteFromColumnNoValidation) {
  auto reversed = DeleteFromColumn("family", "col", 20_us, 0_us);
  EXPECT_TRUE(reversed.op.has_delete_from_column());
  auto empty = DeleteFromColumn("family", "col", 1000_us, 1000_us);
  EXPECT_TRUE(empty.op.has_delete_from_column());
}

/// @test Verify that DeleteFromColumn() and friends work as expected.
TEST(MutationsTest, DeleteFromColumn) {
  auto actual = DeleteFromColumn("family", "col", 1234_us, 1235_us);
  ASSERT_TRUE(actual.op.has_delete_from_column());
  {
    auto const& mut = actual.op.delete_from_column();
    EXPECT_EQ("family", mut.family_name());
    EXPECT_EQ("col", mut.column_qualifier());
    EXPECT_EQ(1234, mut.time_range().start_timestamp_micros());
    EXPECT_EQ(1235, mut.time_range().end_timestamp_micros());
  }

  auto full = DeleteFromColumn("family", "col");
  ASSERT_TRUE(full.op.has_delete_from_column());
  {
    auto const& mut = full.op.delete_from_column();
    EXPECT_EQ("family", mut.family_name());
    EXPECT_EQ("col", mut.column_qualifier());
    EXPECT_EQ(0, mut.time_range().start_timestamp_micros());
    EXPECT_EQ(0, mut.time_range().end_timestamp_micros());
  }
  auto end = DeleteFromColumnEndingAt("family", "col", 1235_us);
  ASSERT_TRUE(end.op.has_delete_from_column());
  {
    auto const& mut = end.op.delete_from_column();
    EXPECT_EQ("family", mut.family_name());
    EXPECT_EQ("col", mut.column_qualifier());
    EXPECT_EQ(0, mut.time_range().start_timestamp_micros());
    EXPECT_EQ(1235, mut.time_range().end_timestamp_micros());
  }
  auto start = DeleteFromColumnStartingFrom("family", "col", 1234_us);
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
  auto actual = DeleteFromFamily("family");
  ASSERT_TRUE(actual.op.has_delete_from_family());
  EXPECT_EQ("family", actual.op.delete_from_family().family_name());
}

/// @test Verify that DeleteFromFamily() and friends work as expected.
TEST(MutationsTest, DeleteFromRow) {
  auto actual = DeleteFromRow();
  ASSERT_TRUE(actual.op.has_delete_from_row());
}

// @test Verify that FailedMutation works as expected.
TEST(MutationsTest, FailedMutation) {
  SingleRowMutation mut("foo", {SetCell("f", "c", 0_ms, "val")});

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

  FailedMutation fm(std::move(status), 27);
  EXPECT_EQ(google::cloud::StatusCode::kFailedPrecondition, fm.status().code());
  EXPECT_EQ("something failed", fm.status().message());
  EXPECT_FALSE(fm.status().message().empty());
  EXPECT_EQ(27, fm.original_index());
}

/// @test Verify that MultipleRowMutations works as expected.
TEST(MutationsTest, MutipleRowMutations) {
  BulkMutation actual;

  // Prepare a non-empty request to verify MoveTo() does something.
  btproto::MutateRowsRequest request;
  (void)request.add_entries();
  ASSERT_FALSE(request.entries().empty());

  actual.MoveTo(&request);
  ASSERT_TRUE(request.entries().empty());

  actual
      .emplace_back(SingleRowMutation("foo1", {SetCell("f", "c", 0_ms, "v1")}))
      .push_back(SingleRowMutation("foo2", {SetCell("f", "c", 0_ms, "v2")}));

  ASSERT_EQ(2, actual.size());
  actual.MoveTo(&request);
  ASSERT_EQ(2, request.entries_size());
  EXPECT_EQ("foo1", request.entries(0).row_key());
  EXPECT_EQ("foo2", request.entries(1).row_key());

  std::vector<SingleRowMutation> vec{
      SingleRowMutation("foo1", {SetCell("f", "c", 0_ms, "v1")}),
      SingleRowMutation("foo2", {SetCell("f", "c", 0_ms, "v2")}),
      SingleRowMutation("foo3", {SetCell("f", "c", 0_ms, "v3")}),
  };
  BulkMutation from_vec(vec.begin(), vec.end());

  ASSERT_EQ(3, from_vec.size());
  from_vec.MoveTo(&request);
  ASSERT_EQ(3, request.entries_size());
  EXPECT_EQ("foo1", request.entries(0).row_key());
  EXPECT_EQ("foo2", request.entries(1).row_key());
  EXPECT_EQ("foo3", request.entries(2).row_key());

  BulkMutation from_il{
      SingleRowMutation("foo2", {SetCell("f", "c", 0_ms, "v2")}),
      SingleRowMutation("foo3", {SetCell("f", "c", 0_ms, "v3")}),
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

  SingleRowMutation actual(row_key, SetCell("family", "c1", 1_ms, "V1000"),
                           SetCell("family", "c2", 2_ms, "V2000"));

  btproto::MutateRowsRequest::Entry entry;
  (void)entry.add_mutations();
  ASSERT_FALSE(entry.mutations().empty());

  actual.MoveTo(&entry);
  ASSERT_EQ(2, entry.mutations_size());
  EXPECT_EQ(row_key, entry.row_key());
}

/// @test Verify single variadic Mutation for SingleRowMutations.
TEST(MutationsTest, SingleRowMutationSingleVariadic) {
  std::string const row_key = "row-key-1";

  SingleRowMutation actual(row_key, SetCell("family", "c1", 1_ms, "V1000"));

  btproto::MutateRowsRequest::Entry entry;
  (void)entry.add_mutations();
  ASSERT_FALSE(entry.mutations().empty());

  actual.MoveTo(&entry);
  ASSERT_EQ(1, entry.mutations_size());
  EXPECT_EQ(row_key, entry.row_key());
}

/// @test Verify that SingleRowMutation::Clear() works.
TEST(MutationsTest, SingleRowMutationClear) {
  std::string const row_key = "row-key-1";

  SingleRowMutation mut(row_key, SetCell("family", "c1", 1_ms, "V1000"));

  mut.Clear();
  EXPECT_EQ("", mut.row_key());
  btproto::MutateRowsRequest::Entry entry;
  mut.MoveTo(&entry);
  EXPECT_EQ(0, entry.mutations_size());
}

}  // namespace
}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable
}  // namespace cloud
}  // namespace google
