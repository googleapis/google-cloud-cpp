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
#include "google/cloud/bigtable/testing/mock_mutate_rows_reader.h"
#include "google/cloud/bigtable/testing/table_test_fixture.h"
#include "google/cloud/internal/make_unique.h"
#include "google/cloud/testing_util/chrono_literals.h"

namespace btproto = google::bigtable::v2;
namespace bigtable = google::cloud::bigtable;
using namespace google::cloud::testing_util::chrono_literals;
using namespace testing;
namespace bt = google::cloud::bigtable;

/// Define types and functions used in the tests.
namespace {
class TableBulkApplyTest : public bigtable::testing::TableTestFixture {};
using bigtable::testing::MockMutateRowsReader;
}  // anonymous namespace

/// @test Verify that Table::BulkApply() works in the easy case.

TEST_F(TableBulkApplyTest, Simple) {
  auto reader = google::cloud::internal::make_unique<MockMutateRowsReader>();
  EXPECT_CALL(*reader, Read(_))
      .WillOnce(Invoke([](btproto::MutateRowsResponse* r) {
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
      }))
      .WillOnce(Return(false));
  EXPECT_CALL(*reader, Finish()).WillOnce(Return(grpc::Status::OK));

  EXPECT_CALL(*client_, MutateRows(_, _))
      .WillOnce(Invoke(reader.release()->MakeMockReturner()));

  table_.BulkApply(bt::BulkMutation(
      bt::SingleRowMutation("foo", {bt::SetCell("fam", "col", 0_ms, "baz")}),
      bt::SingleRowMutation("bar", {bt::SetCell("fam", "col", 0_ms, "qux")})));
  SUCCEED();
}

/// @test Verify that Table::BulkApply() retries partial failures.
TEST_F(TableBulkApplyTest, RetryPartialFailure) {
  auto r1 = google::cloud::internal::make_unique<MockMutateRowsReader>();
  EXPECT_CALL(*r1, Read(_))
      .WillOnce(Invoke([](btproto::MutateRowsResponse* r) {
        // Simulate a partial (recoverable) failure.
        auto& e0 = *r->add_entries();
        e0.set_index(0);
        e0.mutable_status()->set_code(grpc::StatusCode::UNAVAILABLE);
        auto& e1 = *r->add_entries();
        e1.set_index(1);
        e1.mutable_status()->set_code(grpc::StatusCode::OK);
        return true;
      }))
      .WillOnce(Return(false));
  EXPECT_CALL(*r1, Finish()).WillOnce(Return(grpc::Status::OK));

  auto r2 = google::cloud::internal::make_unique<MockMutateRowsReader>();
  EXPECT_CALL(*r2, Read(_))
      .WillOnce(Invoke([](btproto::MutateRowsResponse* r) {
        {
          auto& e = *r->add_entries();
          e.set_index(0);
          e.mutable_status()->set_code(grpc::StatusCode::OK);
        }
        return true;
      }))
      .WillOnce(Return(false));
  EXPECT_CALL(*r2, Finish()).WillOnce(Return(grpc::Status::OK));

  EXPECT_CALL(*client_, MutateRows(_, _))
      .WillOnce(Invoke(r1.release()->MakeMockReturner()))
      .WillOnce(Invoke(r2.release()->MakeMockReturner()));

  table_.BulkApply(bt::BulkMutation(
      bt::SingleRowMutation("foo",
                            {bigtable::SetCell("fam", "col", 0_ms, "baz")}),
      bt::SingleRowMutation("bar",
                            {bigtable::SetCell("fam", "col", 0_ms, "qux")})));
  SUCCEED();
}

#if GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
/// @test Verify that Table::BulkApply() handles permanent failures.
TEST_F(TableBulkApplyTest, PermanentFailure) {
  auto r1 = google::cloud::internal::make_unique<MockMutateRowsReader>();
  EXPECT_CALL(*r1, Read(_))
      .WillOnce(Invoke([](btproto::MutateRowsResponse* r) {
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
      }))
      .WillOnce(Return(false));
  EXPECT_CALL(*r1, Finish()).WillOnce(Return(grpc::Status::OK));

  EXPECT_CALL(*client_, MutateRows(_, _))
      .WillOnce(Invoke(r1.release()->MakeMockReturner()));

  EXPECT_THROW(table_.BulkApply(bt::BulkMutation(
                   bt::SingleRowMutation(
                       "foo", {bt::SetCell("fam", "col", 0_ms, "baz")}),
                   bt::SingleRowMutation(
                       "bar", {bt::SetCell("fam", "col", 0_ms, "qux")}))),
               std::exception);
}
#endif  // GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS

