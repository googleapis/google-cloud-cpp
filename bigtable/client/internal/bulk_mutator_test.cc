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

#include "bigtable/client/internal/bulk_mutator.h"
#include "bigtable/client/internal/make_unique.h"
#include "bigtable/client/testing/chrono_literals.h"
#include <google/bigtable/v2/bigtable_mock.grpc.pb.h>

/// Define types and functions used in the tests.
namespace {
class MockReader : public grpc::ClientReaderInterface<
                       ::google::bigtable::v2::MutateRowsResponse> {
 public:
  MOCK_METHOD0(WaitForInitialMetadata, void());
  MOCK_METHOD0(Finish, grpc::Status());
  MOCK_METHOD1(NextMessageSize, bool(std::uint32_t*));
  MOCK_METHOD1(Read, bool(::google::bigtable::v2::MutateRowsResponse*));
};
}  // anonymous namespace

/// @test Verify that MultipleRowsMutator handles easy cases.
TEST(MultipleRowsMutatorTest, Simple) {
  namespace btproto = ::google::bigtable::v2;
  namespace bt = ::bigtable;
  using namespace ::testing;
  using namespace bigtable::chrono_literals;

  // In this test we create a Mutation for two rows, which succeeds in the
  // first RPC request.  First create the mutation.
  bt::BulkMutation mut(
      bt::SingleRowMutation("foo", {bt::SetCell("fam", "col", 0_ms, "baz")}),
      bt::SingleRowMutation("bar", {bt::SetCell("fam", "col", 0_ms, "qux")}));

  // Prepare the mocks.  The mutator should issue a RPC which must return a
  // stream of responses, we prepare the stream first because it is easier than
  // to create one of the fly.
  auto reader = bigtable::internal::make_unique<MockReader>();
  EXPECT_CALL(*reader, Read(_))
      .WillOnce(Invoke([](btproto::MutateRowsResponse* r) {
        {
          auto& e = *r->add_entries();
          e.set_index(0);
          e.mutable_status()->set_code(grpc::OK);
        }
        {
          auto& e = *r->add_entries();
          e.set_index(1);
          e.mutable_status()->set_code(grpc::OK);
        }
        return true;
      }))
      .WillOnce(Return(false));
  EXPECT_CALL(*reader, Finish()).WillOnce(Return(grpc::Status::OK));

  // Now prepare the client for the one request.
  btproto::MockBigtableStub stub;
  EXPECT_CALL(stub, MutateRowsRaw(_, _))
      .WillOnce(Invoke(
          [&reader](grpc::ClientContext*, btproto::MutateRowsRequest const&) {
            return reader.release();
          }));

  auto policy = bt::DefaultIdempotentMutationPolicy();
  bt::internal::BulkMutator mutator("foo/bar/baz/table", *policy,
                                    std::move(mut));

  EXPECT_TRUE(mutator.HasPendingMutations());
  grpc::ClientContext context;
  auto status = mutator.MakeOneRequest(stub, context);
  EXPECT_TRUE(status.ok());
  auto failures = mutator.ExtractFinalFailures();
  EXPECT_TRUE(failures.empty());
}

