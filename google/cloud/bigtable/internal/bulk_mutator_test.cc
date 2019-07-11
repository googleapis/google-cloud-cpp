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

#include "google/cloud/bigtable/internal/bulk_mutator.h"
#include "google/cloud/bigtable/testing/mock_completion_queue.h"
#include "google/cloud/bigtable/testing/mock_data_client.h"
#include "google/cloud/bigtable/testing/mock_mutate_rows_reader.h"
#include "google/cloud/bigtable/testing/mock_response_reader.h"
#include "google/cloud/internal/make_unique.h"
#include "google/cloud/testing_util/chrono_literals.h"

/// Define types and functions used in the tests.
namespace {
namespace btproto = google::bigtable::v2;
namespace bigtable = google::cloud::bigtable;
namespace bt = ::bigtable;
using namespace ::testing;
using namespace google::cloud::testing_util::chrono_literals;

using bigtable::testing::MockMutateRowsReader;
}  // anonymous namespace

/// @test Verify that MultipleRowsMutator handles easy cases.
TEST(MultipleRowsMutatorTest, Simple) {
  // In this test we create a Mutation for two rows, which succeeds in the
  // first RPC request.  First create the mutation.
  bt::BulkMutation mut(
      bt::SingleRowMutation("foo", {bt::SetCell("fam", "col", 0_ms, "baz")}),
      bt::SingleRowMutation("bar", {bt::SetCell("fam", "col", 0_ms, "qux")}));

  // Prepare the mocks.  The mutator should issue a RPC which must return a
  // stream of responses, we prepare the stream first because it is easier than
  // to create one of the fly.
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

  // Now prepare the client for the one request.
  bigtable::testing::MockDataClient client;
  EXPECT_CALL(client, MutateRows(_, _))
      .WillOnce(Invoke(reader.release()->MakeMockReturner()));

  auto policy = bt::DefaultIdempotentMutationPolicy();
  bt::internal::BulkMutator mutator("", "foo/bar/baz/table", *policy,
                                    std::move(mut));

  EXPECT_TRUE(mutator.HasPendingMutations());
  grpc::ClientContext context;
  auto status = mutator.MakeOneRequest(client, context);
  EXPECT_TRUE(status.ok());
  auto failures = std::move(mutator).OnRetryDone();
  EXPECT_TRUE(failures.empty());
}

/// @test Verify that MultipleRowsMutator uses app_profile_id when set.
TEST(MultipleRowsMutatorTest, BulkApply_AppProfileId) {
  namespace btproto = ::google::bigtable::v2;
  namespace bt = ::bigtable;
  using namespace ::testing;
  using namespace google::cloud::testing_util::chrono_literals;

  // In this test we create a Mutation for two rows, which succeeds in the
  // first RPC request.  First create the mutation.
  bt::BulkMutation mut(
      bt::SingleRowMutation("foo", {bt::SetCell("fam", "col", 0_ms, "baz")}),
      bt::SingleRowMutation("bar", {bt::SetCell("fam", "col", 0_ms, "qux")}));

  // Prepare the mocks.  The mutator should issue a RPC which must return a
  // stream of responses, we prepare the stream first because it is easier than
  // to create one of the fly.
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

  // Now prepare the client for the one request.
  std::string expected_id = "test-id";
  bigtable::testing::MockDataClient client;
  EXPECT_CALL(client, MutateRows(_, _))
      .WillOnce(
          Invoke([expected_id, &reader](grpc::ClientContext*,
                                        btproto::MutateRowsRequest const& req) {
            EXPECT_EQ(expected_id, req.app_profile_id());
            return reader.release()->AsUniqueMocked();
          }));

  auto policy = bt::DefaultIdempotentMutationPolicy();
  bt::internal::BulkMutator mutator("test-id", "foo/bar/baz/table", *policy,
                                    std::move(mut));

  EXPECT_TRUE(mutator.HasPendingMutations());
  grpc::ClientContext context;
  auto status = mutator.MakeOneRequest(client, context);
  EXPECT_TRUE(status.ok());
  auto failures = std::move(mutator).OnRetryDone();
  EXPECT_TRUE(failures.empty());
}

