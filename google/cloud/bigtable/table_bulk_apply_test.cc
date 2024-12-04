// Copyright 2017 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "google/cloud/bigtable/table.h"
#include "google/cloud/bigtable/testing/mock_mutate_rows_reader.h"
#include "google/cloud/bigtable/testing/mock_policies.h"
#include "google/cloud/bigtable/testing/table_test_fixture.h"
#include "google/cloud/testing_util/chrono_literals.h"
#include "google/cloud/testing_util/status_matchers.h"

namespace google {
namespace cloud {
namespace bigtable {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

namespace btproto = ::google::bigtable::v2;

using ::google::cloud::bigtable::testing::MockBackoffPolicy;
using ::google::cloud::testing_util::IsOk;
using ::google::cloud::testing_util::chrono_literals::operator""_ms;
using ::google::cloud::testing_util::chrono_literals::operator""_us;
using ::testing::An;
using ::testing::Not;
using ::testing::Return;

/// Define types and functions used in the tests.
class TableBulkApplyTest
    : public ::google::cloud::bigtable::testing::TableTestFixture {
 public:
  TableBulkApplyTest() : TableTestFixture(CompletionQueue{}) {}
};
using ::google::cloud::bigtable::testing::MockMutateRowsReader;

TEST_F(TableBulkApplyTest, Empty) {
  auto failures = table_.BulkApply(BulkMutation());
  EXPECT_TRUE(failures.empty());
}

/// @test Verify that Table::BulkApply() works in the easy case.
TEST_F(TableBulkApplyTest, Simple) {
  auto reader = std::make_unique<MockMutateRowsReader>(
      "google.bigtable.v2.Bigtable.MutateRows");
  EXPECT_CALL(*reader, Read)
      .WillOnce([](btproto::MutateRowsResponse* r) {
        {
          auto& e = *r->add_entries();
          e.set_index(0);
          e.mutable_status()->set_code(grpc::StatusCode::OK);
        }
        {
          auto& e = *r->add_entries();
          e.set_index(1);
          e.mutable_status()->set_code(grpc::StatusCode::OK);
        }
        return true;
      })
      .WillOnce(Return(false));
  EXPECT_CALL(*reader, Finish()).WillOnce(Return(grpc::Status::OK));

  EXPECT_CALL(*client_, MutateRows)
      .WillOnce(reader.release()->MakeMockReturner());

  auto failures = table_.BulkApply(BulkMutation(
      SingleRowMutation("foo", {SetCell("fam", "col", 0_ms, "baz")}),
      SingleRowMutation("bar", {SetCell("fam", "col", 0_ms, "qux")})));
  ASSERT_TRUE(failures.empty());
  SUCCEED();
}

/// @test Verify that Table::BulkApply() retries partial failures.
TEST_F(TableBulkApplyTest, RetryPartialFailure) {
  auto r1 = std::make_unique<MockMutateRowsReader>(
      "google.bigtable.v2.Bigtable.MutateRows");
  EXPECT_CALL(*r1, Read)
      .WillOnce([](btproto::MutateRowsResponse* r) {
        // Simulate a partial (recoverable) failure.
        auto& e0 = *r->add_entries();
        e0.set_index(0);
        e0.mutable_status()->set_code(grpc::StatusCode::UNAVAILABLE);
        auto& e1 = *r->add_entries();
        e1.set_index(1);
        e1.mutable_status()->set_code(grpc::StatusCode::OK);
        return true;
      })
      .WillOnce(Return(false));
  EXPECT_CALL(*r1, Finish()).WillOnce(Return(grpc::Status::OK));

  auto r2 = std::make_unique<MockMutateRowsReader>(
      "google.bigtable.v2.Bigtable.MutateRows");
  EXPECT_CALL(*r2, Read)
      .WillOnce([](btproto::MutateRowsResponse* r) {
        {
          auto& e = *r->add_entries();
          e.set_index(0);
          e.mutable_status()->set_code(grpc::StatusCode::OK);
        }
        return true;
      })
      .WillOnce(Return(false));
  EXPECT_CALL(*r2, Finish()).WillOnce(Return(grpc::Status::OK));

  EXPECT_CALL(*client_, MutateRows)
      .WillOnce(r1.release()->MakeMockReturner())
      .WillOnce(r2.release()->MakeMockReturner());

  auto failures = table_.BulkApply(BulkMutation(
      SingleRowMutation("foo", {SetCell("fam", "col", 0_ms, "baz")}),
      SingleRowMutation("bar", {SetCell("fam", "col", 0_ms, "qux")})));
  ASSERT_TRUE(failures.empty());
  SUCCEED();
}

/// @test Verify that Table::BulkApply() handles permanent failures.
TEST_F(TableBulkApplyTest, PermanentFailure) {
  auto r1 = std::make_unique<MockMutateRowsReader>(
      "google.bigtable.v2.Bigtable.MutateRows");
  EXPECT_CALL(*r1, Read)
      .WillOnce([](btproto::MutateRowsResponse* r) {
        {
          auto& e = *r->add_entries();
          e.set_index(0);
          e.mutable_status()->set_code(grpc::StatusCode::OK);
        }
        {
          auto& e = *r->add_entries();
          e.set_index(1);
          e.mutable_status()->set_code(grpc::StatusCode::OUT_OF_RANGE);
        }
        return true;
      })
      .WillOnce(Return(false));
  EXPECT_CALL(*r1, Finish()).WillOnce(Return(grpc::Status::OK));

  EXPECT_CALL(*client_, MutateRows).WillOnce(r1.release()->MakeMockReturner());

  auto failures = table_.BulkApply(BulkMutation(
      SingleRowMutation("foo", {SetCell("fam", "col", 0_ms, "baz")}),
      SingleRowMutation("bar", {SetCell("fam", "col", 0_ms, "qux")})));
  EXPECT_FALSE(failures.empty());
}

/// @test Verify that Table::BulkApply() handles a terminated stream.
TEST_F(TableBulkApplyTest, CanceledStream) {
  // Simulate a stream that returns one success and then terminates.  We expect
  // the BulkApply() operation to retry the request, because the mutation is in
  // an undetermined state.  Well, it should retry assuming it is idempotent,
  // which happens to be the case in this test.
  auto r1 = std::make_unique<MockMutateRowsReader>(
      "google.bigtable.v2.Bigtable.MutateRows");
  EXPECT_CALL(*r1, Read)
      .WillOnce([](btproto::MutateRowsResponse* r) {
        {
          auto& e = *r->add_entries();
          e.set_index(0);
          e.mutable_status()->set_code(grpc::StatusCode::OK);
        }
        return true;
      })
      .WillOnce(Return(false));
  EXPECT_CALL(*r1, Finish()).WillOnce(Return(grpc::Status::OK));

  // Create a second stream returned by the mocks when the client retries.
  auto r2 = std::make_unique<MockMutateRowsReader>(
      "google.bigtable.v2.Bigtable.MutateRows");
  EXPECT_CALL(*r2, Read)
      .WillOnce([](btproto::MutateRowsResponse* r) {
        {
          auto& e = *r->add_entries();
          e.set_index(0);
          e.mutable_status()->set_code(grpc::StatusCode::OK);
        }
        return true;
      })
      .WillOnce(Return(false));
  EXPECT_CALL(*r2, Finish()).WillOnce(Return(grpc::Status::OK));

  EXPECT_CALL(*client_, MutateRows)
      .WillOnce(r1.release()->MakeMockReturner())
      .WillOnce(r2.release()->MakeMockReturner());

  auto failures = table_.BulkApply(BulkMutation(
      SingleRowMutation("foo", {SetCell("fam", "col", 0_ms, "baz")}),
      SingleRowMutation("bar", {SetCell("fam", "col", 0_ms, "qux")})));
  ASSERT_TRUE(failures.empty());
  SUCCEED();
}

/// @test Verify that Table::BulkApply() reports correctly on too many errors.
TEST_F(TableBulkApplyTest, TooManyFailures) {
  // Create a table with specific policies so we can test the behavior
  // without having to depend on timers expiring.  In this case tolerate only
  // 3 failures.
  Table custom_table(
      client_, "foo_table",
      // Configure the Table to stop at 3 failures.
      LimitedErrorCountRetryPolicy(2),
      // Use much shorter backoff than the default to test faster.
      ExponentialBackoffPolicy(10_us, 40_us));

  // Setup the mocks to fail more than 3 times.
  auto r1 = std::make_unique<MockMutateRowsReader>(
      "google.bigtable.v2.Bigtable.MutateRows");
  EXPECT_CALL(*r1, Read)
      .WillOnce([](btproto::MutateRowsResponse* r) {
        {
          auto& e = *r->add_entries();
          e.set_index(0);
          e.mutable_status()->set_code(grpc::StatusCode::OK);
        }
        return true;
      })
      .WillOnce(Return(false));
  EXPECT_CALL(*r1, Finish())
      .WillOnce(Return(grpc::Status(grpc::StatusCode::ABORTED, "")));

  auto create_cancelled_stream = [&](grpc::ClientContext*,
                                     btproto::MutateRowsRequest const&) {
    auto stream = std::make_unique<MockMutateRowsReader>(
        "google.bigtable.v2.Bigtable.MutateRows");
    EXPECT_CALL(*stream, Read).WillOnce(Return(false));
    EXPECT_CALL(*stream, Finish())
        .WillOnce(Return(grpc::Status(grpc::StatusCode::ABORTED, "")));
    return stream;
  };

  EXPECT_CALL(*client_, MutateRows)
      .WillOnce(r1.release()->MakeMockReturner())
      .WillOnce(create_cancelled_stream)
      .WillOnce(create_cancelled_stream);

  auto failures = custom_table.BulkApply(BulkMutation(
      SingleRowMutation("foo", {SetCell("fam", "col", 0_ms, "baz")}),
      SingleRowMutation("bar", {SetCell("fam", "col", 0_ms, "qux")})));
  EXPECT_FALSE(failures.empty());
  EXPECT_EQ(google::cloud::StatusCode::kAborted,
            failures.front().status().code());
}

TEST_F(TableBulkApplyTest, RetryPolicyUsedForOkStreamWithFailedMutations) {
  // Create a table with specific policies so we can test the behavior
  // without having to depend on timers expiring.  In this case tolerate only
  // 3 failures.
  Table custom_table(
      client_, "foo_table",
      // Configure the Table to stop at 3 failures.
      LimitedErrorCountRetryPolicy(2),
      // Use much shorter backoff than the default to test faster.
      ExponentialBackoffPolicy(10_us, 40_us));

  auto create_stream = [&](grpc::ClientContext*,
                           btproto::MutateRowsRequest const&) {
    auto stream = std::make_unique<MockMutateRowsReader>(
        "google.bigtable.v2.Bigtable.MutateRows");
    EXPECT_CALL(*stream, Read)
        .WillOnce([](btproto::MutateRowsResponse* r) {
          auto& e0 = *r->add_entries();
          e0.set_index(0);
          e0.mutable_status()->set_code(grpc::StatusCode::UNAVAILABLE);
          return true;
        })
        .WillOnce(Return(false));
    EXPECT_CALL(*stream, Finish()).WillOnce(Return(grpc::Status::OK));
    return stream;
  };

  EXPECT_CALL(*client_, MutateRows).Times(3).WillRepeatedly(create_stream);

  auto failures = custom_table.BulkApply(BulkMutation(
      SingleRowMutation("bar", {SetCell("fam", "col", 0_ms, "qux")})));
  EXPECT_FALSE(failures.empty());
  EXPECT_EQ(StatusCode::kUnavailable, failures.front().status().code());
}

/// @test Verify that Table::BulkApply() retries only idempotent mutations.
TEST_F(TableBulkApplyTest, RetryOnlyIdempotent) {
  // We will send both idempotent and non-idempotent mutations.  We prepare the
  // mocks to return an empty stream in the first RPC request.  That will force
  // the client to only retry the idempotent mutations.
  auto r1 = std::make_unique<MockMutateRowsReader>(
      "google.bigtable.v2.Bigtable.MutateRows");
  EXPECT_CALL(*r1, Read).WillOnce(Return(false));
  EXPECT_CALL(*r1, Finish()).WillOnce(Return(grpc::Status::OK));

  auto r2 = std::make_unique<MockMutateRowsReader>(
      "google.bigtable.v2.Bigtable.MutateRows");
  EXPECT_CALL(*r2, Read)
      .WillOnce([](btproto::MutateRowsResponse* r) {
        {
          auto& e = *r->add_entries();
          e.set_index(0);
          e.mutable_status()->set_code(grpc::StatusCode::OK);
        }
        return true;
      })
      .WillOnce(Return(false));
  EXPECT_CALL(*r2, Finish()).WillOnce(Return(grpc::Status::OK));

  EXPECT_CALL(*client_, MutateRows)
      .WillOnce(r1.release()->MakeMockReturner())
      .WillOnce(r2.release()->MakeMockReturner());

  auto failures = table_.BulkApply(BulkMutation(
      SingleRowMutation("is-idempotent", {SetCell("fam", "col", 0_ms, "qux")}),
      SingleRowMutation("not-idempotent", {SetCell("fam", "col", "baz")})));
  EXPECT_EQ(1UL, failures.size());
  EXPECT_EQ(1, failures.front().original_index());
  EXPECT_THAT(failures.front().status(), Not(IsOk()));
  EXPECT_FALSE(failures.empty());
}

/// @test Verify that Table::BulkApply() works when the RPC fails.
TEST_F(TableBulkApplyTest, FailedRPC) {
  auto reader = std::make_unique<MockMutateRowsReader>(
      "google.bigtable.v2.Bigtable.MutateRows");
  EXPECT_CALL(*reader, Read).WillOnce(Return(false));
  EXPECT_CALL(*reader, Finish())
      .WillOnce(Return(grpc::Status(grpc::StatusCode::FAILED_PRECONDITION,
                                    "no such table")));

  EXPECT_CALL(*client_, MutateRows)
      .WillOnce(reader.release()->MakeMockReturner());

  auto failures = table_.BulkApply(BulkMutation(
      SingleRowMutation("foo", {SetCell("fam", "col", 0_ms, "baz")}),
      SingleRowMutation("bar", {SetCell("fam", "col", 0_ms, "qux")})));
  EXPECT_EQ(2UL, failures.size());
  EXPECT_THAT(failures.front().status(), Not(IsOk()));
  EXPECT_FALSE(failures.empty());
  EXPECT_EQ(google::cloud::StatusCode::kFailedPrecondition,
            failures.front().status().code());
}

TEST_F(TableBulkApplyTest, NoSleepIfNoPendingMutations) {
  auto reader = std::make_unique<MockMutateRowsReader>(
      "google.bigtable.v2.Bigtable.MutateRows");
  EXPECT_CALL(*reader, Read)
      .WillOnce([](btproto::MutateRowsResponse* r) {
        {
          auto& e = *r->add_entries();
          e.set_index(0);
          e.mutable_status()->set_code(grpc::StatusCode::OK);
        }
        {
          auto& e = *r->add_entries();
          e.set_index(1);
          e.mutable_status()->set_code(grpc::StatusCode::PERMISSION_DENIED);
        }
        return true;
      })
      .WillOnce(Return(false));
  EXPECT_CALL(*reader, Finish()).WillOnce(Return(grpc::Status::OK));

  EXPECT_CALL(*client_, MutateRows)
      .WillOnce(reader.release()->MakeMockReturner());

  // The backoff policy is cloned once in the Table constructor, and once before
  // the start of the `BulkApply` call. We set expectations on the second clone.
  auto b1 = std::make_unique<MockBackoffPolicy>();
  EXPECT_CALL(*b1, clone).WillOnce([]() {
    auto b2 = std::make_unique<MockBackoffPolicy>();
    EXPECT_CALL(*b2, clone).WillOnce([]() {
      auto mock = std::make_unique<MockBackoffPolicy>();
      EXPECT_CALL(*mock, Setup).Times(1);
      EXPECT_CALL(*mock, OnCompletion(An<grpc::Status const&>())).Times(0);
      return mock;
    });
    return b2;
  });
  auto table = Table(client_, kTableId, std::move(*b1));
  (void)table.BulkApply(BulkMutation(
      SingleRowMutation("foo", {SetCell("fam", "col", 0_ms, "baz")}),
      SingleRowMutation("bar", {SetCell("fam", "col", 0_ms, "qux")})));
}

}  // anonymous namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable
}  // namespace cloud
}  // namespace google