/// @test Verify that Table::BulkApply() handles a terminated stream.
TEST_F(TableBulkApplyTest, CanceledStream) {
  // Simulate a stream that returns one success and then terminates.  We expect
  // the BulkApply() operation to retry the request, because the mutation is in
  // an undetermined state.  Well, it should retry assuming it is idempotent,
  // which happens to be the case in this test.
  auto r1 = google::cloud::internal::make_unique<MockMutateRowsReader>();
  EXPECT_CALL(*r1, Read(_))
      .WillOnce(Invoke([](btproto::MutateRowsResponse* r) {
        {
          auto& e = *r->add_entries();
          e.set_index(0);
          e.mutable_status()->set_code(grpc::StatusCode::OK);
        }
        return true;
      }))
      .WillOnce(Return(false));
  EXPECT_CALL(*r1, Finish()).WillOnce(Return(grpc::Status::OK));

  // Create a second stream returned by the mocks when the client retries.
  auto r2 = google::cloud::internal::make_unique<MockMutateRowsReader>();
  EXPECT_CALL(*r2, Read(_))
      .WillOnce(Invoke([](btproto::MutateRowsResponse* r) {
        {
          auto& e = *r->add_entries();
          e.set_index(0);
          e.mutable_status()->set_code(grpc::StatusCode::OK);
        }
        return true;
      }))
      .WillOnce(Return(false));
  EXPECT_CALL(*r2, Finish()).WillOnce(Return(grpc::Status::OK));

  EXPECT_CALL(*client_, MutateRows(_, _))
      .WillOnce(Invoke(r1.release()->MakeMockReturner()))
      .WillOnce(Invoke(r2.release()->MakeMockReturner()));

  table_.BulkApply(bt::BulkMutation(
      bt::SingleRowMutation("foo", {bt::SetCell("fam", "col", 0_ms, "baz")}),
      bt::SingleRowMutation("bar", {bt::SetCell("fam", "col", 0_ms, "qux")})));
  SUCCEED();
}

#if GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
/// @test Verify that Table::BulkApply() reports correctly on too many errors.
TEST_F(TableBulkApplyTest, TooManyFailures) {
  // Create a table with specific policies so we can test the behavior
  // without having to depend on timers expiring.  In this case tolerate only
  // 3 failures.
  bt::Table custom_table(
      client_, "foo_table",
      // Configure the Table to stop at 3 failures.
      bt::LimitedErrorCountRetryPolicy(2),
      // Use much shorter backoff than the default to test faster.
      bt::ExponentialBackoffPolicy(10_us, 40_us));

  // Setup the mocks to fail more than 3 times.
  auto r1 = google::cloud::internal::make_unique<MockMutateRowsReader>();
  EXPECT_CALL(*r1, Read(_))
      .WillOnce(Invoke([](btproto::MutateRowsResponse* r) {
        {
          auto& e = *r->add_entries();
          e.set_index(0);
          e.mutable_status()->set_code(grpc::StatusCode::OK);
        }
        return true;
      }))
      .WillOnce(Return(false));
  EXPECT_CALL(*r1, Finish())
      .WillOnce(Return(grpc::Status(grpc::StatusCode::ABORTED, "")));

  auto create_cancelled_stream = [&](grpc::ClientContext*,
                                     btproto::MutateRowsRequest const&) {
    auto stream = google::cloud::internal::make_unique<MockMutateRowsReader>();
    EXPECT_CALL(*stream, Read(_)).WillOnce(Return(false));
    EXPECT_CALL(*stream, Finish())
        .WillOnce(Return(grpc::Status(grpc::StatusCode::ABORTED, "")));
    return stream.release()->AsUniqueMocked();
  };

  EXPECT_CALL(*client_, MutateRows(_, _))
      .WillOnce(Invoke(r1.release()->MakeMockReturner()))
      .WillOnce(Invoke(create_cancelled_stream))
      .WillOnce(Invoke(create_cancelled_stream));

  EXPECT_THROW(custom_table.BulkApply(bt::BulkMutation(
                   bt::SingleRowMutation(
                       "foo", {bt::SetCell("fam", "col", 0_ms, "baz")}),
                   bt::SingleRowMutation(
                       "bar", {bt::SetCell("fam", "col", 0_ms, "qux")}))),
               std::exception);
}