/// @test Verify that MultipleRowsMutator retries partial failures.
TEST(MultipleRowsMutatorTest, RetryPartialFailure) {
  namespace btproto = ::google::bigtable::v2;
  namespace bt = ::bigtable;
  using namespace ::testing;
  using namespace bigtable::chrono_literals;

  // In this test we create a Mutation for two rows, one of which will fail.
  // First create the mutation.
  bt::BulkMutation mut(
      bt::SingleRowMutation("foo", {bt::SetCell("fam", "col", 0_ms, "baz")}),
      bt::SingleRowMutation("bar", {bt::SetCell("fam", "col", 0_ms, "qux")}));

  // Prepare the mocks for the request.  First create a stream response which
  // indicates a partial failure.
  auto r1 = bigtable::internal::make_unique<MockReader>();
  EXPECT_CALL(*r1, Read(_))
      .WillOnce(Invoke([](btproto::MutateRowsResponse* r) {
        // Simulate a partial (and recoverable) failure.
        auto& e0 = *r->add_entries();
        e0.set_index(0);
        e0.mutable_status()->set_code(grpc::UNAVAILABLE);
        auto& e1 = *r->add_entries();
        e1.set_index(1);
        e1.mutable_status()->set_code(grpc::OK);
        return true;
      }))
      .WillOnce(Return(false));
  EXPECT_CALL(*r1, Finish()).WillOnce(Return(grpc::Status::OK));

  // Prepare a second stream response, because the client should retry after
  // the partial failure.
  auto r2 = bigtable::internal::make_unique<MockReader>();
  EXPECT_CALL(*r2, Read(_))
      .WillOnce(Invoke([](btproto::MutateRowsResponse* r) {
        {
          auto& e = *r->add_entries();
          e.set_index(0);
          e.mutable_status()->set_code(grpc::OK);
        }
        return true;
      }))
      .WillOnce(Return(false));
  EXPECT_CALL(*r2, Finish()).WillOnce(Return(grpc::Status::OK));

  // Setup the client to response with r1 first, and r2 next.
  btproto::MockBigtableStub stub;
  EXPECT_CALL(stub, MutateRowsRaw(_, _))
      .WillOnce(Invoke(
          [&r1](grpc::ClientContext*, btproto::MutateRowsRequest const& req) {
            EXPECT_EQ("foo/bar/baz/table", req.table_name());
            return r1.release();
          }))
      .WillOnce(Invoke(
          [&r2](grpc::ClientContext*, btproto::MutateRowsRequest const& req) {
            EXPECT_EQ("foo/bar/baz/table", req.table_name());
            return r2.release();
          }));

  auto policy = bt::DefaultIdempotentMutationPolicy();
  bt::internal::BulkMutator mutator("foo/bar/baz/table", *policy,
                                    std::move(mut));

  // This work will be in BulkApply(), but this is the test for BulkMutator in
  // isolation, so call MakeOneRequest() twice, for the r1, and the r2 cases.
  for (int i = 0; i != 2; ++i) {
    EXPECT_TRUE(mutator.HasPendingMutations());
    grpc::ClientContext context;
    auto status = mutator.MakeOneRequest(stub, context);
    EXPECT_TRUE(status.ok());
  }
  auto failures = mutator.ExtractFinalFailures();
  EXPECT_TRUE(failures.empty());
}

/// @test Verify that MultipleRowsMutator handles permanent failures.
TEST(MultipleRowsMutatorTest, PermanentFailure) {
  namespace btproto = ::google::bigtable::v2;
  namespace bt = ::bigtable;
  using namespace ::testing;
  using namespace bigtable::chrono_literals;

  // In this test we handle a recoverable and one unrecoverable failures.
  // Create a bulk mutation with two SetCell() mutations.
  bt::BulkMutation mut(
      bt::SingleRowMutation("foo", {bt::SetCell("fam", "col", 0_ms, "baz")}),
      bt::SingleRowMutation("bar", {bt::SetCell("fam", "col", 0_ms, "qux")}));

  // Make the first RPC return one recoverable and one unrecoverable failures.
  auto r1 = bigtable::internal::make_unique<MockReader>();
  EXPECT_CALL(*r1, Read(_))
      .WillOnce(Invoke([](btproto::MutateRowsResponse* r) {
        // Simulate a partial failure, which is recoverable for this first
        // element.
        auto& e0 = *r->add_entries();
        e0.set_index(0);
        e0.mutable_status()->set_code(grpc::UNAVAILABLE);
        // Simulate an unrecoverable failure for the second element.
        auto& e1 = *r->add_entries();
        e1.set_index(1);
        e1.mutable_status()->set_code(grpc::OUT_OF_RANGE);
        return true;
      }))
      .WillOnce(Return(false));
  EXPECT_CALL(*r1, Finish()).WillOnce(Return(grpc::Status::OK));

  // The BulkMutator should issue a second request, which will return success
  // for the remaining mutation.
  auto r2 = bigtable::internal::make_unique<MockReader>();
  EXPECT_CALL(*r2, Read(_))
      .WillOnce(Invoke([](btproto::MutateRowsResponse* r) {
        {
          auto& e = *r->add_entries();
          e.set_index(0);
          e.mutable_status()->set_code(grpc::OK);
        }
        return true;
      }))
      .WillOnce(Return(false));
  EXPECT_CALL(*r2, Finish()).WillOnce(Return(grpc::Status::OK));

  btproto::MockBigtableStub stub;
  EXPECT_CALL(stub, MutateRowsRaw(_, _))
      .WillOnce(Invoke(
          [&r1](grpc::ClientContext*, btproto::MutateRowsRequest const&) {
            return r1.release();
          }))
      .WillOnce(Invoke(
          [&r2](grpc::ClientContext*, btproto::MutateRowsRequest const&) {
            return r2.release();
          }));

  auto policy = bt::DefaultIdempotentMutationPolicy();
  bt::internal::BulkMutator mutator("foo/bar/baz/table", *policy,
                                    std::move(mut));

  // This work will be in BulkApply(), but this is the test for BulkMutator in
  // isolation, so call MakeOneRequest() twice, for the r1, and the r2 cases.
  for (int i = 0; i != 2; ++i) {
    EXPECT_TRUE(mutator.HasPendingMutations());
    grpc::ClientContext context;
    auto status = mutator.MakeOneRequest(stub, context);
    EXPECT_TRUE(status.ok());
  }
  auto failures = mutator.ExtractFinalFailures();
  ASSERT_EQ(1UL, failures.size());
  EXPECT_EQ(1, failures[0].original_index());
  EXPECT_EQ("bar", failures[0].mutation().row_key());
  EXPECT_EQ(grpc::StatusCode::OUT_OF_RANGE, failures[0].status().error_code());
}

