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

#include "google/cloud/bigtable/internal/bulk_mutator.h"
#include "google/cloud/bigtable/internal/operation_context.h"
#ifdef GOOGLE_CLOUD_CPP_BIGTABLE_WITH_OTEL_METRICS
#include "google/cloud/bigtable/internal/metrics.h"
#include "google/cloud/testing_util/fake_clock.h"
#endif
#include "google/cloud/bigtable/testing/mock_bigtable_stub.h"
#include "google/cloud/bigtable/testing/mock_mutate_rows_limiter.h"
#include "google/cloud/internal/make_status.h"
#include "google/cloud/testing_util/chrono_literals.h"
#include "google/cloud/testing_util/status_matchers.h"
#include "google/cloud/testing_util/validate_metadata.h"

namespace google {
namespace cloud {
namespace bigtable {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

namespace v2 = ::google::bigtable::v2;
using ::google::cloud::testing_util::chrono_literals::operator""_ms;
using ::google::cloud::bigtable::testing::MockBigtableStub;
using ::google::cloud::bigtable::testing::MockMutateRowsLimiter;
using ::google::cloud::bigtable::testing::MockMutateRowsStream;
using ::google::cloud::testing_util::StatusIs;
using ::testing::AnyOf;
using ::testing::Contains;
using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::IsEmpty;
using ::testing::Matcher;
using ::testing::MockFunction;
using ::testing::Pair;
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

#ifdef GOOGLE_CLOUD_CPP_BIGTABLE_WITH_OTEL_METRICS

class MockMetric : public bigtable_internal::Metric {
 public:
  MOCK_METHOD(void, PreCall,
              (opentelemetry::context::Context const&,
               bigtable_internal::PreCallParams const&),
              (override));
  MOCK_METHOD(void, PostCall,
              (opentelemetry::context::Context const&,
               grpc::ClientContext const&,
               bigtable_internal::PostCallParams const&),
              (override));
  MOCK_METHOD(void, OnDone,
              (opentelemetry::context::Context const&,
               bigtable_internal::OnDoneParams const&),
              (override));
  MOCK_METHOD(void, ElementRequest,
              (opentelemetry::context::Context const&,
               bigtable_internal::ElementRequestParams const&),
              (override));
  MOCK_METHOD(void, ElementDelivery,
              (opentelemetry::context::Context const&,
               bigtable_internal::ElementDeliveryParams const&),
              (override));
  MOCK_METHOD(std::unique_ptr<Metric>, clone,
              (bigtable_internal::ResourceLabels resource_labels,
               bigtable_internal::DataLabels data_labels),
              (const, override));
};

// This class is a vehicle to get a MockMetric into the OperationContext object.
class CloningMetric : public bigtable_internal::Metric {
 public:
  explicit CloningMetric(std::unique_ptr<MockMetric> metric)
      : metric_(std::move(metric)) {}
  std::unique_ptr<bigtable_internal::Metric> clone(
      bigtable_internal::ResourceLabels,
      bigtable_internal::DataLabels) const override {
    return std::move(metric_);
  }

