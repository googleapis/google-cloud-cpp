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

#include "google/cloud/bigtable/table.h"
#include "google/cloud/bigtable/testing/table_test_fixture.h"
#include "google/cloud/testing_util/assert_ok.h"
#include "google/cloud/testing_util/chrono_literals.h"

namespace bigtable = google::cloud::bigtable;
using namespace google::cloud::testing_util::chrono_literals;

/// Define helper types and functions for this test.
namespace {
class TableApplyTest : public bigtable::testing::TableTestFixture {};
}  // anonymous namespace

/// @test Verify that Table::Apply() works in a simplest case.
TEST_F(TableApplyTest, Simple) {
  using namespace ::testing;

  EXPECT_CALL(*client_, MutateRow(_, _, _)).WillOnce(Return(grpc::Status::OK));

  auto status = table_.Apply(bigtable::SingleRowMutation(
      "bar", {bigtable::SetCell("fam", "col", 0_ms, "val")}));
  ASSERT_STATUS_OK(status);
}

/// @test Verify that Table::Apply() raises an exception on permanent failures.
TEST_F(TableApplyTest, Failure) {
  using namespace ::testing;

  EXPECT_CALL(*client_, MutateRow(_, _, _))
      .WillRepeatedly(
          Return(grpc::Status(grpc::StatusCode::FAILED_PRECONDITION, "uh-oh")));

  auto status = table_.Apply(bigtable::SingleRowMutation(
      "bar", {bigtable::SetCell("fam", "col", 0_ms, "val")}));
  EXPECT_FALSE(status.ok());
  EXPECT_EQ(google::cloud::StatusCode::kFailedPrecondition, status.code());
}

/// @test Verify that Table::Apply() retries on partial failures.
TEST_F(TableApplyTest, Retry) {
  using namespace ::testing;

  EXPECT_CALL(*client_, MutateRow(_, _, _))
      .WillOnce(
          Return(grpc::Status(grpc::StatusCode::UNAVAILABLE, "try-again")))
      .WillOnce(
          Return(grpc::Status(grpc::StatusCode::UNAVAILABLE, "try-again")))
      .WillOnce(
          Return(grpc::Status(grpc::StatusCode::UNAVAILABLE, "try-again")))
      .WillOnce(Return(grpc::Status::OK));

  auto status = table_.Apply(bigtable::SingleRowMutation(
      "bar", {bigtable::SetCell("fam", "col", 0_ms, "val")}));
  ASSERT_STATUS_OK(status);
}

/// @test Verify that Table::Apply() retries only idempotent mutations.
TEST_F(TableApplyTest, RetryIdempotent) {
  using namespace ::testing;

  EXPECT_CALL(*client_, MutateRow(_, _, _))
      .WillRepeatedly(
          Return(grpc::Status(grpc::StatusCode::UNAVAILABLE, "try-again")));

  auto status = table_.Apply(bigtable::SingleRowMutation(
      "not-idempotent", {bigtable::SetCell("fam", "col", "val")}));
  EXPECT_FALSE(status.ok());
  EXPECT_EQ(google::cloud::StatusCode::kUnavailable, status.code());
}