/// @test Verify that MultipleRowsMutator handles a stream with partial results
TEST(MultipleRowsMutatorTest, PartialStream) {
  namespace btproto = ::google::bigtable::v2;
  namespace bt = ::bigtable;
  using namespace ::testing;
  using namespace bigtable::chrono_literals;

  // We are going to test the case where the stream does not contain a response
  // for all requests.  Create a BulkMutation with two entries.
  bt::BulkMutation mut(
      bt::SingleRowMutation("foo", {bt::SetCell("fam", "col", 0_ms, "baz")}),
      bt::SingleRowMutation("bar", {bt::SetCell("fam", "col", 0_ms, "qux")}));

  // This will be the stream returned by the first request.  It is missing
  // information about one of the mutations.
  auto r1 = bigtable::internal::make_unique<MockReader>();
  EXPECT_CALL(*r1, Read(_))
      .WillOnce(Invoke([](btproto::MutateRowsResponse* r) {
        auto& e0 = *r->add_entries();
        e0.set_index(0);
        e0.mutable_status()->set_code(grpc::OK);
        return true;
      }))
      .WillOnce(Return(false));
  EXPECT_CALL(*r1, Finish()).WillOnce(Return(grpc::Status::OK));

  // The BulkMutation should issue a second request, this is the stream returned
  // by the second request, which indicates success for the missed mutation
  // on r1.
  auto r2 = bigtable::internal::make_unique<MockReader>();
  EXPECT_CALL(*r2, Read(_))
      .WillOnce(Invoke([](btproto::MutateRowsResponse* r) {
        {
          auto& e = *r->add_entries();
          e.set_index(0);
          e.mutable_status()->set_code(grpc::OK);
        }
        return true;
      }))
      .WillOnce(Return(false));
  EXPECT_CALL(*r2, Finish()).WillOnce(Return(grpc::Status::OK));

  btproto::MockBigtableStub stub;
  EXPECT_CALL(stub, MutateRowsRaw(_, _))
      .WillOnce(Invoke(
          [&r1](grpc::ClientContext*, btproto::MutateRowsRequest const&) {
            return r1.release();
          }))
      .WillOnce(Invoke(
          [&r2](grpc::ClientContext*, btproto::MutateRowsRequest const&) {
            return r2.release();
          }));

  auto policy = bt::DefaultIdempotentMutationPolicy();
  bt::internal::BulkMutator mutator("foo/bar/baz/table", *policy,
                                    std::move(mut));

  // This work will be in BulkApply(), but this is the test for BulkMutator in
  // isolation, so call MakeOneRequest() twice: for the r1 and r2 cases.
  for (int i = 0; i != 2; ++i) {
    EXPECT_TRUE(mutator.HasPendingMutations());
    grpc::ClientContext context;
    auto status = mutator.MakeOneRequest(stub, context);
    EXPECT_TRUE(status.ok());
  }
  auto failures = mutator.ExtractFinalFailures();
  EXPECT_TRUE(failures.empty());
}