/// @test Verify that Table::BulkApply() retries only idempotent mutations.
TEST_F(TableBulkApplyTest, RetryOnlyIdempotent) {
  // We will send both idempotent and non-idempotent mutations.  We prepare the
  // mocks to return an empty stream in the first RPC request.  That will force
  // the client to only retry the idempotent mutations.
  auto r1 = google::cloud::internal::make_unique<MockMutateRowsReader>();
  EXPECT_CALL(*r1, Read(_)).WillOnce(Return(false));
  EXPECT_CALL(*r1, Finish()).WillOnce(Return(grpc::Status::OK));

  auto r2 = google::cloud::internal::make_unique<MockMutateRowsReader>();
  EXPECT_CALL(*r2, Read(_))
      .WillOnce(Invoke([](btproto::MutateRowsResponse* r) {
        {
          auto& e = *r->add_entries();
          e.set_index(0);
          e.mutable_status()->set_code(grpc::StatusCode::OK);
        }
        return true;
      }))
      .WillOnce(Return(false));
  EXPECT_CALL(*r2, Finish()).WillOnce(Return(grpc::Status::OK));

  EXPECT_CALL(*client_, MutateRows(_, _))
      .WillOnce(Invoke(r1.release()->MakeMockReturner()))
      .WillOnce(Invoke(r2.release()->MakeMockReturner()));

  try {
    table_.BulkApply(bt::BulkMutation(
        bt::SingleRowMutation("is-idempotent",
                              {bt::SetCell("fam", "col", 0_ms, "qux")}),
        bt::SingleRowMutation("not-idempotent",
                              {bt::SetCell("fam", "col", "baz")})));
  } catch (bt::PermanentMutationFailure const& ex) {
    ASSERT_EQ(1UL, ex.failures().size());
    EXPECT_EQ(1, ex.failures()[0].original_index());
    EXPECT_EQ("not-idempotent", ex.failures()[0].mutation().row_key());
  } catch (std::exception const& ex) {
    FAIL() << "unexpected std::exception raised: " << ex.what();
  } catch (...) {
    FAIL() << "unexpected exception of unknown type raised";
  }
}

/// @test Verify that Table::BulkApply() works when the RPC fails.
TEST_F(TableBulkApplyTest, FailedRPC) {
  auto reader = google::cloud::internal::make_unique<MockMutateRowsReader>();
  EXPECT_CALL(*reader, Read(_)).WillOnce(Return(false));
  EXPECT_CALL(*reader, Finish())
      .WillOnce(Return(grpc::Status(grpc::StatusCode::FAILED_PRECONDITION,
                                    "no such table")));

  EXPECT_CALL(*client_, MutateRows(_, _))
      .WillOnce(Invoke(reader.release()->MakeMockReturner()));

  try {
    table_.BulkApply(bt::BulkMutation(
        bt::SingleRowMutation("foo", {bt::SetCell("fam", "col", 0_ms, "baz")}),
        bt::SingleRowMutation("bar",
                              {bt::SetCell("fam", "col", 0_ms, "qux")})));
  } catch (bt::PermanentMutationFailure const& ex) {
    EXPECT_EQ(2UL, ex.failures().size());
    EXPECT_EQ(grpc::StatusCode::FAILED_PRECONDITION, ex.status().error_code());
    EXPECT_EQ("no such table", ex.status().error_message());
  } catch (std::exception const& ex) {
    FAIL() << "unexpected std::exception raised: " << ex.what();
  } catch (...) {
    FAIL() << "unexpected exception of unknown type raised";
  }
}
#endif  // GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