/// @test Verify that MultipleRowsMutator retries partial failures.
TEST(MultipleRowsMutatorTest, RetryPartialFailure) {
  // In this test we create a Mutation for two rows, one of which will fail.
  // First create the mutation.
  bt::BulkMutation mut(
      bt::SingleRowMutation("foo", {bt::SetCell("fam", "col", 0_ms, "baz")}),
      bt::SingleRowMutation("bar", {bt::SetCell("fam", "col", 0_ms, "qux")}));

  // Prepare the mocks for the request.  First create a stream response which
  // indicates a partial failure.
  auto r1 = google::cloud::internal::make_unique<MockMutateRowsReader>();
  EXPECT_CALL(*r1, Read(_))
      .WillOnce(Invoke([](btproto::MutateRowsResponse* r) {
        // Simulate a partial (and recoverable) failure.
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

  // Prepare a second stream response, because the client should retry after
  // the partial failure.
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

  // Setup the client to response with r1 first, and r2 next.
  bigtable::testing::MockDataClient client;
  EXPECT_CALL(client, MutateRows(_, _))
      .WillOnce(Invoke(
          [&r1](grpc::ClientContext*, btproto::MutateRowsRequest const& req) {
            EXPECT_EQ("foo/bar/baz/table", req.table_name());
            return r1.release()->AsUniqueMocked();
          }))
      .WillOnce(Invoke(
          [&r2](grpc::ClientContext*, btproto::MutateRowsRequest const& req) {
            EXPECT_EQ("foo/bar/baz/table", req.table_name());
            return r2.release()->AsUniqueMocked();
          }));

  auto policy = bt::DefaultIdempotentMutationPolicy();
  bt::internal::BulkMutator mutator("", "foo/bar/baz/table", *policy,
                                    std::move(mut));

  // This work will be in BulkApply(), but this is the test for BulkMutator in
  // isolation, so call MakeOneRequest() twice, for the r1, and the r2 cases.
  for (int i = 0; i != 2; ++i) {
    EXPECT_TRUE(mutator.HasPendingMutations());
    grpc::ClientContext context;
    auto status = mutator.MakeOneRequest(client, context);
    EXPECT_TRUE(status.ok());
  }
  auto failures = std::move(mutator).OnRetryDone();
  EXPECT_TRUE(failures.empty());
}

/// @test Verify that MultipleRowsMutator handles permanent failures.
TEST(MultipleRowsMutatorTest, PermanentFailure) {
  // In this test we handle a recoverable and one unrecoverable failures.
  // Create a bulk mutation with two SetCell() mutations.
  bt::BulkMutation mut(
      bt::SingleRowMutation("foo", {bt::SetCell("fam", "col", 0_ms, "baz")}),
      bt::SingleRowMutation("bar", {bt::SetCell("fam", "col", 0_ms, "qux")}));

  // Make the first RPC return one recoverable and one unrecoverable failures.
  auto r1 = google::cloud::internal::make_unique<MockMutateRowsReader>();
  EXPECT_CALL(*r1, Read(_))
      .WillOnce(Invoke([](btproto::MutateRowsResponse* r) {
        // Simulate a partial failure, which is recoverable for this first
        // element.
        auto& e0 = *r->add_entries();
        e0.set_index(0);
        e0.mutable_status()->set_code(grpc::StatusCode::UNAVAILABLE);
        // Simulate an unrecoverable failure for the second element.
        auto& e1 = *r->add_entries();
        e1.set_index(1);
        e1.mutable_status()->set_code(grpc::StatusCode::OUT_OF_RANGE);
        return true;
      }))
      .WillOnce(Return(false));
  EXPECT_CALL(*r1, Finish()).WillOnce(Return(grpc::Status::OK));

  // The BulkMutator should issue a second request, which will return success
  // for the remaining mutation.
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

  bigtable::testing::MockDataClient client;
  EXPECT_CALL(client, MutateRows(_, _))
      .WillOnce(Invoke(r1.release()->MakeMockReturner()))
      .WillOnce(Invoke(r2.release()->MakeMockReturner()));

  auto policy = bt::DefaultIdempotentMutationPolicy();
  bt::internal::BulkMutator mutator("", "foo/bar/baz/table", *policy,
                                    std::move(mut));

  // This work will be in BulkApply(), but this is the test for BulkMutator in
  // isolation, so call MakeOneRequest() twice, for the r1, and the r2 cases.
  for (int i = 0; i != 2; ++i) {
    EXPECT_TRUE(mutator.HasPendingMutations());
    grpc::ClientContext context;
    auto status = mutator.MakeOneRequest(client, context);
    EXPECT_TRUE(status.ok());
  }
  auto failures = std::move(mutator).OnRetryDone();
  ASSERT_EQ(1UL, failures.size());
  EXPECT_EQ(1, failures[0].original_index());
  // EXPECT_EQ("bar", failures[0].mutation().row_key());
  EXPECT_EQ(google::cloud::StatusCode::kOutOfRange,
            failures[0].status().code());
}

/// @test Verify that MultipleRowsMutator handles a stream with partial results
TEST(MultipleRowsMutatorTest, PartialStream) {
  // We are going to test the case where the stream does not contain a response
  // for all requests.  Create a BulkMutation with two entries.
  bt::BulkMutation mut(
      bt::SingleRowMutation("foo", {bt::SetCell("fam", "col", 0_ms, "baz")}),
      bt::SingleRowMutation("bar", {bt::SetCell("fam", "col", 0_ms, "qux")}));

  // This will be the stream returned by the first request.  It is missing
  // information about one of the mutations.
  auto r1 = google::cloud::internal::make_unique<MockMutateRowsReader>();
  EXPECT_CALL(*r1, Read(_))
      .WillOnce(Invoke([](btproto::MutateRowsResponse* r) {
        auto& e0 = *r->add_entries();
        e0.set_index(0);
        e0.mutable_status()->set_code(grpc::StatusCode::OK);
        return true;
      }))
      .WillOnce(Return(false));
  EXPECT_CALL(*r1, Finish()).WillOnce(Return(grpc::Status::OK));

  // The BulkMutation should issue a second request, this is the stream returned
  // by the second request, which indicates success for the missed mutation
  // on r1.
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

  bigtable::testing::MockDataClient client;
  EXPECT_CALL(client, MutateRows(_, _))
      .WillOnce(Invoke(r1.release()->MakeMockReturner()))
      .WillOnce(Invoke(r2.release()->MakeMockReturner()));

  auto policy = bt::DefaultIdempotentMutationPolicy();
  bt::internal::BulkMutator mutator("", "foo/bar/baz/table", *policy,
                                    std::move(mut));

  // This work will be in BulkApply(), but this is the test for BulkMutator in
  // isolation, so call MakeOneRequest() twice: for the r1 and r2 cases.
  for (int i = 0; i != 2; ++i) {
    EXPECT_TRUE(mutator.HasPendingMutations());
    grpc::ClientContext context;
    auto status = mutator.MakeOneRequest(client, context);
    EXPECT_TRUE(status.ok());
  }
  auto failures = std::move(mutator).OnRetryDone();
  EXPECT_TRUE(failures.empty());
}

/// @test Verify that MultipleRowsMutator only retries idempotent mutations
TEST(MultipleRowsMutatorTest, RetryOnlyIdempotent) {
  // Create a BulkMutation with a non-idempotent mutation.
  bt::BulkMutation mut(
      bt::SingleRowMutation("foo", {bt::SetCell("fam", "col", "baz")}),
      bt::SingleRowMutation("bar", {bt::SetCell("fam", "col", 0_ms, "qux")}),
      bt::SingleRowMutation("baz", {bt::SetCell("fam", "col", "v")}),
      bt::SingleRowMutation("bar", {bt::SetCell("fam", "col", 0_ms, "qux")}));

  // We will setup the mock to return recoverable failures for idempotent
  // mutations.
  auto r1 = google::cloud::internal::make_unique<MockMutateRowsReader>();
  EXPECT_CALL(*r1, Read(_))
      .WillOnce(Invoke([](btproto::MutateRowsResponse* r) {
        // Simulate recoverable failures for both elements.
        auto& e0 = *r->add_entries();
        e0.set_index(0);
        e0.mutable_status()->set_code(grpc::StatusCode::UNAVAILABLE);
        auto& e1 = *r->add_entries();
        e1.set_index(1);
        e1.mutable_status()->set_code(grpc::StatusCode::UNAVAILABLE);
        return true;
      }))
      .WillOnce(Return(false));
  EXPECT_CALL(*r1, Finish()).WillOnce(Return(grpc::Status::OK));

  // The BulkMutator should issue a second request, with only the idempotent
  // mutations, make the mocks return success for them.
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

  // Verify that the second response has the right contents.  It is easier (and
  // more readable) to write these in a separate small lambda expression because
  // ASSERT_EQ() has an embedded "return;" in it, which does not play well with
  // the rest of the stuff here.
  auto expect_r2 = [](btproto::MutateRowsRequest const& r) {
    ASSERT_EQ(2, r.entries_size());
    EXPECT_EQ("bar", r.entries(0).row_key());
  };
  bigtable::testing::MockDataClient client;
  EXPECT_CALL(client, MutateRows(_, _))
      .WillOnce(Invoke(
          [&r1](grpc::ClientContext*, btproto::MutateRowsRequest const& r) {
            EXPECT_EQ(4, r.entries_size());
            return r1.release()->AsUniqueMocked();
          }))
      .WillOnce(Invoke([&r2, expect_r2](grpc::ClientContext*,
                                        btproto::MutateRowsRequest const& r) {
        expect_r2(r);
        return r2.release()->AsUniqueMocked();
      }));

  auto policy = bt::DefaultIdempotentMutationPolicy();
  bt::internal::BulkMutator mutator("", "foo/bar/baz/table", *policy,
                                    std::move(mut));

  // This work will be in BulkApply(), but this is the test for BulkMutator in
  // isolation, so call MakeOneRequest() twice, for the r1, and the r2 cases.
  for (int i = 0; i != 2; ++i) {
    EXPECT_TRUE(mutator.HasPendingMutations());
    grpc::ClientContext context;
    auto status = mutator.MakeOneRequest(client, context);
    EXPECT_TRUE(status.ok());
  }
  auto failures = std::move(mutator).OnRetryDone();
  ASSERT_EQ(3UL, failures.size());
  EXPECT_EQ(0, failures[0].original_index());
  // EXPECT_EQ("foo", failures[0].mutation().row_key());
  EXPECT_EQ(google::cloud::StatusCode::kUnavailable,
            failures[0].status().code());

  EXPECT_EQ(2, failures[1].original_index());
  // EXPECT_EQ("baz", failures[1].mutation().row_key());
  EXPECT_EQ(google::cloud::StatusCode::kInternal, failures[1].status().code());
  EXPECT_EQ(google::cloud::StatusCode::kInternal, failures[2].status().code());
}

TEST(MultipleRowsMutatorTest, UnconfirmedAreFailed) {
  // Make sure that mutations which are not confirmed are reported as UNKNOWN
  // with the proper index.
  bt::BulkMutation mut(
      bt::SingleRowMutation("foo", {bt::SetCell("fam", "col", "baz")}),
      // this one will be unconfirmed
      bt::SingleRowMutation("bar", {bt::SetCell("fam", "col", "qux")}),
      bt::SingleRowMutation("baz", {bt::SetCell("fam", "col", "v")}));

  // We will setup the mock to return recoverable failures for idempotent
  // mutations.
  auto r1 = google::cloud::internal::make_unique<MockMutateRowsReader>();
  EXPECT_CALL(*r1, Read(_))
      .WillOnce(Invoke([](btproto::MutateRowsResponse* r) {
        auto& e0 = *r->add_entries();
        e0.set_index(0);
        e0.mutable_status()->set_code(grpc::StatusCode::OK);
        auto& e1 = *r->add_entries();
        e1.set_index(2);
        e1.mutable_status()->set_code(grpc::StatusCode::OK);
        return true;
      }))
      .WillOnce(Return(false));
  EXPECT_CALL(*r1, Finish())
      .WillOnce(Return(grpc::Status(grpc::StatusCode::PERMISSION_DENIED, "")));

  // The BulkMutator should not issue a second request because the error is
  // PERMISSION_DENIED (not retriable).

  bigtable::testing::MockDataClient client;
  EXPECT_CALL(client, MutateRows(_, _))
      .WillOnce(Invoke(
          [&r1](grpc::ClientContext*, btproto::MutateRowsRequest const& r) {
            EXPECT_EQ(3, r.entries_size());
            return r1.release()->AsUniqueMocked();
          }));

  auto policy = bt::DefaultIdempotentMutationPolicy();
  bt::internal::BulkMutator mutator("", "foo/bar/baz/table", *policy,
                                    std::move(mut));

  EXPECT_TRUE(mutator.HasPendingMutations());
  grpc::ClientContext context;
  auto status = mutator.MakeOneRequest(client, context);
  EXPECT_FALSE(status.ok());

  auto failures = std::move(mutator).OnRetryDone();
  ASSERT_EQ(1UL, failures.size());
  EXPECT_EQ(1, failures[0].original_index());
  // EXPECT_EQ("bar", failures[0].mutation().row_key());
  EXPECT_EQ(google::cloud::StatusCode::kPermissionDenied,
            failures.front().status().code());
}