/// @test Verify that MultipleRowsMutator only retries idempotent mutations
TEST(MultipleRowsMutatorTest, RetryOnlyIdempotent) {
  namespace btproto = ::google::bigtable::v2;
  namespace bt = ::bigtable;
  using namespace ::testing;
  using namespace bigtable::chrono_literals;

  // Create a BulkMutation with a non-idempotent mutation.
  bt::BulkMutation mut(
      bt::SingleRowMutation("foo", {bt::SetCell("fam", "col", "baz")}),
      bt::SingleRowMutation("bar", {bt::SetCell("fam", "col", 0_ms, "qux")}),
      bt::SingleRowMutation("baz", {bt::SetCell("fam", "col", "v")}));

  // We will setup the mock to return recoverable failures for idempotent
  // mutations.
  auto r1 = bigtable::internal::make_unique<MockReader>();
  EXPECT_CALL(*r1, Read(_))
      .WillOnce(Invoke([](btproto::MutateRowsResponse* r) {
        // Simulate recoverable failures for both elements.
        auto& e0 = *r->add_entries();
        e0.set_index(0);
        e0.mutable_status()->set_code(grpc::UNAVAILABLE);
        auto& e1 = *r->add_entries();
        e1.set_index(1);
        e1.mutable_status()->set_code(grpc::UNAVAILABLE);
        return true;
      }))
      .WillOnce(Return(false));
  EXPECT_CALL(*r1, Finish()).WillOnce(Return(grpc::Status::OK));

  // The BulkMutator should issue a second request, with only the idempotent
  // mutations, make the mocks return success for them.
  auto r2 = bigtable::internal::make_unique<MockReader>();
  EXPECT_CALL(*r2, Read(_))
      .WillOnce(Invoke([](btproto::MutateRowsResponse* r) {
        {
          auto& e = *r->add_entries();
          e.set_index(0);
          e.mutable_status()->set_code(grpc::OK);
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
    ASSERT_EQ(1, r.entries_size());
    EXPECT_EQ("bar", r.entries(0).row_key());
  };
  btproto::MockBigtableStub stub;
  EXPECT_CALL(stub, MutateRowsRaw(_, _))
      .WillOnce(Invoke(
          [&r1](grpc::ClientContext*, btproto::MutateRowsRequest const& r) {
            EXPECT_EQ(3, r.entries_size());
            return r1.release();
          }))
      .WillOnce(Invoke([&r2, expect_r2](grpc::ClientContext*,
                                        btproto::MutateRowsRequest const& r) {
        expect_r2(r);
        return r2.release();
      }));

  auto policy = bt::DefaultIdempotentMutationPolicy();
  bt::internal::BulkMutator mutator("foo/bar/baz/table", *policy,
                                    std::move(mut));

  // This work will be in BulkApply(), but this is the test for BulkMutator in
  // isolation, so call MakeOneRequest() twice, for the r1, and the r2 cases.
  for (int i = 0; i != 2; ++i) {
    EXPECT_TRUE(mutator.HasPendingMutations());
    grpc::ClientContext context;
    auto status = mutator.MakeOneRequest(stub, context);
    EXPECT_TRUE(status.ok());
  }
  auto failures = mutator.ExtractFinalFailures();
  ASSERT_EQ(2UL, failures.size());
  EXPECT_EQ(0, failures[0].original_index());
  EXPECT_EQ("foo", failures[0].mutation().row_key());
  EXPECT_EQ(grpc::StatusCode::UNAVAILABLE, failures[0].status().error_code());

  EXPECT_EQ(2, failures[1].original_index());
  EXPECT_EQ("baz", failures[1].mutation().row_key());
  EXPECT_EQ(grpc::StatusCode::OK, failures[1].status().error_code());
}
