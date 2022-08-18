// Copyright 2017 Google Inc.
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

#include "google/cloud/bigtable/internal/bulk_mutator.h"
#include "google/cloud/bigtable/testing/mock_bigtable_stub.h"
#include "google/cloud/testing_util/chrono_literals.h"
#include "google/cloud/testing_util/status_matchers.h"
#include "absl/memory/memory.h"

namespace google {
namespace cloud {
namespace bigtable {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

namespace v2 = ::google::bigtable::v2;
using ::google::cloud::testing_util::chrono_literals::operator"" _ms;  // NOLINT
using ::google::cloud::bigtable::testing::MockBigtableStub;
using ::google::cloud::bigtable::testing::MockMutateRowsStream;
using ::google::cloud::testing_util::StatusIs;
using ::testing::AnyOf;
using ::testing::Eq;
using ::testing::Matcher;
using ::testing::MockFunction;
using ::testing::Property;
using ::testing::Return;

auto const* const kAppProfile = "the-profile";
auto const* const kTableName =
    "projects/the-project/instances/the-instance/tables/the-table";

Matcher<v2::MutateRowsRequest const&> HasCorrectResourceNames() {
  return AllOf(
      Property(&v2::MutateRowsRequest::app_profile_id, Eq(kAppProfile)),
      Property(&v2::MutateRowsRequest::table_name, Eq(kTableName)));
}

bigtable::SingleRowMutation IdempotentMutation(std::string const& row) {
  return bigtable::SingleRowMutation(
      row, {bigtable::SetCell("fam", "col", 0_ms, "val")});
}

bigtable::SingleRowMutation NonIdempotentMutation(std::string const& row) {
  return bigtable::SingleRowMutation(row,
                                     {bigtable::SetCell("fam", "col", "val")});
}

// Individual entry pairs are: {index, StatusCode}
v2::MutateRowsResponse MakeResponse(
    std::vector<std::pair<int, grpc::StatusCode>> const& entries) {
  v2::MutateRowsResponse resp;
  for (auto entry : entries) {
    auto& e = *resp.add_entries();
    e.set_index(entry.first);
    e.mutable_status()->set_code(entry.second);
  }
  return resp;
}

TEST(BulkMutatorTest, Simple) {
  BulkMutation mut(IdempotentMutation("r0"), IdempotentMutation("r1"));

  auto mock = std::make_shared<MockBigtableStub>();
  EXPECT_CALL(*mock, MutateRows)
      .WillOnce([](std::unique_ptr<grpc::ClientContext>,
                   google::bigtable::v2::MutateRowsRequest const& request) {
        EXPECT_THAT(request, HasCorrectResourceNames());
        auto stream = absl::make_unique<MockMutateRowsStream>();
        EXPECT_CALL(*stream, Read)
            .WillOnce(Return(MakeResponse(
                {{0, grpc::StatusCode::OK}, {1, grpc::StatusCode::OK}})))
            .WillOnce(Return(Status()));
        return stream;
      });

  auto policy = DefaultIdempotentMutationPolicy();
  internal::BulkMutator mutator(kAppProfile, kTableName, *policy,
                                std::move(mut));

  EXPECT_TRUE(mutator.HasPendingMutations());
  auto status = mutator.MakeOneRequest(*mock);
  EXPECT_STATUS_OK(status);
  auto failures = std::move(mutator).OnRetryDone();
  EXPECT_TRUE(failures.empty());
}

TEST(BulkMutatorTest, RetryPartialFailure) {
  // In this test we create a Mutation for two rows, one of which will fail.
  // First create the mutation.
  BulkMutation mut(IdempotentMutation("r0"), IdempotentMutation("r1"));

  auto mock = std::make_shared<MockBigtableStub>();
  EXPECT_CALL(*mock, MutateRows)
      // Prepare the mocks for the request.  First create a stream response
      // which indicates a partial failure.
      .WillOnce([](std::unique_ptr<grpc::ClientContext>,
                   google::bigtable::v2::MutateRowsRequest const& request) {
        EXPECT_THAT(request, HasCorrectResourceNames());
        auto stream = absl::make_unique<MockMutateRowsStream>();
        EXPECT_CALL(*stream, Read)
            .WillOnce(Return(MakeResponse({{0, grpc::StatusCode::UNAVAILABLE},
                                           {1, grpc::StatusCode::OK}})))
            .WillOnce(Return(Status()));
        return stream;
      })
      // Prepare a second stream response, because the client should retry after
      // the partial failure.
      .WillOnce([](std::unique_ptr<grpc::ClientContext>,
                   google::bigtable::v2::MutateRowsRequest const& request) {
        EXPECT_THAT(request, HasCorrectResourceNames());
        auto stream = absl::make_unique<MockMutateRowsStream>();
        EXPECT_CALL(*stream, Read)
            .WillOnce(Return(MakeResponse({{0, grpc::StatusCode::OK}})))
            .WillOnce(Return(Status()));
        return stream;
      });

  auto policy = DefaultIdempotentMutationPolicy();
  internal::BulkMutator mutator(kAppProfile, kTableName, *policy,
                                std::move(mut));

  // This work will be in BulkApply(), but this is the test for BulkMutator in
  // isolation, so call MakeOneRequest() twice, for the r1, and the r2 cases.
  for (int i = 0; i != 2; ++i) {
    EXPECT_TRUE(mutator.HasPendingMutations());
    auto status = mutator.MakeOneRequest(*mock);
    EXPECT_STATUS_OK(status);
  }
  auto failures = std::move(mutator).OnRetryDone();
  EXPECT_TRUE(failures.empty());
}

TEST(BulkMutatorTest, PermanentFailure) {
  // In this test we handle a recoverable and one unrecoverable failures.
  // Create a bulk mutation with two SetCell() mutations.
  BulkMutation mut(IdempotentMutation("r0"), IdempotentMutation("r1"));

  auto mock = std::make_shared<MockBigtableStub>();
  EXPECT_CALL(*mock, MutateRows)
      // The first RPC return one recoverable and one unrecoverable failure.
      .WillOnce([](std::unique_ptr<grpc::ClientContext>,
                   google::bigtable::v2::MutateRowsRequest const& request) {
        EXPECT_THAT(request, HasCorrectResourceNames());
        auto stream = absl::make_unique<MockMutateRowsStream>();
        EXPECT_CALL(*stream, Read)
            .WillOnce(
                Return(MakeResponse({{0, grpc::StatusCode::UNAVAILABLE},
                                     {1, grpc::StatusCode::OUT_OF_RANGE}})))
            .WillOnce(Return(Status()));
        return stream;
      })
      // The BulkMutator should issue a second request, which will return
      // success for the remaining mutation.
      .WillOnce([](std::unique_ptr<grpc::ClientContext>,
                   google::bigtable::v2::MutateRowsRequest const& request) {
        EXPECT_THAT(request, HasCorrectResourceNames());
        auto stream = absl::make_unique<MockMutateRowsStream>();
        EXPECT_CALL(*stream, Read)
            .WillOnce(Return(MakeResponse({{0, grpc::StatusCode::OK}})))
            .WillOnce(Return(Status()));
        return stream;
      });

  auto policy = DefaultIdempotentMutationPolicy();
  internal::BulkMutator mutator(kAppProfile, kTableName, *policy,
                                std::move(mut));

  // This work will be in BulkApply(), but this is the test for BulkMutator in
  // isolation, so call MakeOneRequest() twice, for the r1, and the r2 cases.
  for (int i = 0; i != 2; ++i) {
    EXPECT_TRUE(mutator.HasPendingMutations());
    auto status = mutator.MakeOneRequest(*mock);
    EXPECT_STATUS_OK(status);
  }
  auto failures = std::move(mutator).OnRetryDone();
  ASSERT_EQ(1UL, failures.size());
  EXPECT_EQ(1, failures[0].original_index());
  EXPECT_THAT(failures[0].status(), StatusIs(StatusCode::kOutOfRange));
}

TEST(BulkMutatorTest, PartialStream) {
  // We are going to test the case where the stream does not contain a response
  // for all requests.  Create a BulkMutation with two entries.
  BulkMutation mut(IdempotentMutation("r0"), IdempotentMutation("r1"));

  auto mock = std::make_shared<MockBigtableStub>();
  EXPECT_CALL(*mock, MutateRows)
      // This will be the stream returned by the first request.  It is missing
      // information about one of the mutations.
      .WillOnce([](std::unique_ptr<grpc::ClientContext>,
                   google::bigtable::v2::MutateRowsRequest const& request) {
        EXPECT_THAT(request, HasCorrectResourceNames());
        auto stream = absl::make_unique<MockMutateRowsStream>();
        EXPECT_CALL(*stream, Read)
            .WillOnce(Return(MakeResponse({{0, grpc::StatusCode::OK}})))
            .WillOnce(Return(Status()));
        return stream;
      })
      // The BulkMutation should issue a second request, this is the stream
      // returned by the second request, which indicates success for the missed
      // mutation on r1.
      .WillOnce([](std::unique_ptr<grpc::ClientContext>,
                   google::bigtable::v2::MutateRowsRequest const& request) {
        EXPECT_THAT(request, HasCorrectResourceNames());
        auto stream = absl::make_unique<MockMutateRowsStream>();
        EXPECT_CALL(*stream, Read)
            .WillOnce(Return(MakeResponse({{0, grpc::StatusCode::OK}})))
            .WillOnce(Return(Status()));
        return stream;
      });

  auto policy = DefaultIdempotentMutationPolicy();
  internal::BulkMutator mutator(kAppProfile, kTableName, *policy,
                                std::move(mut));

  // This work will be in BulkApply(), but this is the test for BulkMutator in
  // isolation, so call MakeOneRequest() twice: for the r1 and r2 cases.
  for (int i = 0; i != 2; ++i) {
    EXPECT_TRUE(mutator.HasPendingMutations());
    auto status = mutator.MakeOneRequest(*mock);
    EXPECT_STATUS_OK(status);
  }
  auto failures = std::move(mutator).OnRetryDone();
  EXPECT_TRUE(failures.empty());
}

TEST(BulkMutatorTest, RetryOnlyIdempotent) {
  // Create a BulkMutation with a non-idempotent mutation.
  BulkMutation mut(NonIdempotentMutation("r0"),
                   IdempotentMutation("r1-retried"));

  // Verify that the second response has the right contents.  It is easier (and
  // more readable) to write these in a separate small lambda expression because
  // ASSERT_EQ() has an embedded "return;" in it, which does not play well with
  // the rest of the stuff here.
  auto expect_r2 = [](v2::MutateRowsRequest const& r) {
    ASSERT_EQ(1, r.entries_size());
    EXPECT_EQ("r1-retried", r.entries(0).row_key());
  };

  auto mock = std::make_shared<MockBigtableStub>();
  EXPECT_CALL(*mock, MutateRows)
      // We will setup the mock to return recoverable transient errors for all
      // mutations.
      .WillOnce([](std::unique_ptr<grpc::ClientContext>,
                   google::bigtable::v2::MutateRowsRequest const& request) {
        EXPECT_THAT(request, HasCorrectResourceNames());
        EXPECT_EQ(2, request.entries_size());
        auto stream = absl::make_unique<MockMutateRowsStream>();
        EXPECT_CALL(*stream, Read)
            .WillOnce(
                Return(MakeResponse({{0, grpc::StatusCode::UNAVAILABLE},
                                     {1, grpc::StatusCode::UNAVAILABLE}})))
            .WillOnce(Return(Status()));
        return stream;
      })
      // The BulkMutator should issue a second request, with only the
      // idempotent mutation. Make the mock return success for it.
      .WillOnce(
          [expect_r2](std::unique_ptr<grpc::ClientContext>,
                      google::bigtable::v2::MutateRowsRequest const& request) {
            EXPECT_THAT(request, HasCorrectResourceNames());
            expect_r2(request);
            auto stream = absl::make_unique<MockMutateRowsStream>();
            EXPECT_CALL(*stream, Read)
                .WillOnce(Return(MakeResponse({{0, grpc::StatusCode::OK}})))
                .WillOnce(Return(Status()));
            return stream;
          });

  auto policy = DefaultIdempotentMutationPolicy();
  internal::BulkMutator mutator(kAppProfile, kTableName, *policy,
                                std::move(mut));

  // This work will be in BulkApply(), but this is the test for BulkMutator in
  // isolation, so call MakeOneRequest() twice, for the r1, and the r2 cases.
  for (int i = 0; i != 2; ++i) {
    EXPECT_TRUE(mutator.HasPendingMutations());
    auto status = mutator.MakeOneRequest(*mock);
    EXPECT_STATUS_OK(status);
  }
  auto failures = std::move(mutator).OnRetryDone();
  ASSERT_EQ(1UL, failures.size());
  EXPECT_EQ(0, failures[0].original_index());
  EXPECT_THAT(failures[0].status(), StatusIs(StatusCode::kUnavailable));
}

TEST(BulkMutatorTest, UnconfirmedAreFailed) {
  // Make sure that mutations which are not confirmed are reported as UNKNOWN
  // with the proper index.
  BulkMutation mut(NonIdempotentMutation("r0"),
                   NonIdempotentMutation("r1-unconfirmed"),
                   NonIdempotentMutation("r2"));

  auto mock = std::make_shared<MockBigtableStub>();
  EXPECT_CALL(*mock, MutateRows)
      // We will setup the mock to return recoverable failures for idempotent
      // mutations.
      .WillOnce([](std::unique_ptr<grpc::ClientContext>,
                   google::bigtable::v2::MutateRowsRequest const& request) {
        EXPECT_THAT(request, HasCorrectResourceNames());
        EXPECT_EQ(3, request.entries_size());
        auto stream = absl::make_unique<MockMutateRowsStream>();
        EXPECT_CALL(*stream, Read)
            .WillOnce(Return(MakeResponse(
                {{0, grpc::StatusCode::OK}, {2, grpc::StatusCode::OK}})))
            .WillOnce(Return(Status(StatusCode::kPermissionDenied, "fail")));
        return stream;
      });
  // The BulkMutator should not issue a second request because the error is
  // PERMISSION_DENIED (not retryable).

  auto policy = DefaultIdempotentMutationPolicy();
  internal::BulkMutator mutator(kAppProfile, kTableName, *policy,
                                std::move(mut));

  EXPECT_TRUE(mutator.HasPendingMutations());
  auto status = mutator.MakeOneRequest(*mock);
  EXPECT_THAT(status, StatusIs(StatusCode::kPermissionDenied));

  auto failures = std::move(mutator).OnRetryDone();
  ASSERT_EQ(1UL, failures.size());
  EXPECT_EQ(1, failures[0].original_index());
  EXPECT_THAT(failures[0].status(), StatusIs(StatusCode::kPermissionDenied));
}

TEST(BulkMutatorTest, ConfiguresContext) {
  BulkMutation mut(IdempotentMutation("r0"));

  auto mock = std::make_shared<MockBigtableStub>();
  EXPECT_CALL(*mock, MutateRows)
      .WillOnce([](std::unique_ptr<grpc::ClientContext>,
                   google::bigtable::v2::MutateRowsRequest const& request) {
        EXPECT_THAT(request, HasCorrectResourceNames());
        auto stream = absl::make_unique<MockMutateRowsStream>();
        EXPECT_CALL(*stream, Read).WillOnce(Return(Status()));
        return stream;
      });

  auto policy = DefaultIdempotentMutationPolicy();
  internal::BulkMutator mutator(kAppProfile, kTableName, *policy,
                                std::move(mut));

  MockFunction<void(grpc::ClientContext&)> mock_setup;
  EXPECT_CALL(mock_setup, Call).Times(1);

  google::cloud::internal::OptionsSpan span(
      Options{}.set<google::cloud::internal::GrpcSetupOption>(
          mock_setup.AsStdFunction()));
  (void)mutator.MakeOneRequest(*mock);
}

TEST(BulkMutatorTest, MutationStatusReportedOnOkStream) {
  BulkMutation mut(IdempotentMutation("r0"), IdempotentMutation("r1"));

  auto mock = std::make_shared<MockBigtableStub>();
  EXPECT_CALL(*mock, MutateRows)
      .WillOnce([](std::unique_ptr<grpc::ClientContext>,
                   google::bigtable::v2::MutateRowsRequest const& request) {
        EXPECT_THAT(request, HasCorrectResourceNames());
        auto stream = absl::make_unique<MockMutateRowsStream>();
        EXPECT_CALL(*stream, Read)
            .WillOnce(
                Return(MakeResponse({{0, grpc::StatusCode::UNAVAILABLE}})))
            .WillOnce(Return(Status()));
        return stream;
      });

  auto policy = DefaultIdempotentMutationPolicy();
  internal::BulkMutator mutator(kAppProfile, kTableName, *policy,
                                std::move(mut));

  auto status = mutator.MakeOneRequest(*mock);
  EXPECT_STATUS_OK(status);

  auto failures = std::move(mutator).OnRetryDone();
  ASSERT_EQ(2UL, failures.size());
  // This mutation failed, although the stream succeeded. We should report the
  // mutation status.
  EXPECT_EQ(0, failures[0].original_index());
  EXPECT_THAT(failures[0].status(), StatusIs(StatusCode::kUnavailable));
  // The stream was OK, but it did not contain this mutation. Something has gone
  // wrong, so we should report an INTERNAL error.
  EXPECT_EQ(1, failures[1].original_index());
  EXPECT_THAT(failures[1].status(), StatusIs(StatusCode::kInternal));
}

TEST(BulkMutatorTest, ReportEitherRetryableMutationFailOrStreamFail) {
  BulkMutation mut(IdempotentMutation("r0"));

  auto mock = std::make_shared<MockBigtableStub>();
  EXPECT_CALL(*mock, MutateRows)
      .WillOnce([](std::unique_ptr<grpc::ClientContext>,
                   google::bigtable::v2::MutateRowsRequest const& request) {
        EXPECT_THAT(request, HasCorrectResourceNames());
        auto stream = absl::make_unique<MockMutateRowsStream>();
        EXPECT_CALL(*stream, Read)
            .WillOnce(
                Return(MakeResponse({{0, grpc::StatusCode::UNAVAILABLE}})))
            .WillOnce(Return(Status(StatusCode::kDataLoss, "stream fail")));
        return stream;
      });

  auto policy = DefaultIdempotentMutationPolicy();
  internal::BulkMutator mutator(kAppProfile, kTableName, *policy,
                                std::move(mut));

  auto status = mutator.MakeOneRequest(*mock);
  EXPECT_THAT(status, StatusIs(StatusCode::kDataLoss));

  auto failures = std::move(mutator).OnRetryDone();
  ASSERT_EQ(1UL, failures.size());
  EXPECT_EQ(0, failures[0].original_index());
  // The mutation fails for one reason, and the stream fails for another. As far
  // as I am concerned, both are valid errors to report. The contract of the
  // code does not need to be stricter than this.
  EXPECT_THAT(failures[0].status(),
              StatusIs(AnyOf(StatusCode::kUnavailable, StatusCode::kDataLoss)));
}

TEST(BulkMutatorTest, ReportOnlyLatestMutationStatus) {
  // In this test, the mutation fails with an ABORTED status in the first
  // response. It is not included in the second response. We should report the
  // final stream failure for this mutation, as it is the more informative
  // error.
  BulkMutation mut(IdempotentMutation("r0"));

  auto mock = std::make_shared<MockBigtableStub>();
  EXPECT_CALL(*mock, MutateRows)
      .WillOnce([](std::unique_ptr<grpc::ClientContext>,
                   google::bigtable::v2::MutateRowsRequest const& request) {
        EXPECT_THAT(request, HasCorrectResourceNames());
        auto stream = absl::make_unique<MockMutateRowsStream>();
        EXPECT_CALL(*stream, Read)
            .WillOnce(Return(MakeResponse({{0, grpc::StatusCode::ABORTED}})))
            .WillOnce(Return(Status(StatusCode::kUnavailable, "try again")));
        return stream;
      })
      .WillOnce([](std::unique_ptr<grpc::ClientContext>,
                   google::bigtable::v2::MutateRowsRequest const& request) {
        EXPECT_THAT(request, HasCorrectResourceNames());
        auto stream = absl::make_unique<MockMutateRowsStream>();
        EXPECT_CALL(*stream, Read)
            .WillOnce(Return(Status(StatusCode::kDataLoss, "fail")));
        return stream;
      });

  auto policy = DefaultIdempotentMutationPolicy();
  internal::BulkMutator mutator(kAppProfile, kTableName, *policy,
                                std::move(mut));

  auto status = mutator.MakeOneRequest(*mock);
  EXPECT_THAT(status, StatusIs(StatusCode::kUnavailable));

  status = mutator.MakeOneRequest(*mock);
  EXPECT_THAT(status, StatusIs(StatusCode::kDataLoss));

  auto failures = std::move(mutator).OnRetryDone();
  ASSERT_EQ(1UL, failures.size());
  EXPECT_EQ(0, failures[0].original_index());
  EXPECT_THAT(failures[0].status(), StatusIs(StatusCode::kDataLoss));
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable
}  // namespace cloud
}  // namespace google
