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

#include "bigtable/client/table.h"
#include "bigtable/client/testing/chrono_literals.h"
#include "bigtable/client/testing/table_test_fixture.h"

/// Define helper types and functions for this test.
namespace {
class TableApplyTest : public bigtable::testing::TableTestFixture {};
}  // anonymous namespace

/// @test Verify that Table::Apply() works in a simplest case.

using namespace bigtable::chrono_literals;

TEST_F(TableApplyTest, Simple) {
  using namespace ::testing;

  EXPECT_CALL(*bigtable_stub_, MutateRow(_, _, _))
      .WillOnce(Return(grpc::Status::OK));

  table_.Apply(bigtable::SingleRowMutation(
      "bar", {bigtable::SetCell("fam", "col", 0_ms, "val")}));
}

/// @test Verify that Table::Apply() raises an exception on permanent failures.
TEST_F(TableApplyTest, Failure) {
  using namespace ::testing;

  EXPECT_CALL(*bigtable_stub_, MutateRow(_, _, _))
      .WillRepeatedly(
          Return(grpc::Status(grpc::StatusCode::FAILED_PRECONDITION, "uh-oh")));

#if GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
  EXPECT_THROW(table_.Apply(bigtable::SingleRowMutation(
                   "bar", {bigtable::SetCell("fam", "col", 0_ms, "val")})),
               std::exception);
#else
  EXPECT_DEATH_IF_SUPPORTED(
      table_.Apply(bigtable::SingleRowMutation(
          "bar", {bigtable::SetCell("fam", "col", 0_ms, "val")})),
      "exceptions are disabled");
#endif  // GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
}

/// @test Verify that Table::Apply() retries on partial failures.
TEST_F(TableApplyTest, Retry) {
  using namespace ::testing;

  EXPECT_CALL(*bigtable_stub_, MutateRow(_, _, _))
      .WillOnce(
          Return(grpc::Status(grpc::StatusCode::UNAVAILABLE, "try-again")))
      .WillOnce(
          Return(grpc::Status(grpc::StatusCode::UNAVAILABLE, "try-again")))
      .WillOnce(
          Return(grpc::Status(grpc::StatusCode::UNAVAILABLE, "try-again")))
      .WillOnce(Return(grpc::Status::OK));

  table_.Apply(bigtable::SingleRowMutation(
      "bar", {bigtable::SetCell("fam", "col", 0_ms, "val")}));
}

/// @test Verify that Table::Apply() retries only idempotent mutations.
TEST_F(TableApplyTest, RetryIdempotent) {
  using namespace ::testing;

  EXPECT_CALL(*bigtable_stub_, MutateRow(_, _, _))
      .WillRepeatedly(
          Return(grpc::Status(grpc::StatusCode::UNAVAILABLE, "try-again")));

#if GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
  // TODO(#234) - update this test when Apply() returns on error.
  try {
    table_.Apply(bigtable::SingleRowMutation(
        "not-idempotent", {bigtable::SetCell("fam", "col", "val")}));
  } catch (bigtable::PermanentMutationFailure const& ex) {
    ASSERT_EQ(1UL, ex.failures().size());
    EXPECT_EQ(0, ex.failures()[0].original_index());
  } catch (std::exception const& ex) {
    FAIL() << "unexpected std::exception raised: " << ex.what();
  } catch (...) {
    FAIL() << "unexpected exception of unknown type raised";
  }
#else
  EXPECT_DEATH_IF_SUPPORTED(
      table_.Apply(bigtable::SingleRowMutation(
          "not-idempotent", {bigtable::SetCell("fam", "col", "val")})),
      "exceptions are disabled");
#endif  // GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
}
