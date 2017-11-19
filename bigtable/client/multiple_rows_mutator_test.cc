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

#include "bigtable/client/multiple_rows_mutator.h"

#include <absl/memory/memory.h>
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
  namespace bt = bigtable;
  using namespace ::testing;

  // In this test we create a Mutation for two rows, which succeeds in the
  // first RPC request ...
  bt::MultipleRowMutations mut(
      bt::SingleRowMutation("foo", {bigtable::SetCell("fam", "col", 0, "baz")}),
      bt::SingleRowMutation("bar",
                            {bigtable::SetCell("fam", "col", 0, "qux")}));

  // ... the mutator will issue an RPC which must return a stream of responses,
  // we prepare the stream first because it is easier to do so ...
  auto reader = absl::make_unique<MockReader>();
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

  // ... then prepare the client to receive one request ...
  btproto::MockBigtableStub stub;
  EXPECT_CALL(stub, MutateRowsRaw(_, _))
      .WillOnce(Invoke(
          [&reader](grpc::ClientContext*, btproto::MutateRowsRequest const&) {
            return reader.release();
          }));

  auto policy = bt::DefaultIdempotentMutationPolicy();
  bt::detail::MultipleRowsMutator mutator("foo/bar/baz/table",
                                          *policy,
                                          std::move(mut));

  EXPECT_TRUE(mutator.has_pending_mutations());
  grpc::ClientContext context;
  auto status = mutator.make_one_request(stub, context);
  EXPECT_TRUE(status.ok());
  auto failures = mutator.extract_final_failures();
  EXPECT_TRUE(failures.empty());
}