 private:
  mutable std::unique_ptr<MockMetric> metric_;
};

#endif  // GOOGLE_CLOUD_CPP_BIGTABLE_WITH_OTEL_METRICS

class BulkMutatorTest : public ::testing::Test {
 protected:
  testing_util::ValidateMetadataFixture metadata_fixture_;
};

TEST_F(BulkMutatorTest, Simple) {
  BulkMutation mut(IdempotentMutation("r0"), IdempotentMutation("r1"));
#ifdef GOOGLE_CLOUD_CPP_BIGTABLE_WITH_OTEL_METRICS
  auto mock_metric = std::make_unique<MockMetric>();
  EXPECT_CALL(*mock_metric, PreCall).Times(1);
  EXPECT_CALL(*mock_metric, PostCall).Times(1);

  auto fake_metric = std::make_shared<CloningMetric>(std::move(mock_metric));
  auto clock = std::make_shared<testing_util::FakeSteadyClock>();

  // Normally std::make_shared would be used here, but some weird type deduction
  // is preventing it.
  // NOLINTNEXTLINE(modernize-make-shared)
  auto operation_context = std::shared_ptr<bigtable_internal::OperationContext>(
      new bigtable_internal::OperationContext({}, {}, {fake_metric}, clock));
#else
  auto operation_context =
      std::make_shared<bigtable_internal::OperationContext>();
#endif
  auto mock = std::make_shared<MockBigtableStub>();
  EXPECT_CALL(*mock, MutateRows)
      .WillOnce([this](auto context, auto const&,
                       google::bigtable::v2::MutateRowsRequest const& request) {
        metadata_fixture_.SetServerMetadata(*context, {});
        EXPECT_THAT(request, HasCorrectResourceNames());
        auto stream = std::make_unique<MockMutateRowsStream>();
        EXPECT_CALL(*stream, Read)
            .WillOnce(Return(MakeResponse(
                {{0, grpc::StatusCode::OK}, {1, grpc::StatusCode::OK}})))
            .WillOnce(Return(Status()));
        return stream;
      });

  auto policy = DefaultIdempotentMutationPolicy();
  bigtable_internal::BulkMutator mutator(kAppProfile, kTableName, *policy,
                                         std::move(mut),
                                         std::move(operation_context));

  EXPECT_TRUE(mutator.HasPendingMutations());
  bigtable_internal::NoopMutateRowsLimiter limiter;
  auto status = mutator.MakeOneRequest(*mock, limiter, Options{});
  EXPECT_STATUS_OK(status);
  auto failures = std::move(mutator).OnRetryDone();
  EXPECT_TRUE(failures.empty());
}

TEST_F(BulkMutatorTest, RetryPartialFailure) {
  // In this test we create a Mutation for two rows, one of which will fail.
  // First create the mutation.
  BulkMutation mut(IdempotentMutation("r0"), IdempotentMutation("r1"));
#ifdef GOOGLE_CLOUD_CPP_BIGTABLE_WITH_OTEL_METRICS
  auto mock_metric = std::make_unique<MockMetric>();
  EXPECT_CALL(*mock_metric, PreCall).Times(2);
  EXPECT_CALL(*mock_metric, PostCall).Times(2);

  auto fake_metric = std::make_shared<CloningMetric>(std::move(mock_metric));
  auto clock = std::make_shared<testing_util::FakeSteadyClock>();

  // Normally std::make_shared would be used here, but some weird type deduction
  // is preventing it.
  // NOLINTNEXTLINE(modernize-make-shared)
  auto operation_context = std::shared_ptr<bigtable_internal::OperationContext>(
      new bigtable_internal::OperationContext({}, {}, {fake_metric}, clock));
#else
  auto operation_context =
      std::make_shared<bigtable_internal::OperationContext>();
#endif

  auto mock = std::make_shared<MockBigtableStub>();
  EXPECT_CALL(*mock, MutateRows)
      // Prepare the mocks for the request.  First create a stream response
      // which indicates a partial failure.
      .WillOnce([this](auto context, auto const&,
                       google::bigtable::v2::MutateRowsRequest const& request) {
        metadata_fixture_.SetServerMetadata(*context, {});
        EXPECT_THAT(request, HasCorrectResourceNames());
        auto stream = std::make_unique<MockMutateRowsStream>();
        EXPECT_CALL(*stream, Read)
            .WillOnce(Return(MakeResponse({{0, grpc::StatusCode::UNAVAILABLE},
                                           {1, grpc::StatusCode::OK}})))
            .WillOnce(Return(Status()));
        return stream;
      })
      // Prepare a second stream response, because the client should retry after
      // the partial failure.
      .WillOnce([this](auto context, auto const&,
                       google::bigtable::v2::MutateRowsRequest const& request) {
        metadata_fixture_.SetServerMetadata(*context, {});
        EXPECT_THAT(request, HasCorrectResourceNames());
        auto stream = std::make_unique<MockMutateRowsStream>();
        EXPECT_CALL(*stream, Read)
            .WillOnce(Return(MakeResponse({{0, grpc::StatusCode::OK}})))
            .WillOnce(Return(Status()));
        return stream;
      });

  auto policy = DefaultIdempotentMutationPolicy();
  bigtable_internal::BulkMutator mutator(kAppProfile, kTableName, *policy,
                                         std::move(mut),
                                         std::move(operation_context));

  // This work will be in BulkApply(), but this is the test for BulkMutator in
  // isolation, so call MakeOneRequest() twice, for the r1, and the r2 cases.
  for (int i = 0; i != 2; ++i) {
    EXPECT_TRUE(mutator.HasPendingMutations());
    bigtable_internal::NoopMutateRowsLimiter limiter;
    auto status = mutator.MakeOneRequest(*mock, limiter, Options{});
    EXPECT_STATUS_OK(status);
  }
  auto failures = std::move(mutator).OnRetryDone();
  EXPECT_TRUE(failures.empty());
}

TEST_F(BulkMutatorTest, PermanentFailure) {
  // In this test we handle a recoverable and one unrecoverable failures.
  // Create a bulk mutation with two SetCell() mutations.
  BulkMutation mut(IdempotentMutation("r0"), IdempotentMutation("r1"));

#ifdef GOOGLE_CLOUD_CPP_BIGTABLE_WITH_OTEL_METRICS
  auto mock_metric = std::make_unique<MockMetric>();
  EXPECT_CALL(*mock_metric, PreCall).Times(2);
  EXPECT_CALL(*mock_metric, PostCall).Times(2);

  auto fake_metric = std::make_shared<CloningMetric>(std::move(mock_metric));
  auto clock = std::make_shared<testing_util::FakeSteadyClock>();

  // Normally std::make_shared would be used here, but some weird type deduction
  // is preventing it.
  // NOLINTNEXTLINE(modernize-make-shared)
  auto operation_context = std::shared_ptr<bigtable_internal::OperationContext>(
      new bigtable_internal::OperationContext({}, {}, {fake_metric}, clock));
#else
  auto operation_context =
      std::make_shared<bigtable_internal::OperationContext>();
#endif
  auto mock = std::make_shared<MockBigtableStub>();
  EXPECT_CALL(*mock, MutateRows)
      // The first RPC return one recoverable and one unrecoverable failure.
      .WillOnce([this](auto context, auto const&,
                       google::bigtable::v2::MutateRowsRequest const& request) {
        metadata_fixture_.SetServerMetadata(*context, {});
        EXPECT_THAT(request, HasCorrectResourceNames());
        auto stream = std::make_unique<MockMutateRowsStream>();
        EXPECT_CALL(*stream, Read)
            .WillOnce(
                Return(MakeResponse({{0, grpc::StatusCode::UNAVAILABLE},
                                     {1, grpc::StatusCode::OUT_OF_RANGE}})))
            .WillOnce(Return(Status()));
        return stream;
      })
      // The BulkMutator should issue a second request, which will return
      // success for the remaining mutation.
      .WillOnce([this](auto context, auto const&,
                       google::bigtable::v2::MutateRowsRequest const& request) {
        metadata_fixture_.SetServerMetadata(*context, {});
        EXPECT_THAT(request, HasCorrectResourceNames());
        auto stream = std::make_unique<MockMutateRowsStream>();
        EXPECT_CALL(*stream, Read)
            .WillOnce(Return(MakeResponse({{0, grpc::StatusCode::OK}})))
            .WillOnce(Return(Status()));
        return stream;
      });

  auto policy = DefaultIdempotentMutationPolicy();
  bigtable_internal::BulkMutator mutator(kAppProfile, kTableName, *policy,
                                         std::move(mut),
                                         std::move(operation_context));

  // This work will be in BulkApply(), but this is the test for BulkMutator in
  // isolation, so call MakeOneRequest() twice, for the r1, and the r2 cases.
  for (int i = 0; i != 2; ++i) {
    EXPECT_TRUE(mutator.HasPendingMutations());
    bigtable_internal::NoopMutateRowsLimiter limiter;
    auto status = mutator.MakeOneRequest(*mock, limiter, Options{});
    EXPECT_STATUS_OK(status);
  }
  auto failures = std::move(mutator).OnRetryDone();
  ASSERT_EQ(1UL, failures.size());
  EXPECT_EQ(1, failures[0].original_index());
  EXPECT_THAT(failures[0].status(), StatusIs(StatusCode::kOutOfRange));
}

TEST_F(BulkMutatorTest, PartialStream) {
  // We are going to test the case where the stream does not contain a response
  // for all requests.  Create a BulkMutation with two entries.
  BulkMutation mut(IdempotentMutation("r0"), IdempotentMutation("r1"));

#ifdef GOOGLE_CLOUD_CPP_BIGTABLE_WITH_OTEL_METRICS
  auto mock_metric = std::make_unique<MockMetric>();
  EXPECT_CALL(*mock_metric, PreCall).Times(2);
  EXPECT_CALL(*mock_metric, PostCall).Times(2);

  auto fake_metric = std::make_shared<CloningMetric>(std::move(mock_metric));
  auto clock = std::make_shared<testing_util::FakeSteadyClock>();

  // Normally std::make_shared would be used here, but some weird type deduction
  // is preventing it.
  // NOLINTNEXTLINE(modernize-make-shared)
  auto operation_context = std::shared_ptr<bigtable_internal::OperationContext>(
      new bigtable_internal::OperationContext({}, {}, {fake_metric}, clock));
#else
  auto operation_context =
      std::make_shared<bigtable_internal::OperationContext>();
#endif
  auto mock = std::make_shared<MockBigtableStub>();
  EXPECT_CALL(*mock, MutateRows)
      // This will be the stream returned by the first request.  It is missing
      // information about one of the mutations.
      .WillOnce([this](auto context, auto const&,
                       google::bigtable::v2::MutateRowsRequest const& request) {
        metadata_fixture_.SetServerMetadata(*context, {});
        EXPECT_THAT(request, HasCorrectResourceNames());
        auto stream = std::make_unique<MockMutateRowsStream>();
        EXPECT_CALL(*stream, Read)
            .WillOnce(Return(MakeResponse({{0, grpc::StatusCode::OK}})))
            .WillOnce(Return(Status()));
        return stream;
      })
      // The BulkMutation should issue a second request, this is the stream
      // returned by the second request, which indicates success for the missed
      // mutation on r1.
      .WillOnce([this](auto context, auto const&,
                       google::bigtable::v2::MutateRowsRequest const& request) {
        metadata_fixture_.SetServerMetadata(*context, {});
        EXPECT_THAT(request, HasCorrectResourceNames());
        auto stream = std::make_unique<MockMutateRowsStream>();
        EXPECT_CALL(*stream, Read)
            .WillOnce(Return(MakeResponse({{0, grpc::StatusCode::OK}})))
            .WillOnce(Return(Status()));
        return stream;
      });

  auto policy = DefaultIdempotentMutationPolicy();
  bigtable_internal::BulkMutator mutator(kAppProfile, kTableName, *policy,
                                         std::move(mut),
                                         std::move(operation_context));

  // This work will be in BulkApply(), but this is the test for BulkMutator in
  // isolation, so call MakeOneRequest() twice: for the r1 and r2 cases.
  for (int i = 0; i != 2; ++i) {
    EXPECT_TRUE(mutator.HasPendingMutations());
    bigtable_internal::NoopMutateRowsLimiter limiter;
    auto status = mutator.MakeOneRequest(*mock, limiter, Options{});
    EXPECT_STATUS_OK(status);
  }
  auto failures = std::move(mutator).OnRetryDone();
  EXPECT_TRUE(failures.empty());
}

TEST_F(BulkMutatorTest, RetryOnlyIdempotent) {
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
      .WillOnce([this](auto context, auto const&,
                       google::bigtable::v2::MutateRowsRequest const& request) {
        metadata_fixture_.SetServerMetadata(*context, {});
        EXPECT_THAT(request, HasCorrectResourceNames());
        EXPECT_EQ(2, request.entries_size());
        auto stream = std::make_unique<MockMutateRowsStream>();
        EXPECT_CALL(*stream, Read)
            .WillOnce(
                Return(MakeResponse({{0, grpc::StatusCode::UNAVAILABLE},
                                     {1, grpc::StatusCode::UNAVAILABLE}})))
            .WillOnce(Return(Status()));
        return stream;
      })
      // The BulkMutator should issue a second request, with only the
      // idempotent mutation. Make the mock return success for it.
      .WillOnce([this, expect_r2](
                    auto context, auto const&,
                    google::bigtable::v2::MutateRowsRequest const& request) {
        metadata_fixture_.SetServerMetadata(*context, {});
        EXPECT_THAT(request, HasCorrectResourceNames());
        expect_r2(request);
        auto stream = std::make_unique<MockMutateRowsStream>();
        EXPECT_CALL(*stream, Read)
            .WillOnce(Return(MakeResponse({{0, grpc::StatusCode::OK}})))
            .WillOnce(Return(Status()));
        return stream;
      });

  auto policy = DefaultIdempotentMutationPolicy();
  bigtable_internal::BulkMutator mutator(
      kAppProfile, kTableName, *policy, std::move(mut),
      std::make_shared<bigtable_internal::OperationContext>());

  // This work will be in BulkApply(), but this is the test for BulkMutator in
  // isolation, so call MakeOneRequest() twice, for the r1, and the r2 cases.
  for (int i = 0; i != 2; ++i) {
    EXPECT_TRUE(mutator.HasPendingMutations());
    bigtable_internal::NoopMutateRowsLimiter limiter;
    auto status = mutator.MakeOneRequest(*mock, limiter, Options{});
    EXPECT_STATUS_OK(status);
  }
  auto failures = std::move(mutator).OnRetryDone();
  ASSERT_EQ(1UL, failures.size());
  EXPECT_EQ(0, failures[0].original_index());
  EXPECT_THAT(failures[0].status(), StatusIs(StatusCode::kUnavailable));
}

TEST_F(BulkMutatorTest, RetryInfoHeeded) {
  // Create a BulkMutation with a non-idempotent mutation.
  BulkMutation mut(NonIdempotentMutation("row"));

  auto mock = std::make_shared<MockBigtableStub>();
  EXPECT_CALL(*mock, MutateRows)
      .WillOnce([this](auto context, auto const&,
                       google::bigtable::v2::MutateRowsRequest const&) {
        metadata_fixture_.SetServerMetadata(*context, {});
        auto status =
            google::cloud::internal::ResourceExhaustedError("try again");
        google::cloud::internal::SetRetryInfo(
            status, google::cloud::internal::RetryInfo{0_ms});
        auto stream = std::make_unique<MockMutateRowsStream>();
        EXPECT_CALL(*stream, Read).WillOnce(Return(status));
        return stream;
      })
      // By supplying a `RetryInfo` in the error details, the server is telling
      // us that it is safe to retry the mutation, even though it is not
      // idempotent.
      .WillOnce([this](auto context, auto const&,
                       google::bigtable::v2::MutateRowsRequest const& request) {
        metadata_fixture_.SetServerMetadata(*context, {});
        std::vector<RowKeyType> row_keys;
        for (auto const& entry : request.entries()) {
          row_keys.push_back(entry.row_key());
        }
        EXPECT_THAT(row_keys, ElementsAre("row"));
        auto stream = std::make_unique<MockMutateRowsStream>();
        EXPECT_CALL(*stream, Read)
            .WillOnce(Return(MakeResponse({{0, grpc::StatusCode::OK}})))
            .WillOnce(Return(Status()));
        return stream;
      });

  auto policy = DefaultIdempotentMutationPolicy();
  bigtable_internal::BulkMutator mutator(
      kAppProfile, kTableName, *policy, std::move(mut),
      std::make_shared<bigtable_internal::OperationContext>());

  for (int i = 0; i != 2; ++i) {
    EXPECT_TRUE(mutator.HasPendingMutations());
    bigtable_internal::NoopMutateRowsLimiter limiter;
    (void)mutator.MakeOneRequest(
        *mock, limiter, Options{}.set<EnableServerRetriesOption>(true));
  }
  auto failures = std::move(mutator).OnRetryDone();
  EXPECT_THAT(failures, IsEmpty());
}

TEST_F(BulkMutatorTest, RetryInfoIgnored) {
  // Create a BulkMutation with a non-idempotent mutation.
  BulkMutation mut(NonIdempotentMutation("row"));

  auto mock = std::make_shared<MockBigtableStub>();
  EXPECT_CALL(*mock, MutateRows)
      .WillOnce([this](auto context, auto const&,
                       google::bigtable::v2::MutateRowsRequest const&) {
        metadata_fixture_.SetServerMetadata(*context, {});
        auto status =
            google::cloud::internal::ResourceExhaustedError("try again");
        google::cloud::internal::SetRetryInfo(
            status, google::cloud::internal::RetryInfo{0_ms});
        auto stream = std::make_unique<MockMutateRowsStream>();
        EXPECT_CALL(*stream, Read).WillOnce(Return(status));
        return stream;
      });

  auto policy = DefaultIdempotentMutationPolicy();
  bigtable_internal::BulkMutator mutator(
      kAppProfile, kTableName, *policy, std::move(mut),
      std::make_shared<bigtable_internal::OperationContext>());

  EXPECT_TRUE(mutator.HasPendingMutations());
  bigtable_internal::NoopMutateRowsLimiter limiter;
  auto status = mutator.MakeOneRequest(
      *mock, limiter, Options{}.set<EnableServerRetriesOption>(false));
  EXPECT_THAT(status, StatusIs(StatusCode::kResourceExhausted));
  EXPECT_FALSE(mutator.HasPendingMutations());
  auto failures = std::move(mutator).OnRetryDone();
  EXPECT_THAT(failures, ElementsAre(FailedMutation(status, 0)));
}

TEST_F(BulkMutatorTest, UnconfirmedAreFailed) {
  // Make sure that mutations which are not confirmed are reported as UNKNOWN
  // with the proper index.
  BulkMutation mut(NonIdempotentMutation("r0"),
                   NonIdempotentMutation("r1-unconfirmed"),
                   NonIdempotentMutation("r2"));

  auto mock = std::make_shared<MockBigtableStub>();
  EXPECT_CALL(*mock, MutateRows)
      // We will setup the mock to return recoverable failures for idempotent
      // mutations.
      .WillOnce([this](auto context, auto const&,
                       google::bigtable::v2::MutateRowsRequest const& request) {
        metadata_fixture_.SetServerMetadata(*context, {});
        EXPECT_THAT(request, HasCorrectResourceNames());
        EXPECT_EQ(3, request.entries_size());
        auto stream = std::make_unique<MockMutateRowsStream>();
        EXPECT_CALL(*stream, Read)
            .WillOnce(Return(MakeResponse(
                {{0, grpc::StatusCode::OK}, {2, grpc::StatusCode::OK}})))
            .WillOnce(Return(Status(StatusCode::kPermissionDenied, "fail")));
        return stream;
      });
  // The BulkMutator should not issue a second request because the error is
  // PERMISSION_DENIED (not retryable).

  auto policy = DefaultIdempotentMutationPolicy();
  bigtable_internal::BulkMutator mutator(
      kAppProfile, kTableName, *policy, std::move(mut),
      std::make_shared<bigtable_internal::OperationContext>());

  EXPECT_TRUE(mutator.HasPendingMutations());
  bigtable_internal::NoopMutateRowsLimiter limiter;
  auto status = mutator.MakeOneRequest(*mock, limiter, Options{});
  EXPECT_THAT(status, StatusIs(StatusCode::kPermissionDenied));

  auto failures = std::move(mutator).OnRetryDone();
  ASSERT_EQ(1UL, failures.size());
  EXPECT_EQ(1, failures[0].original_index());
  EXPECT_THAT(failures[0].status(), StatusIs(StatusCode::kPermissionDenied));
}

TEST_F(BulkMutatorTest, ConfiguresContext) {
  BulkMutation mut(IdempotentMutation("r0"));

  auto mock = std::make_shared<MockBigtableStub>();
  EXPECT_CALL(*mock, MutateRows)
      .WillOnce([this](auto context, auto const&,
                       google::bigtable::v2::MutateRowsRequest const& request) {
        metadata_fixture_.SetServerMetadata(*context, {});
        EXPECT_THAT(request, HasCorrectResourceNames());
        auto stream = std::make_unique<MockMutateRowsStream>();
        EXPECT_CALL(*stream, Read).WillOnce(Return(Status()));
        return stream;
      });

  auto policy = DefaultIdempotentMutationPolicy();
  bigtable_internal::BulkMutator mutator(
      kAppProfile, kTableName, *policy, std::move(mut),
      std::make_shared<bigtable_internal::OperationContext>());

  MockFunction<void(grpc::ClientContext&)> mock_setup;
  EXPECT_CALL(mock_setup, Call).Times(1);

  bigtable_internal::NoopMutateRowsLimiter limiter;
  (void)mutator.MakeOneRequest(
      *mock, limiter,
      Options{}.set<google::cloud::internal::GrpcSetupOption>(
          mock_setup.AsStdFunction()));
}

TEST_F(BulkMutatorTest, MutationStatusReportedOnOkStream) {
  BulkMutation mut(IdempotentMutation("r0"), IdempotentMutation("r1"));

  auto mock = std::make_shared<MockBigtableStub>();
  EXPECT_CALL(*mock, MutateRows)
      .WillOnce([this](auto context, auto const&,
                       google::bigtable::v2::MutateRowsRequest const& request) {
        metadata_fixture_.SetServerMetadata(*context, {});
        EXPECT_THAT(request, HasCorrectResourceNames());
        auto stream = std::make_unique<MockMutateRowsStream>();
        EXPECT_CALL(*stream, Read)
            .WillOnce(
                Return(MakeResponse({{0, grpc::StatusCode::UNAVAILABLE}})))
            .WillOnce(Return(Status()));
        return stream;
      });

  auto policy = DefaultIdempotentMutationPolicy();
  bigtable_internal::BulkMutator mutator(
      kAppProfile, kTableName, *policy, std::move(mut),
      std::make_shared<bigtable_internal::OperationContext>());

  bigtable_internal::NoopMutateRowsLimiter limiter;
  auto status = mutator.MakeOneRequest(*mock, limiter, Options{});
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

TEST_F(BulkMutatorTest, ReportEitherRetryableMutationFailOrStreamFail) {
  BulkMutation mut(IdempotentMutation("r0"));

  auto mock = std::make_shared<MockBigtableStub>();
  EXPECT_CALL(*mock, MutateRows)
      .WillOnce([this](auto context, auto const&,
                       google::bigtable::v2::MutateRowsRequest const& request) {
        metadata_fixture_.SetServerMetadata(*context, {});
        EXPECT_THAT(request, HasCorrectResourceNames());
        auto stream = std::make_unique<MockMutateRowsStream>();
        EXPECT_CALL(*stream, Read)
            .WillOnce(
                Return(MakeResponse({{0, grpc::StatusCode::UNAVAILABLE}})))
            .WillOnce(Return(Status(StatusCode::kDataLoss, "stream fail")));
        return stream;
      });

  auto policy = DefaultIdempotentMutationPolicy();
  bigtable_internal::BulkMutator mutator(
      kAppProfile, kTableName, *policy, std::move(mut),
      std::make_shared<bigtable_internal::OperationContext>());

  bigtable_internal::NoopMutateRowsLimiter limiter;
  auto status = mutator.MakeOneRequest(*mock, limiter, Options{});
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

TEST_F(BulkMutatorTest, ReportOnlyLatestMutationStatus) {
  // In this test, the mutation fails with an ABORTED status in the first
  // response. It is not included in the second response. We should report the
  // final stream failure for this mutation, as it is the more informative
  // error.
  BulkMutation mut(IdempotentMutation("r0"));

  auto mock = std::make_shared<MockBigtableStub>();
  EXPECT_CALL(*mock, MutateRows)
      .WillOnce([this](auto context, auto const&,
                       google::bigtable::v2::MutateRowsRequest const& request) {
        metadata_fixture_.SetServerMetadata(*context, {});
        EXPECT_THAT(request, HasCorrectResourceNames());
        auto stream = std::make_unique<MockMutateRowsStream>();
        EXPECT_CALL(*stream, Read)
            .WillOnce(Return(MakeResponse({{0, grpc::StatusCode::ABORTED}})))
            .WillOnce(Return(Status(StatusCode::kUnavailable, "try again")));
        return stream;
      })
      .WillOnce([this](auto context, auto const&,
                       google::bigtable::v2::MutateRowsRequest const& request) {
        metadata_fixture_.SetServerMetadata(*context, {});
        EXPECT_THAT(request, HasCorrectResourceNames());
        auto stream = std::make_unique<MockMutateRowsStream>();
        EXPECT_CALL(*stream, Read)
            .WillOnce(Return(Status(StatusCode::kDataLoss, "fail")));
        return stream;
      });

  auto policy = DefaultIdempotentMutationPolicy();
  bigtable_internal::BulkMutator mutator(
      kAppProfile, kTableName, *policy, std::move(mut),
      std::make_shared<bigtable_internal::OperationContext>());

  bigtable_internal::NoopMutateRowsLimiter limiter;
  auto status = mutator.MakeOneRequest(*mock, limiter, Options{});
  EXPECT_THAT(status, StatusIs(StatusCode::kUnavailable));

  status = mutator.MakeOneRequest(*mock, limiter, Options{});
  EXPECT_THAT(status, StatusIs(StatusCode::kDataLoss));

  auto failures = std::move(mutator).OnRetryDone();
  ASSERT_EQ(1UL, failures.size());
  EXPECT_EQ(0, failures[0].original_index());
  EXPECT_THAT(failures[0].status(), StatusIs(StatusCode::kDataLoss));
}

TEST_F(BulkMutatorTest, Throttling) {
  BulkMutation mut(IdempotentMutation("r0"), IdempotentMutation("r1"));

  auto mock_stub = std::make_shared<MockBigtableStub>();
  auto mock_limiter = std::make_shared<MockMutateRowsLimiter>();

  {
    ::testing::InSequence seq;
    EXPECT_CALL(*mock_limiter, Acquire);
    EXPECT_CALL(*mock_stub, MutateRows)
        .WillOnce(
            [this](auto context, auto const&,
                   google::bigtable::v2::MutateRowsRequest const& request) {
              metadata_fixture_.SetServerMetadata(*context, {});
              EXPECT_THAT(request, HasCorrectResourceNames());
              auto stream = std::make_unique<MockMutateRowsStream>();
              EXPECT_CALL(*stream, Read)
                  .WillOnce(Return(MakeResponse({{0, grpc::StatusCode::OK}})))
                  .WillOnce(Return(MakeResponse({{1, grpc::StatusCode::OK}})))
                  .WillOnce(Return(Status()));
              return stream;
            });
    EXPECT_CALL(*mock_limiter, Update).Times(2);
  }

  auto policy = DefaultIdempotentMutationPolicy();
  bigtable_internal::BulkMutator mutator(
      kAppProfile, kTableName, *policy, std::move(mut),
      std::make_shared<bigtable_internal::OperationContext>());

  EXPECT_TRUE(mutator.HasPendingMutations());
  auto status = mutator.MakeOneRequest(*mock_stub, *mock_limiter, Options{});
  EXPECT_STATUS_OK(status);
  auto failures = std::move(mutator).OnRetryDone();
  EXPECT_THAT(failures, IsEmpty());
}

TEST_F(BulkMutatorTest, BigtableCookies) {
  BulkMutation mut(IdempotentMutation("r0"));

  auto mock = std::make_shared<MockBigtableStub>();

  EXPECT_CALL(*mock, MutateRows)
      .WillOnce([this](auto context, auto const&,
                       google::bigtable::v2::MutateRowsRequest const&) {
        // Return a bigtable cookie in the first request.
        metadata_fixture_.SetServerMetadata(
            *context, {{}, {{"x-goog-cbt-cookie-routing", "routing"}}});
        auto stream = std::make_unique<MockMutateRowsStream>();
        EXPECT_CALL(*stream, Read)
            .WillOnce(
                Return(google::cloud::internal::UnavailableError("try again")));
        return stream;
      })
      .WillOnce([this](auto context, auto const&,
                       google::bigtable::v2::MutateRowsRequest const&) {
        // Verify that the next request includes the bigtable cookie from above.
        auto headers = metadata_fixture_.GetMetadata(*context);
        EXPECT_THAT(headers,
                    Contains(Pair("x-goog-cbt-cookie-routing", "routing")));
        auto stream = std::make_unique<MockMutateRowsStream>();
        EXPECT_CALL(*stream, Read)
            .WillOnce(
                Return(google::cloud::internal::PermissionDeniedError("fail")));
        return stream;
      });

  auto policy = DefaultIdempotentMutationPolicy();
  bigtable_internal::BulkMutator mutator(
      kAppProfile, kTableName, *policy, std::move(mut),
      std::make_shared<bigtable_internal::OperationContext>());

  EXPECT_TRUE(mutator.HasPendingMutations());
  bigtable_internal::NoopMutateRowsLimiter limiter;
  auto status = mutator.MakeOneRequest(*mock, limiter, Options{});
  EXPECT_THAT(status, StatusIs(StatusCode::kUnavailable));

  EXPECT_TRUE(mutator.HasPendingMutations());
  status = mutator.MakeOneRequest(*mock, limiter, Options{});
  EXPECT_THAT(status, StatusIs(StatusCode::kPermissionDenied));
}
}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable
}  // namespace cloud
}  // namespace google