/// @test Verify that MultipleRowsMutator retries partial failures.
TEST(MultipleRowsMutatorTest, RetryPartialFailure) {
  namespace btproto = ::google::bigtable::v2;
  namespace bt = bigtable;
  using namespace ::testing;

  // In this test we create a Mutation for two rows ...
  bt::MultipleRowMutations mut(
      bt::SingleRowMutation("foo", {bigtable::SetCell("fam", "col", 0, "baz")}),
      bt::SingleRowMutation("bar",
                            {bigtable::SetCell("fam", "col", 0, "qux")}));

  // ... the first RPC will return a (recoverable) failure for the first
  // mutation, and success for the second mutation ...
  auto r1 = absl::make_unique<MockReader>();
  EXPECT_CALL(*r1, Read(_))
      .WillOnce(Invoke([](btproto::MutateRowsResponse* r) {
        // ... simulate a partial (recoverable) failure ...
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

  // ... the Mutator should issue a second request, which will return success
  // for the remaining mutation ...
  auto r2 = absl::make_unique<MockReader>();
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
  bt::detail::MultipleRowsMutator mutator("foo/bar/baz/table",
                                          *policy,
                                          std::move(mut));

  // ... we need to call make_one_request() twice of course ...
  for (int i = 0; i != 2; ++i) {
    EXPECT_TRUE(mutator.has_pending_mutations());
    grpc::ClientContext context;
    auto status = mutator.make_one_request(stub, context);
    EXPECT_TRUE(status.ok());
  }
  auto failures = mutator.extract_final_failures();
  EXPECT_TRUE(failures.empty());
}

/// @test Verify that MultipleRowsMutator handles permanent failures.
TEST(MultipleRowsMutatorTest, PermanentFailure) {
  namespace btproto = ::google::bigtable::v2;
  namespace bt = bigtable;
  using namespace ::testing;

  // In this test we create a Mutation for two rows ...
  bt::MultipleRowMutations mut(
      bt::SingleRowMutation("foo", {bigtable::SetCell("fam", "col", 0, "baz")}),
      bt::SingleRowMutation("bar",
                            {bigtable::SetCell("fam", "col", 0, "qux")}));

  // ... the first RPC will return one recoverable and one unrecoverable
  // failure ...
  auto r1 = absl::make_unique<MockReader>();
  EXPECT_CALL(*r1, Read(_))
      .WillOnce(Invoke([](btproto::MutateRowsResponse* r) {
        // ... simulate a partial (recoverable) failure ...
        auto& e0 = *r->add_entries();
        e0.set_index(0);
        e0.mutable_status()->set_code(grpc::UNAVAILABLE);
        auto& e1 = *r->add_entries();
        e1.set_index(1);
        e1.mutable_status()->set_code(grpc::OUT_OF_RANGE);
        return true;
      }))
      .WillOnce(Return(false));
  EXPECT_CALL(*r1, Finish()).WillOnce(Return(grpc::Status::OK));

  // ... the Mutator should issue a second request, which will return success
  // for the remaining mutation ...
  auto r2 = absl::make_unique<MockReader>();
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
  bt::detail::MultipleRowsMutator mutator("foo/bar/baz/table",
                                          *policy,
                                          std::move(mut));

  // ... we need to call make_one_request() twice of course ...
  for (int i = 0; i != 2; ++i) {
    EXPECT_TRUE(mutator.has_pending_mutations());
    grpc::ClientContext context;
    auto status = mutator.make_one_request(stub, context);
    EXPECT_TRUE(status.ok());
  }
  auto failures = mutator.extract_final_failures();
  ASSERT_EQ(failures.size(), 1UL);
  EXPECT_EQ(failures[0].original_index(), 1);
  EXPECT_EQ(failures[0].mutation().row_key(), "bar");
  EXPECT_EQ(failures[0].status().error_code(), grpc::StatusCode::OUT_OF_RANGE);
}

/// @test Verify that MultipleRowsMutator handles a stream with partial results
TEST(MultipleRowsMutatorTest, PartialStream) {
  namespace btproto = ::google::bigtable::v2;
  namespace bt = bigtable;
  using namespace ::testing;

  // In this test we create a Mutation for two rows ...
  bt::MultipleRowMutations mut(
      bt::SingleRowMutation("foo", {bigtable::SetCell("fam", "col", 0, "baz")}),
      bt::SingleRowMutation("bar",
                            {bigtable::SetCell("fam", "col", 0, "qux")}));

  // ... the first RPC will return a short stream ...
  auto r1 = absl::make_unique<MockReader>();
  EXPECT_CALL(*r1, Read(_))
      .WillOnce(Invoke([](btproto::MutateRowsResponse* r) {
        // ... simulate a partial (recoverable) failure ...
        auto& e0 = *r->add_entries();
        e0.set_index(0);
        e0.mutable_status()->set_code(grpc::OK);
        return true;
      }))
      .WillOnce(Return(false));
  EXPECT_CALL(*r1, Finish()).WillOnce(Return(grpc::Status::OK));

  // ... the Mutator should issue a second request, which will return success
  // for the remaining mutation ...
  auto r2 = absl::make_unique<MockReader>();
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
  bt::detail::MultipleRowsMutator mutator("foo/bar/baz/table",
                                          *policy,
                                          std::move(mut));

  // ... we need to call make_one_request() twice of course ...
  for (int i = 0; i != 2; ++i) {
    EXPECT_TRUE(mutator.has_pending_mutations());
    grpc::ClientContext context;
    auto status = mutator.make_one_request(stub, context);
    EXPECT_TRUE(status.ok());
  }
  auto failures = mutator.extract_final_failures();
  EXPECT_TRUE(failures.empty());
}

/// @test Verify that MultipleRowsMutator only retries idempotent mutations
TEST(MultipleRowsMutatorTest, RetryOnlyIdempotent) {
  namespace btproto = ::google::bigtable::v2;
  namespace bt = bigtable;
  using namespace ::testing;

  // In this test we create a Mutation for three rows ...
  bt::MultipleRowMutations mut(
      bt::SingleRowMutation("foo", {bigtable::SetCell("fam", "col", "baz")}),
      bt::SingleRowMutation("bar",
                            {bigtable::SetCell("fam", "col", 0, "qux")}),
      bt::SingleRowMutation("baz", {bigtable::SetCell("fam", "col", "v")}));

  // ... the first RPC will return (recoverable) failures for two mutations,
  // and is missing the last (non-idempotent) mutation ...
  auto r1 = absl::make_unique<MockReader>();
  EXPECT_CALL(*r1, Read(_))
      .WillOnce(Invoke([](btproto::MutateRowsResponse* r) {
        // ... simulate a partial (recoverable) failure ...
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

  // ... the Mutator should issue a second request, with only the idempotent
  // mutation, this second request will succeed immediately ...
  auto r2 = absl::make_unique<MockReader>();
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

  // ... we want to verify the MutateRowsRaw() call has the right contents,
  // it is easier (or more readable) to write these in a separate small lambda
  // expression because ASSERT_EQ() has an embedded "return;" in it ...
  auto expect_r2 = [](btproto::MutateRowsRequest const& r) {
    ASSERT_EQ(r.entries_size(), 1);
    EXPECT_EQ(r.entries(0).row_key(), "bar");
  };
  btproto::MockBigtableStub stub;
  EXPECT_CALL(stub, MutateRowsRaw(_, _))
      .WillOnce(Invoke(
          [&r1](grpc::ClientContext*, btproto::MutateRowsRequest const& r) {
            EXPECT_EQ(r.entries_size(), 3);
            return r1.release();
          }))
      .WillOnce(Invoke(
          [&r2, expect_r2](grpc::ClientContext*,
                           btproto::MutateRowsRequest const& r) {
            expect_r2(r);
            return r2.release();
          }));

  auto policy = bt::DefaultIdempotentMutationPolicy();
  bt::detail::MultipleRowsMutator mutator("foo/bar/baz/table",
                                          *policy,
                                          std::move(mut));

  // ... we need to call make_one_request() twice of course ...
  for (int i = 0; i != 2; ++i) {
    EXPECT_TRUE(mutator.has_pending_mutations());
    grpc::ClientContext context;
    auto status = mutator.make_one_request(stub, context);
    EXPECT_TRUE(status.ok());
  }
  auto failures = mutator.extract_final_failures();
  ASSERT_EQ(failures.size(), 2UL);
  EXPECT_EQ(failures[0].original_index(), 0);
  EXPECT_EQ(failures[0].mutation().row_key(), "foo");
  EXPECT_EQ(failures[0].status().error_code(), grpc::StatusCode::UNAVAILABLE);

  EXPECT_EQ(failures[1].original_index(), 2);
  EXPECT_EQ(failures[1].mutation().row_key(), "baz");
  EXPECT_EQ(failures[1].status().error_code(), grpc::StatusCode::OK);
}
