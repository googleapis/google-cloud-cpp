// Copyright 2020 Google LLC
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

#include "google/cloud/bigtable/internal/async_bulk_apply.h"
#include "google/cloud/bigtable/internal/bigtable_stub.h"
#include "google/cloud/bigtable/internal/operation_context.h"
#include "google/cloud/bigtable/testing/mock_bigtable_stub.h"
#include "google/cloud/bigtable/testing/mock_mutate_rows_limiter.h"
#include "google/cloud/grpc_options.h"
#include "google/cloud/internal/async_streaming_read_rpc_impl.h"
#include "google/cloud/internal/background_threads_impl.h"
#ifdef GOOGLE_CLOUD_CPP_BIGTABLE_WITH_OTEL_METRICS
#include "google/cloud/bigtable/internal/metrics.h"
#include "google/cloud/bigtable/internal/table_schema_metrics.h"
#include "google/cloud/testing_util/fake_clock.h"
#endif
#include "google/cloud/internal/opentelemetry.h"
#include "google/cloud/testing_util/mock_backoff_policy.h"
#include "google/cloud/testing_util/mock_completion_queue_impl.h"
#include "google/cloud/testing_util/opentelemetry_matchers.h"
#include "google/cloud/testing_util/status_matchers.h"
#include "google/cloud/testing_util/validate_metadata.h"
#include "google/bigtable/v2/bigtable.grpc.pb.h"
#include <gmock/gmock.h>
#include <grpcpp/grpcpp.h>
#include <grpcpp/support/status.h>
#include <atomic>
#include <chrono>
#include <memory>
#include <optional>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace google {
namespace cloud {
namespace bigtable_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

namespace v2 = ::google::bigtable::v2;
using ms = std::chrono::milliseconds;
using ::google::cloud::bigtable::DataLimitedErrorCountRetryPolicy;
using ::google::cloud::bigtable::testing::MockAsyncMutateRowsStream;
using ::google::cloud::bigtable::testing::MockBigtableStub;
using ::google::cloud::bigtable::testing::MockMutateRowsLimiter;
using ::google::cloud::testing_util::MockBackoffPolicy;
using ::google::cloud::testing_util::MockCompletionQueueImpl;
using ::testing::ByMove;
using ::testing::Contains;
using ::testing::ElementsAreArray;
using ::testing::Matcher;
using ::testing::MockFunction;
using ::testing::Pair;
using ::testing::Property;
using ::testing::Return;

auto constexpr kNumRetries = 2;
auto const* const kTableName =
    "projects/the-project/instances/the-instance/tables/the-table";
auto const* const kAppProfile = "the-profile";

Status TransientError() {
  return Status(StatusCode::kUnavailable, "try again");
}

Status PermanentError() {
  return Status(StatusCode::kPermissionDenied, "fail");
}

bigtable::SingleRowMutation IdempotentMutation(std::string const& row_key) {
  return bigtable::SingleRowMutation(
      row_key, {bigtable::SetCell("fam", "col", ms(0), "val")});
}

bigtable::SingleRowMutation NonIdempotentMutation(std::string const& row_key) {
  return bigtable::SingleRowMutation(row_key,
                                     {bigtable::SetCell("fam", "col", "val")});
}

Matcher<v2::MutateRowsRequest::Entry> MatchEntry(std::string const& row_key) {
  return Property(&v2::MutateRowsRequest::Entry::row_key, row_key);
}

// Individual entry pairs are: {index, StatusCode}
std::optional<v2::MutateRowsResponse> MakeResponse(
    std::vector<std::pair<int, grpc::StatusCode>> const& entries) {
  v2::MutateRowsResponse resp;
  for (auto entry : entries) {
    auto& e = *resp.add_entries();
    e.set_index(entry.first);
    e.mutable_status()->set_code(entry.second);
  }
  return resp;
}

void CheckFailedMutations(
    std::vector<bigtable::FailedMutation> const& actual,
    std::vector<bigtable::FailedMutation> const& expected) {
  struct Unroll {
    explicit Unroll(std::vector<bigtable::FailedMutation> const& failed) {
      for (auto const& f : failed) {
        statuses.push_back(f.status().code());
        indices.push_back(f.original_index());
      }
    }
    std::vector<StatusCode> statuses;
    std::vector<int> indices;
  };

  auto a = Unroll(actual);
  auto e = Unroll(expected);
  EXPECT_THAT(a.statuses, ElementsAreArray(e.statuses));
  EXPECT_THAT(a.indices, ElementsAreArray(e.indices));
}

class AsyncBulkApplyTest : public ::testing::Test {
 protected:
  testing_util::ValidateMetadataFixture metadata_fixture_;
};

TEST_F(AsyncBulkApplyTest, NoMutations) {
  auto mock = std::make_shared<MockBigtableStub>();
  EXPECT_CALL(*mock, AsyncMutateRows).Times(0);

  auto mock_cq = std::make_shared<MockCompletionQueueImpl>();
  CompletionQueue cq(mock_cq);

  auto retry = DataLimitedErrorCountRetryPolicy(kNumRetries).clone();
  auto mock_b = std::make_unique<MockBackoffPolicy>();
  EXPECT_CALL(*mock_b, OnCompletion).Times(0);
  auto idempotency = bigtable::DefaultIdempotentMutationPolicy();

  auto actual = AsyncBulkApplier::Create(
      cq, mock, std::make_shared<NoopMutateRowsLimiter>(), std::move(retry),
      std::move(mock_b), false, *idempotency, kAppProfile, kTableName,
      bigtable::BulkMutation(), std::make_shared<OperationContext>());

  CheckFailedMutations(actual.get(), {});
}

#ifdef GOOGLE_CLOUD_CPP_BIGTABLE_WITH_OTEL_METRICS

class MockMetric : public TableSchemaMetric {
 public:
  MOCK_METHOD(void, PreCall,
              (opentelemetry::context::Context const&, PreCallParams const&),
              (override));
  MOCK_METHOD(void, PostCall,
              (opentelemetry::context::Context const&,
               grpc::ClientContext const&, PostCallParams const&),
              (override));
  MOCK_METHOD(void, OnDone,
              (opentelemetry::context::Context const&, OnDoneParams const&),
              (override));
  MOCK_METHOD(void, ElementRequest,
              (opentelemetry::context::Context const&,
               ElementRequestParams const&),
              (override));
  MOCK_METHOD(void, ElementDelivery,
              (opentelemetry::context::Context const&,
               ElementDeliveryParams const&),
              (override));
  MOCK_METHOD(std::unique_ptr<TableSchemaMetric>, clone,
              (TableResourceLabels const& resource_labels,
               TableDataLabels const& data_labels),
              (const, override));
};

#endif  // GOOGLE_CLOUD_CPP_BIGTABLE_WITH_OTEL_METRICS

TEST_F(AsyncBulkApplyTest, Success) {
  bigtable::BulkMutation mut(IdempotentMutation("r0"),
                             IdempotentMutation("r1"));

#ifdef GOOGLE_CLOUD_CPP_BIGTABLE_WITH_OTEL_METRICS
  auto mock_metric = std::make_shared<MockMetric>();
  EXPECT_CALL(*mock_metric, PreCall).Times(1);
  EXPECT_CALL(*mock_metric, PostCall).Times(1);
  EXPECT_CALL(*mock_metric, OnDone).Times(1);

  auto clock = std::make_shared<testing_util::FakeSteadyClock>();

  // Normally std::make_shared would be used here, but some weird type deduction
  // is preventing it.
  // NOLINTNEXTLINE(modernize-make-shared)
  auto operation_context = std::shared_ptr<OperationContext>(
      new OperationContext({mock_metric}, clock));
#else
  auto operation_context = std::make_shared<OperationContext>();
#endif

  auto mock = std::make_shared<MockBigtableStub>();
  EXPECT_CALL(*mock, AsyncMutateRows)
      .WillOnce([this](CompletionQueue const&, auto client_context, auto,
                       v2::MutateRowsRequest const& request, auto const&) {
        metadata_fixture_.SetServerMetadata(*client_context, {});
        EXPECT_EQ(kAppProfile, request.app_profile_id());
        EXPECT_EQ(kTableName, request.table_name());
        EXPECT_THAT(request.entries(),
                    ElementsAre(MatchEntry("r0"), MatchEntry("r1")));
        auto stream = std::make_unique<MockAsyncMutateRowsStream>();
        EXPECT_CALL(*stream, Start).WillOnce([] {
          return make_ready_future(true);
        });
        EXPECT_CALL(*stream, Read)
            .WillOnce([] {
              return make_ready_future(
                  MakeResponse({{0, grpc::StatusCode::OK}}));
            })
            .WillOnce([] {
              return make_ready_future(
                  MakeResponse({{1, grpc::StatusCode::OK}}));
            })
            .WillOnce([] {
              return make_ready_future(std::optional<v2::MutateRowsResponse>{});
            });
        EXPECT_CALL(*stream, Finish).WillOnce([] {
          return make_ready_future(Status{});
        });
        return stream;
      });

  auto mock_cq = std::make_shared<MockCompletionQueueImpl>();
  CompletionQueue cq(mock_cq);

  auto retry = DataLimitedErrorCountRetryPolicy(kNumRetries).clone();
  auto mock_b = std::make_unique<MockBackoffPolicy>();
  EXPECT_CALL(*mock_b, OnCompletion).Times(0);
  auto idempotency = bigtable::DefaultIdempotentMutationPolicy();

  MockFunction<void(grpc::ClientContext&)> mock_setup;
  EXPECT_CALL(mock_setup, Call).Times(1);
  internal::OptionsSpan span(
      Options{}.set<internal::GrpcSetupOption>(mock_setup.AsStdFunction()));

  auto actual = AsyncBulkApplier::Create(
      cq, mock, std::make_shared<NoopMutateRowsLimiter>(), std::move(retry),
      std::move(mock_b), false, *idempotency, kAppProfile, kTableName,
      std::move(mut), std::move(operation_context));

  CheckFailedMutations(actual.get(), {});
}

TEST_F(AsyncBulkApplyTest, PartialStreamIsRetried) {
  bigtable::BulkMutation mut(IdempotentMutation("r0"),
                             IdempotentMutation("r1"));

#ifdef GOOGLE_CLOUD_CPP_BIGTABLE_WITH_OTEL_METRICS
  auto mock_metric = std::make_shared<MockMetric>();
  EXPECT_CALL(*mock_metric, PreCall).Times(2);
  EXPECT_CALL(*mock_metric, PostCall).Times(2);
  EXPECT_CALL(*mock_metric, OnDone).Times(1);

  auto clock = std::make_shared<testing_util::FakeSteadyClock>();

  // Normally std::make_shared would be used here, but some weird type deduction
  // is preventing it.
  // NOLINTNEXTLINE(modernize-make-shared)
  auto operation_context = std::shared_ptr<OperationContext>(
      new OperationContext({mock_metric}, clock));
#else
  auto operation_context = std::make_shared<OperationContext>();
#endif

  auto mock = std::make_shared<MockBigtableStub>();
  EXPECT_CALL(*mock, AsyncMutateRows)
      .WillOnce([this](CompletionQueue const&, auto context, auto,
                       v2::MutateRowsRequest const& request, auto const&) {
        metadata_fixture_.SetServerMetadata(*context, {});
        EXPECT_EQ(kAppProfile, request.app_profile_id());
        EXPECT_EQ(kTableName, request.table_name());
        EXPECT_THAT(request.entries(),
                    ElementsAre(MatchEntry("r0"), MatchEntry("r1")));
        auto stream = std::make_unique<MockAsyncMutateRowsStream>();
        EXPECT_CALL(*stream, Start).WillOnce([] {
          return make_ready_future(true);
        });
        // This first stream only returns one of the two entries.
        EXPECT_CALL(*stream, Read)
            .WillOnce([] {
              return make_ready_future(
                  MakeResponse({{0, grpc::StatusCode::OK}}));
            })
            .WillOnce([] {
              return make_ready_future(std::optional<v2::MutateRowsResponse>{});
            });
        EXPECT_CALL(*stream, Finish).WillOnce([] {
          return make_ready_future(Status{});
        });
        return stream;
      })
      .WillOnce([this](CompletionQueue const&, auto client_context, auto,
                       v2::MutateRowsRequest const& request, auto const&) {
        metadata_fixture_.SetServerMetadata(*client_context, {});
        EXPECT_EQ(kAppProfile, request.app_profile_id());
        EXPECT_EQ(kTableName, request.table_name());
        EXPECT_THAT(request.entries(), ElementsAre(MatchEntry("r1")));
        auto stream = std::make_unique<MockAsyncMutateRowsStream>();
        EXPECT_CALL(*stream, Start).WillOnce([] {
          return make_ready_future(true);
        });
        EXPECT_CALL(*stream, Read)
            .WillOnce([] {
              return make_ready_future(
                  MakeResponse({{0, grpc::StatusCode::OK}}));
            })
            .WillOnce([] {
              return make_ready_future(std::optional<v2::MutateRowsResponse>{});
            });
        EXPECT_CALL(*stream, Finish).WillOnce([] {
          return make_ready_future(Status{});
        });
        return stream;
      });

  auto mock_cq = std::make_shared<MockCompletionQueueImpl>();
  EXPECT_CALL(*mock_cq, MakeRelativeTimer).WillOnce([] {
    return make_ready_future(make_status_or(std::chrono::system_clock::now()));
  });
  CompletionQueue cq(mock_cq);

  auto retry = DataLimitedErrorCountRetryPolicy(kNumRetries).clone();
  auto mock_b = std::make_unique<MockBackoffPolicy>();
  EXPECT_CALL(*mock_b, OnCompletion).Times(1);
  auto idempotency = bigtable::DefaultIdempotentMutationPolicy();

  MockFunction<void(grpc::ClientContext&)> mock_setup;
  EXPECT_CALL(mock_setup, Call).Times(2);
  internal::OptionsSpan span(
      Options{}.set<internal::GrpcSetupOption>(mock_setup.AsStdFunction()));

  auto actual = AsyncBulkApplier::Create(
      cq, mock, std::make_shared<NoopMutateRowsLimiter>(), std::move(retry),
      std::move(mock_b), false, *idempotency, kAppProfile, kTableName,
      std::move(mut), std::move(operation_context));

  CheckFailedMutations(actual.get(), {});
}

TEST_F(AsyncBulkApplyTest, IdempotentMutationPolicy) {
  std::vector<bigtable::FailedMutation> expected = {{PermanentError(), 2},
                                                    {TransientError(), 3}};
  bigtable::BulkMutation mut(
      IdempotentMutation("success"),
      IdempotentMutation("retry-transient-error"),
      IdempotentMutation("fail-with-permanent-error"),
      NonIdempotentMutation("fail-with-transient-error"));

#ifdef GOOGLE_CLOUD_CPP_BIGTABLE_WITH_OTEL_METRICS
  auto mock_metric = std::make_shared<MockMetric>();
  EXPECT_CALL(*mock_metric, PreCall).Times(2);
  EXPECT_CALL(*mock_metric, PostCall).Times(2);
  EXPECT_CALL(*mock_metric, OnDone).Times(1);

  auto clock = std::make_shared<testing_util::FakeSteadyClock>();

  // Normally std::make_shared would be used here, but some weird type deduction
  // is preventing it.
  // NOLINTNEXTLINE(modernize-make-shared)
  auto operation_context = std::shared_ptr<OperationContext>(
      new OperationContext({mock_metric}, clock));
#else
  auto operation_context = std::make_shared<OperationContext>();
#endif

  auto mock = std::make_shared<MockBigtableStub>();
  EXPECT_CALL(*mock, AsyncMutateRows)
      .WillOnce([this](CompletionQueue const&, auto client_context, auto,
                       v2::MutateRowsRequest const& request, auto const&) {
        metadata_fixture_.SetServerMetadata(*client_context, {});
        EXPECT_EQ(kAppProfile, request.app_profile_id());
        EXPECT_EQ(kTableName, request.table_name());
        auto stream = std::make_unique<MockAsyncMutateRowsStream>();
        EXPECT_CALL(*stream, Start).WillOnce([] {
          return make_ready_future(true);
        });
        EXPECT_CALL(*stream, Read)
            .WillOnce([] {
              return make_ready_future(
                  MakeResponse({{0, grpc::StatusCode::OK},
                                {1, grpc::StatusCode::UNAVAILABLE},
                                {2, grpc::StatusCode::PERMISSION_DENIED},
                                {3, grpc::StatusCode::UNAVAILABLE}}));
            })
            .WillOnce([] {
              return make_ready_future(std::optional<v2::MutateRowsResponse>{});
            });
        EXPECT_CALL(*stream, Finish).WillOnce([] {
          return make_ready_future(Status{});
        });
        return stream;
      })
      .WillOnce([this](CompletionQueue const&, auto client_context, auto,
                       v2::MutateRowsRequest const& request, auto const&) {
        metadata_fixture_.SetServerMetadata(*client_context, {});
        EXPECT_EQ(kAppProfile, request.app_profile_id());
        EXPECT_EQ(kTableName, request.table_name());
        EXPECT_THAT(request.entries(),
                    ElementsAre(MatchEntry("retry-transient-error")));
        auto stream = std::make_unique<MockAsyncMutateRowsStream>();
        EXPECT_CALL(*stream, Start).WillOnce([] {
          return make_ready_future(true);
        });
        EXPECT_CALL(*stream, Read)
            .WillOnce([] {
              return make_ready_future(
                  MakeResponse({{0, grpc::StatusCode::OK}}));
            })
            .WillOnce([] {
              return make_ready_future(std::optional<v2::MutateRowsResponse>{});
            });
        EXPECT_CALL(*stream, Finish).WillOnce([] {
          return make_ready_future(Status{});
        });
        return stream;
      });

  auto mock_cq = std::make_shared<MockCompletionQueueImpl>();
  EXPECT_CALL(*mock_cq, MakeRelativeTimer).WillOnce([] {
    return make_ready_future(make_status_or(std::chrono::system_clock::now()));
  });
  CompletionQueue cq(mock_cq);

  auto retry = DataLimitedErrorCountRetryPolicy(kNumRetries).clone();
  auto mock_b = std::make_unique<MockBackoffPolicy>();
  EXPECT_CALL(*mock_b, OnCompletion).Times(1);
  auto idempotency = bigtable::DefaultIdempotentMutationPolicy();

  MockFunction<void(grpc::ClientContext&)> mock_setup;
  EXPECT_CALL(mock_setup, Call).Times(2);
  internal::OptionsSpan span(
      Options{}.set<internal::GrpcSetupOption>(mock_setup.AsStdFunction()));

  auto actual = AsyncBulkApplier::Create(
      cq, mock, std::make_shared<NoopMutateRowsLimiter>(), std::move(retry),
      std::move(mock_b), false, *idempotency, kAppProfile, kTableName,
      std::move(mut), std::move(operation_context));

  CheckFailedMutations(actual.get(), expected);
}

TEST_F(AsyncBulkApplyTest, TooManyStreamFailures) {
  std::vector<bigtable::FailedMutation> expected = {{TransientError(), 0}};
  bigtable::BulkMutation mut(IdempotentMutation("r0"));

  auto mock = std::make_shared<MockBigtableStub>();
  EXPECT_CALL(*mock, AsyncMutateRows)
      .Times(kNumRetries + 1)
      .WillRepeatedly([this](CompletionQueue const&, auto context, auto,
                             v2::MutateRowsRequest const& request,
                             auto const&) {
        metadata_fixture_.SetServerMetadata(*context, {});
        EXPECT_EQ(kAppProfile, request.app_profile_id());
        EXPECT_EQ(kTableName, request.table_name());
        EXPECT_THAT(request.entries(), ElementsAre(MatchEntry("r0")));
        auto stream = std::make_unique<MockAsyncMutateRowsStream>();
        EXPECT_CALL(*stream, Start).WillOnce([] {
          return make_ready_future(false);
        });
        EXPECT_CALL(*stream, Finish).WillOnce([] {
          return make_ready_future(TransientError());
        });
        return stream;
      });

  auto mock_cq = std::make_shared<MockCompletionQueueImpl>();
  EXPECT_CALL(*mock_cq, MakeRelativeTimer)
      .Times(kNumRetries)
      .WillRepeatedly([] {
        return make_ready_future(
            make_status_or(std::chrono::system_clock::now()));
      });
  CompletionQueue cq(mock_cq);

  auto retry = DataLimitedErrorCountRetryPolicy(kNumRetries).clone();
  auto mock_b = std::make_unique<MockBackoffPolicy>();
  EXPECT_CALL(*mock_b, OnCompletion).Times(kNumRetries);
  auto idempotency = bigtable::DefaultIdempotentMutationPolicy();

  MockFunction<void(grpc::ClientContext&)> mock_setup;
  EXPECT_CALL(mock_setup, Call).Times(kNumRetries + 1);
  internal::OptionsSpan span(
      Options{}.set<internal::GrpcSetupOption>(mock_setup.AsStdFunction()));

  auto actual = AsyncBulkApplier::Create(
      cq, mock, std::make_shared<NoopMutateRowsLimiter>(), std::move(retry),
      std::move(mock_b), false, *idempotency, kAppProfile, kTableName,
      std::move(mut), std::make_shared<OperationContext>());

  CheckFailedMutations(actual.get(), expected);
}

TEST_F(AsyncBulkApplyTest, RetryInfoHeeded) {
  bigtable::BulkMutation mut(IdempotentMutation("r0"));

#ifdef GOOGLE_CLOUD_CPP_BIGTABLE_WITH_OTEL_METRICS
  auto mock_metric = std::make_shared<MockMetric>();
  EXPECT_CALL(*mock_metric, PreCall).Times(2);
  EXPECT_CALL(*mock_metric, PostCall).Times(2);
  EXPECT_CALL(*mock_metric, OnDone).Times(1);

  auto clock = std::make_shared<testing_util::FakeSteadyClock>();

  // Normally std::make_shared would be used here, but some weird type deduction
  // is preventing it.
  // NOLINTNEXTLINE(modernize-make-shared)
  auto operation_context = std::shared_ptr<OperationContext>(
      new OperationContext({mock_metric}, clock));
#else
  auto operation_context = std::make_shared<OperationContext>();
#endif

  auto mock = std::make_shared<MockBigtableStub>();
  EXPECT_CALL(*mock, AsyncMutateRows)
      .WillOnce([this](CompletionQueue const&, auto context, auto,
                       v2::MutateRowsRequest const&, auto const&) {
        metadata_fixture_.SetServerMetadata(*context, {});
        auto stream = std::make_unique<MockAsyncMutateRowsStream>();
        EXPECT_CALL(*stream, Start).WillOnce([] {
          return make_ready_future(false);
        });
        EXPECT_CALL(*stream, Finish).WillOnce([] {
          auto status = PermanentError();
          internal::SetRetryInfo(status, internal::RetryInfo{ms(0)});
          return make_ready_future(status);
        });
        return stream;
      })
      .WillOnce([this](CompletionQueue const&, auto context, auto,
                       v2::MutateRowsRequest const&, auto const&) {
        metadata_fixture_.SetServerMetadata(*context, {});
        auto stream = std::make_unique<MockAsyncMutateRowsStream>();
        EXPECT_CALL(*stream, Start).WillOnce([] {
          return make_ready_future(true);
        });
        EXPECT_CALL(*stream, Read)
            .WillOnce(Return(ByMove(
                make_ready_future(MakeResponse({{0, grpc::StatusCode::OK}})))))
            .WillOnce(Return(ByMove(
                make_ready_future(std::optional<v2::MutateRowsResponse>()))));
        EXPECT_CALL(*stream, Finish)
            .WillOnce(Return(make_ready_future(Status())));
        return stream;
      });

  auto mock_cq = std::make_shared<MockCompletionQueueImpl>();
  EXPECT_CALL(*mock_cq, MakeRelativeTimer)
      .WillOnce(Return(ByMove(make_ready_future(
          make_status_or(std::chrono::system_clock::now())))));
  CompletionQueue cq(mock_cq);

  auto retry = DataLimitedErrorCountRetryPolicy(kNumRetries).clone();
  auto mock_b = std::make_unique<MockBackoffPolicy>();
  auto idempotency = bigtable::DefaultIdempotentMutationPolicy();

  auto actual = AsyncBulkApplier::Create(
      cq, mock, std::make_shared<NoopMutateRowsLimiter>(), std::move(retry),
      std::move(mock_b), true, *idempotency, kAppProfile, kTableName,
      std::move(mut), std::move(operation_context));

  CheckFailedMutations(actual.get(), {});
}

TEST_F(AsyncBulkApplyTest, RetryInfoIgnored) {
  std::vector<bigtable::FailedMutation> expected = {{PermanentError(), 0}};
  bigtable::BulkMutation mut(IdempotentMutation("r0"));

#ifdef GOOGLE_CLOUD_CPP_BIGTABLE_WITH_OTEL_METRICS
  auto mock_metric = std::make_shared<MockMetric>();
  EXPECT_CALL(*mock_metric, PreCall).Times(1);
  EXPECT_CALL(*mock_metric, PostCall).Times(1);
  EXPECT_CALL(*mock_metric, OnDone).Times(1);

  auto clock = std::make_shared<testing_util::FakeSteadyClock>();

  // Normally std::make_shared would be used here, but some weird type deduction
  // is preventing it.
  // NOLINTNEXTLINE(modernize-make-shared)
  auto operation_context = std::shared_ptr<OperationContext>(
      new OperationContext({mock_metric}, clock));
#else
  auto operation_context = std::make_shared<OperationContext>();
#endif

  auto mock = std::make_shared<MockBigtableStub>();
  EXPECT_CALL(*mock, AsyncMutateRows)
      .WillOnce([this](CompletionQueue const&, auto context, auto,
                       v2::MutateRowsRequest const&, auto const&) {
        metadata_fixture_.SetServerMetadata(*context, {});
        auto stream = std::make_unique<MockAsyncMutateRowsStream>();
        EXPECT_CALL(*stream, Start).WillOnce([] {
          return make_ready_future(false);
        });
        EXPECT_CALL(*stream, Finish).WillOnce([] {
          auto status = PermanentError();
          internal::SetRetryInfo(status, internal::RetryInfo{ms(0)});
          return make_ready_future(status);
        });
        return stream;
      });

  auto mock_cq = std::make_shared<MockCompletionQueueImpl>();
  EXPECT_CALL(*mock_cq, MakeRelativeTimer).Times(0);
  CompletionQueue cq(mock_cq);

  auto retry = DataLimitedErrorCountRetryPolicy(kNumRetries).clone();
  auto mock_b = std::make_unique<MockBackoffPolicy>();
  auto idempotency = bigtable::DefaultIdempotentMutationPolicy();

  auto actual = AsyncBulkApplier::Create(
      cq, mock, std::make_shared<NoopMutateRowsLimiter>(), std::move(retry),
      std::move(mock_b), false, *idempotency, kAppProfile, kTableName,
      std::move(mut), std::move(operation_context));

  CheckFailedMutations(actual.get(), expected);
}

TEST_F(AsyncBulkApplyTest, TimerError) {
  std::vector<bigtable::FailedMutation> expected = {{TransientError(), 0}};
  bigtable::BulkMutation mut(IdempotentMutation("r0"));

  auto mock = std::make_shared<MockBigtableStub>();
  EXPECT_CALL(*mock, AsyncMutateRows)
      .WillOnce([this](CompletionQueue const&, auto context, auto,
                       v2::MutateRowsRequest const& request, auto const&) {
        metadata_fixture_.SetServerMetadata(*context, {});
        EXPECT_EQ(kAppProfile, request.app_profile_id());
        EXPECT_EQ(kTableName, request.table_name());
        auto stream = std::make_unique<MockAsyncMutateRowsStream>();
        EXPECT_CALL(*stream, Start).WillOnce([] {
          return make_ready_future(false);
        });
        EXPECT_CALL(*stream, Finish).WillOnce([] {
          return make_ready_future(
              Status(StatusCode::kUnavailable, "try again"));
        });
        return stream;
      });

  auto mock_cq = std::make_shared<MockCompletionQueueImpl>();
  EXPECT_CALL(*mock_cq, MakeRelativeTimer).WillOnce([] {
    return make_ready_future<StatusOr<std::chrono::system_clock::time_point>>(
        Status(StatusCode::kDeadlineExceeded, "timer error"));
  });
  CompletionQueue cq(mock_cq);

  auto retry = DataLimitedErrorCountRetryPolicy(kNumRetries).clone();
  auto mock_b = std::make_unique<MockBackoffPolicy>();
  EXPECT_CALL(*mock_b, OnCompletion).Times(1);
  auto idempotency = bigtable::DefaultIdempotentMutationPolicy();

  MockFunction<void(grpc::ClientContext&)> mock_setup;
  EXPECT_CALL(mock_setup, Call).Times(1);
  internal::OptionsSpan span(
      Options{}.set<internal::GrpcSetupOption>(mock_setup.AsStdFunction()));

  auto actual = AsyncBulkApplier::Create(
      cq, mock, std::make_shared<NoopMutateRowsLimiter>(), std::move(retry),
      std::move(mock_b), false, *idempotency, kAppProfile, kTableName,
      std::move(mut), std::make_shared<OperationContext>());

  CheckFailedMutations(actual.get(), expected);
}

TEST_F(AsyncBulkApplyTest, CancelAfterSuccess) {
  bigtable::BulkMutation mut(IdempotentMutation("r0"));
  promise<std::optional<v2::MutateRowsResponse>> p;

#ifdef GOOGLE_CLOUD_CPP_BIGTABLE_WITH_OTEL_METRICS
  auto mock_metric = std::make_shared<MockMetric>();
  EXPECT_CALL(*mock_metric, PreCall).Times(1);
  EXPECT_CALL(*mock_metric, PostCall).Times(1);
  EXPECT_CALL(*mock_metric, OnDone).Times(1);

  auto clock = std::make_shared<testing_util::FakeSteadyClock>();

  // Normally std::make_shared would be used here, but some weird type deduction
  // is preventing it.
  // NOLINTNEXTLINE(modernize-make-shared)
  auto operation_context = std::shared_ptr<OperationContext>(
      new OperationContext({mock_metric}, clock));
#else
  auto operation_context = std::make_shared<OperationContext>();
#endif

  auto mock = std::make_shared<MockBigtableStub>();
  EXPECT_CALL(*mock, AsyncMutateRows)
      .WillOnce([&p, this](CompletionQueue const&, auto client_context, auto,
                           v2::MutateRowsRequest const& request, auto const&) {
        metadata_fixture_.SetServerMetadata(*client_context, {});
        EXPECT_EQ(kAppProfile, request.app_profile_id());
        EXPECT_EQ(kTableName, request.table_name());
        auto stream = std::make_unique<MockAsyncMutateRowsStream>();
        EXPECT_CALL(*stream, Start).WillOnce([] {
          return make_ready_future(true);
        });
        EXPECT_CALL(*stream, Read)
            .WillOnce([] {
              return make_ready_future(
                  MakeResponse({{0, grpc::StatusCode::OK}}));
            })
            // We block here so the caller can cancel the request. The value
            // returned will be empty, meaning the stream is complete.
            .WillOnce([&p] { return p.get_future(); });
        EXPECT_CALL(*stream, Finish).WillOnce([] {
          return make_ready_future(Status{});
        });
        return stream;
      });

  auto mock_cq = std::make_shared<MockCompletionQueueImpl>();
  CompletionQueue cq(mock_cq);

  auto retry = DataLimitedErrorCountRetryPolicy(kNumRetries).clone();
  auto mock_b = std::make_unique<MockBackoffPolicy>();
  EXPECT_CALL(*mock_b, OnCompletion).Times(0);
  auto idempotency = bigtable::DefaultIdempotentMutationPolicy();

  MockFunction<void(grpc::ClientContext&)> mock_setup;
  EXPECT_CALL(mock_setup, Call).Times(1);
  internal::OptionsSpan span(
      Options{}.set<internal::GrpcSetupOption>(mock_setup.AsStdFunction()));

  auto actual = AsyncBulkApplier::Create(
      cq, mock, std::make_shared<NoopMutateRowsLimiter>(), std::move(retry),
      std::move(mock_b), false, *idempotency, kAppProfile, kTableName,
      std::move(mut), std::move(operation_context));

  // Cancel the call after performing the one and only read of this test stream.
  actual.cancel();
  // Proceed with the rest of the stream. In this test, there are no more
  // responses to be read. The client call should succeed.
  p.set_value(std::optional<v2::MutateRowsResponse>{});
  CheckFailedMutations(actual.get(), {});
}

TEST_F(AsyncBulkApplyTest, CancelMidStream) {
  std::vector<bigtable::FailedMutation> expected = {
      {Status(StatusCode::kCancelled, "User cancelled"), 2}};
  bigtable::BulkMutation mut(IdempotentMutation("r0"), IdempotentMutation("r1"),
                             IdempotentMutation("r2"));
  promise<std::optional<v2::MutateRowsResponse>> p;

#ifdef GOOGLE_CLOUD_CPP_BIGTABLE_WITH_OTEL_METRICS
  auto mock_metric = std::make_shared<MockMetric>();
  EXPECT_CALL(*mock_metric, PreCall).Times(1);
  EXPECT_CALL(*mock_metric, PostCall).Times(1);
  EXPECT_CALL(*mock_metric, OnDone).Times(1);

  auto clock = std::make_shared<testing_util::FakeSteadyClock>();

  // Normally std::make_shared would be used here, but some weird type deduction
  // is preventing it.
  // NOLINTNEXTLINE(modernize-make-shared)
  auto operation_context = std::shared_ptr<OperationContext>(
      new OperationContext({mock_metric}, clock));
#else
  auto operation_context = std::make_shared<OperationContext>();
#endif

  auto mock = std::make_shared<MockBigtableStub>();
  EXPECT_CALL(*mock, AsyncMutateRows)
      .WillOnce([&p, this](CompletionQueue const&, auto client_context, auto,
                           v2::MutateRowsRequest const& request, auto const&) {
        metadata_fixture_.SetServerMetadata(*client_context, {});
        EXPECT_EQ(kAppProfile, request.app_profile_id());
        EXPECT_EQ(kTableName, request.table_name());
        auto stream = std::make_unique<MockAsyncMutateRowsStream>();
        ::testing::InSequence s;
        EXPECT_CALL(*stream, Start).WillOnce([] {
          return make_ready_future(true);
        });
        EXPECT_CALL(*stream, Read)
            .WillOnce([] {
              return make_ready_future(
                  MakeResponse({{0, grpc::StatusCode::OK}}));
            })
            // We block here so the caller can cancel the request. The value
            // returned will be a response, meaning the stream is still active
            // and needs to be drained.
            .WillOnce([&p] { return p.get_future(); });
        EXPECT_CALL(*stream, Cancel);
        EXPECT_CALL(*stream, Read).WillOnce([] {
          return make_ready_future(std::optional<v2::MutateRowsResponse>{});
        });
        EXPECT_CALL(*stream, Finish).WillOnce([] {
          return make_ready_future(
              Status(StatusCode::kCancelled, "User cancelled"));
        });
        return stream;
      });

  auto mock_cq = std::make_shared<MockCompletionQueueImpl>();
  CompletionQueue cq(mock_cq);

  auto retry = DataLimitedErrorCountRetryPolicy(kNumRetries).clone();
  auto mock_b = std::make_unique<MockBackoffPolicy>();
  EXPECT_CALL(*mock_b, OnCompletion).Times(0);
  auto idempotency = bigtable::DefaultIdempotentMutationPolicy();

  MockFunction<void(grpc::ClientContext&)> mock_setup;
  EXPECT_CALL(mock_setup, Call).Times(1);
  internal::OptionsSpan span(
      Options{}.set<internal::GrpcSetupOption>(mock_setup.AsStdFunction()));

  auto actual = AsyncBulkApplier::Create(
      cq, mock, std::make_shared<NoopMutateRowsLimiter>(), std::move(retry),
      std::move(mock_b), false, *idempotency, kAppProfile, kTableName,
      std::move(mut), std::move(operation_context));

  // Cancel the call after performing one read of this test stream.
  actual.cancel();
  // Proceed with the rest of the stream. In this test, there are more responses
  // to be read, which we must drain. The client call should fail.
  p.set_value(MakeResponse({{1, grpc::StatusCode::OK}}));
  CheckFailedMutations(actual.get(), expected);
}

TEST_F(AsyncBulkApplyTest, CurrentOptionsContinuedOnRetries) {
  struct TestOption {
    using Type = int;
  };

  std::vector<bigtable::FailedMutation> expected = {
      {Status(StatusCode::kUnavailable, "try again"), 0}};
  bigtable::BulkMutation mut(IdempotentMutation("r0"));

  auto mock = std::make_shared<MockBigtableStub>();
  EXPECT_CALL(*mock, AsyncMutateRows)
      .Times(2)
      .WillRepeatedly([this](CompletionQueue const&, auto context, auto,
                             v2::MutateRowsRequest const&, auto const&) {
        EXPECT_EQ(5, internal::CurrentOptions().get<TestOption>());
        metadata_fixture_.SetServerMetadata(*context, {});
        auto stream = std::make_unique<MockAsyncMutateRowsStream>();
        EXPECT_CALL(*stream, Start).WillOnce([] {
          return make_ready_future(false);
        });
        EXPECT_CALL(*stream, Finish).WillOnce([] {
          return make_ready_future(
              Status(StatusCode::kUnavailable, "try again"));
        });
        return stream;
      });

  promise<StatusOr<std::chrono::system_clock::time_point>> timer_promise;
  auto mock_cq = std::make_shared<MockCompletionQueueImpl>();
  EXPECT_CALL(*mock_cq, MakeRelativeTimer).WillOnce([&timer_promise] {
    return timer_promise.get_future();
  });
  CompletionQueue cq(mock_cq);

  auto retry = DataLimitedErrorCountRetryPolicy(1).clone();
  auto mock_b = std::make_unique<MockBackoffPolicy>();
  EXPECT_CALL(*mock_b, OnCompletion).Times(1);
  auto idempotency = bigtable::DefaultIdempotentMutationPolicy();

  MockFunction<void(grpc::ClientContext&)> mock_setup;
  EXPECT_CALL(mock_setup, Call).Times(2);

  internal::OptionsSpan span(
      Options{}
          .set<internal::GrpcSetupOption>(mock_setup.AsStdFunction())
          .set<TestOption>(5));
  auto fut = AsyncBulkApplier::Create(
      cq, mock, std::make_shared<NoopMutateRowsLimiter>(), std::move(retry),
      std::move(mock_b), false, *idempotency, kAppProfile, kTableName,
      std::move(mut), std::make_shared<OperationContext>());

  // Simulate the timer being satisfied in a thread with different prevailing
  // options than the calling thread.
  internal::OptionsSpan clear(Options{});
  timer_promise.set_value(make_status_or(std::chrono::system_clock::now()));
}

TEST_F(AsyncBulkApplyTest, RetriesOkStreamWithFailedMutations) {
  std::vector<bigtable::FailedMutation> expected = {
      {Status(StatusCode::kUnavailable, "try again"), 0}};
  bigtable::BulkMutation mut(IdempotentMutation("r1"));

  auto mock = std::make_shared<MockBigtableStub>();
  EXPECT_CALL(*mock, AsyncMutateRows)
      .Times(kNumRetries + 1)
      .WillRepeatedly([this](CompletionQueue const&, auto context, auto,
                             v2::MutateRowsRequest const& request,
                             auto const&) {
        metadata_fixture_.SetServerMetadata(*context, {});
        EXPECT_EQ(kAppProfile, request.app_profile_id());
        EXPECT_EQ(kTableName, request.table_name());
        EXPECT_THAT(request.entries(), ElementsAre(MatchEntry("r1")));
        auto stream = std::make_unique<MockAsyncMutateRowsStream>();
        EXPECT_CALL(*stream, Start).WillOnce([] {
          return make_ready_future(true);
        });
        // The overall stream succeeds, but it contains a failed mutation.
        // Our retry and backoff policies should take effect.
        EXPECT_CALL(*stream, Read)
            .WillOnce([] {
              return make_ready_future(
                  MakeResponse({{0, grpc::StatusCode::UNAVAILABLE}}));
            })
            .WillOnce([] {
              return make_ready_future(std::optional<v2::MutateRowsResponse>{});
            });
        EXPECT_CALL(*stream, Finish).WillOnce([] {
          return make_ready_future(Status());
        });
        return stream;
      });

  auto mock_cq = std::make_shared<MockCompletionQueueImpl>();
  EXPECT_CALL(*mock_cq, MakeRelativeTimer)
      .Times(kNumRetries)
      .WillRepeatedly([] {
        return make_ready_future(
            make_status_or(std::chrono::system_clock::now()));
      });
  CompletionQueue cq(mock_cq);

  auto retry = DataLimitedErrorCountRetryPolicy(kNumRetries).clone();
  auto mock_b = std::make_unique<MockBackoffPolicy>();
  EXPECT_CALL(*mock_b, OnCompletion).Times(kNumRetries);
  auto idempotency = bigtable::DefaultIdempotentMutationPolicy();

  MockFunction<void(grpc::ClientContext&)> mock_setup;
  EXPECT_CALL(mock_setup, Call).Times(kNumRetries + 1);
  internal::OptionsSpan span(
      Options{}.set<internal::GrpcSetupOption>(mock_setup.AsStdFunction()));

  auto actual = AsyncBulkApplier::Create(
      cq, mock, std::make_shared<NoopMutateRowsLimiter>(), std::move(retry),
      std::move(mock_b), false, *idempotency, kAppProfile, kTableName,
      std::move(mut), std::make_shared<OperationContext>());

  CheckFailedMutations(actual.get(), expected);
}

TEST_F(AsyncBulkApplyTest, Throttling) {
  std::vector<bigtable::FailedMutation> expected = {{PermanentError(), 0}};
  bigtable::BulkMutation mut(IdempotentMutation("r1"));

  auto mock = std::make_shared<MockBigtableStub>();
  auto mock_cq = std::make_shared<MockCompletionQueueImpl>();
  CompletionQueue cq(mock_cq);
  auto limiter = std::make_shared<MockMutateRowsLimiter>();

  auto retry = DataLimitedErrorCountRetryPolicy(kNumRetries).clone();
  auto mock_b = std::make_unique<MockBackoffPolicy>();
  EXPECT_CALL(*mock_b, OnCompletion).Times(0);
  auto idempotency = bigtable::DefaultIdempotentMutationPolicy();

  ::testing::InSequence seq;
  EXPECT_CALL(*limiter, AsyncAcquire).WillOnce([] {
    return make_ready_future();
  });
  EXPECT_CALL(*mock, AsyncMutateRows)
      .WillOnce([&limiter, this](CompletionQueue const&, auto client_context,
                                 auto, v2::MutateRowsRequest const&,
                                 auto const&) {
        metadata_fixture_.SetServerMetadata(*client_context, {});
        ::testing::InSequence seq2;
        auto stream = std::make_unique<MockAsyncMutateRowsStream>();
        EXPECT_CALL(*stream, Start).WillOnce([] {
          return make_ready_future(true);
        });
        EXPECT_CALL(*stream, Read).WillOnce([] {
          return make_ready_future(
              MakeResponse({{0, grpc::StatusCode::PERMISSION_DENIED}}));
        });
        EXPECT_CALL(*limiter, Update);
        EXPECT_CALL(*stream, Read).WillOnce([] {
          return make_ready_future(std::optional<v2::MutateRowsResponse>{});
        });
        EXPECT_CALL(*stream, Finish).WillOnce([] {
          return make_ready_future(Status());
        });
        return stream;
      });

  auto actual = AsyncBulkApplier::Create(
      cq, mock, limiter, std::move(retry), std::move(mock_b), false,
      *idempotency, kAppProfile, kTableName, std::move(mut),
      std::make_shared<OperationContext>());

  CheckFailedMutations(actual.get(), expected);
}

TEST_F(AsyncBulkApplyTest, ThrottlingBeforeEachRetry) {
  std::vector<bigtable::FailedMutation> expected = {{TransientError(), 0}};
  bigtable::BulkMutation mut(IdempotentMutation("r1"));

  auto mock = std::make_shared<MockBigtableStub>();
  EXPECT_CALL(*mock, AsyncMutateRows)
      .Times(kNumRetries + 1)
      .WillRepeatedly([this](CompletionQueue const&, auto context, auto,
                             v2::MutateRowsRequest const&, auto const&) {
        metadata_fixture_.SetServerMetadata(*context, {});
        auto stream = std::make_unique<MockAsyncMutateRowsStream>();
        EXPECT_CALL(*stream, Start).WillOnce([] {
          return make_ready_future(true);
        });
        EXPECT_CALL(*stream, Read)
            .WillOnce([] {
              return make_ready_future(
                  MakeResponse({{0, grpc::StatusCode::UNAVAILABLE}}));
            })
            .WillOnce([] {
              return make_ready_future(std::optional<v2::MutateRowsResponse>{});
            });
        EXPECT_CALL(*stream, Finish).WillOnce([] {
          return make_ready_future(Status());
        });
        return stream;
      });

  auto mock_cq = std::make_shared<MockCompletionQueueImpl>();
  EXPECT_CALL(*mock_cq, MakeRelativeTimer)
      .Times(kNumRetries)
      .WillRepeatedly([] {
        return make_ready_future(
            make_status_or(std::chrono::system_clock::now()));
      });
  CompletionQueue cq(mock_cq);
  auto limiter = std::make_shared<MockMutateRowsLimiter>();
  EXPECT_CALL(*limiter, AsyncAcquire).Times(kNumRetries + 1).WillRepeatedly([] {
    return make_ready_future();
  });
  EXPECT_CALL(*limiter, Update).Times(kNumRetries + 1);

  auto retry = DataLimitedErrorCountRetryPolicy(kNumRetries).clone();
  auto mock_b = std::make_unique<MockBackoffPolicy>();
  EXPECT_CALL(*mock_b, OnCompletion).Times(kNumRetries);
  auto idempotency = bigtable::DefaultIdempotentMutationPolicy();

  auto actual = AsyncBulkApplier::Create(
      cq, mock, limiter, std::move(retry), std::move(mock_b), false,
      *idempotency, kAppProfile, kTableName, std::move(mut),
      std::make_shared<OperationContext>());

  CheckFailedMutations(actual.get(), expected);
}

TEST_F(AsyncBulkApplyTest, BigtableCookie) {
  std::vector<bigtable::FailedMutation> expected = {{PermanentError(), 0}};
  bigtable::BulkMutation mut(IdempotentMutation("r0"));

  auto mock = std::make_shared<MockBigtableStub>();
  EXPECT_CALL(*mock, AsyncMutateRows)
      .WillOnce([this](CompletionQueue const&, auto context, auto,
                       v2::MutateRowsRequest const&, auto const&) {
        // Return a bigtable cookie in the first request.
        metadata_fixture_.SetServerMetadata(
            *context, {{}, {{"x-goog-cbt-cookie-routing", "routing"}}});
        auto stream = std::make_unique<MockAsyncMutateRowsStream>();
        EXPECT_CALL(*stream, Start).WillOnce([] {
          return make_ready_future(false);
        });
        EXPECT_CALL(*stream, Finish).WillOnce([] {
          return make_ready_future(TransientError());
        });
        return stream;
      })
      .WillOnce([this](CompletionQueue const&, auto context, auto,
                       v2::MutateRowsRequest const&, auto const&) {
        // Verify that the next request includes the bigtable cookie from above.
        auto headers = metadata_fixture_.GetMetadata(*context);
        EXPECT_THAT(headers,
                    Contains(Pair("x-goog-cbt-cookie-routing", "routing")));
        auto stream = std::make_unique<MockAsyncMutateRowsStream>();
        EXPECT_CALL(*stream, Start).WillOnce([] {
          return make_ready_future(false);
        });
        EXPECT_CALL(*stream, Finish).WillOnce([] {
          return make_ready_future(PermanentError());
        });
        return stream;
      });

  auto mock_cq = std::make_shared<MockCompletionQueueImpl>();
  EXPECT_CALL(*mock_cq, MakeRelativeTimer).WillOnce([] {
    return make_ready_future(make_status_or(std::chrono::system_clock::now()));
  });
  CompletionQueue cq(mock_cq);

  auto retry = DataLimitedErrorCountRetryPolicy(kNumRetries).clone();
  auto mock_b = std::make_unique<MockBackoffPolicy>();
  EXPECT_CALL(*mock_b, OnCompletion).Times(1);
  auto idempotency = bigtable::DefaultIdempotentMutationPolicy();

  auto actual = AsyncBulkApplier::Create(
      cq, mock, std::make_shared<NoopMutateRowsLimiter>(), std::move(retry),
      std::move(mock_b), false, *idempotency, kAppProfile, kTableName,
      std::move(mut), std::make_shared<OperationContext>());

  CheckFailedMutations(actual.get(), expected);
}

using ::google::cloud::testing_util::EnableTracing;
using ::google::cloud::testing_util::IsActive;
using ::google::cloud::testing_util::SpanNamed;
using ::testing::AllOf;
using ::testing::Each;
using ::testing::SizeIs;
using ErrorStream = ::google::cloud::internal::AsyncStreamingReadRpcError<
    v2::MutateRowsResponse>;

TEST_F(AsyncBulkApplyTest, TracedBackoff) {
  auto span_catcher = testing_util::InstallSpanCatcher();

  auto mock = std::make_shared<MockBigtableStub>();
  EXPECT_CALL(*mock, AsyncMutateRows)
      .Times(kNumRetries + 1)
      .WillRepeatedly(
          [this](auto&, auto context, auto, auto const&, auto const&) {
            metadata_fixture_.SetServerMetadata(*context, {});
            return std::make_unique<ErrorStream>(TransientError());
          });

  internal::AutomaticallyCreatedBackgroundThreads background;
  auto retry = DataLimitedErrorCountRetryPolicy(kNumRetries).clone();
  auto mock_b = std::make_unique<MockBackoffPolicy>();
  EXPECT_CALL(*mock_b, OnCompletion).Times(kNumRetries);
  auto idempotency = bigtable::DefaultIdempotentMutationPolicy();
  bigtable::BulkMutation mut(IdempotentMutation("r0"),
                             IdempotentMutation("r1"));

  internal::OptionsSpan o(EnableTracing(Options{}));
  (void)AsyncBulkApplier::Create(
      background.cq(), mock, std::make_shared<NoopMutateRowsLimiter>(),
      std::move(retry), std::move(mock_b), false, *idempotency, kAppProfile,
      kTableName, std::move(mut), std::make_shared<OperationContext>())
      .get();

  EXPECT_THAT(span_catcher->GetSpans(),
              AllOf(SizeIs(kNumRetries), Each(SpanNamed("Async Backoff"))));
}

TEST_F(AsyncBulkApplyTest, CallSpanActiveThroughout) {
  auto span_catcher = testing_util::InstallSpanCatcher();

  auto span = internal::MakeSpan("span");

  auto mock = std::make_shared<MockBigtableStub>();
  EXPECT_CALL(*mock, AsyncMutateRows)
      .Times(kNumRetries + 1)
      .WillRepeatedly(
          [this, span](auto&, auto context, auto, auto const&, auto const&) {
            metadata_fixture_.SetServerMetadata(*context, {});
            EXPECT_THAT(span, IsActive());
            return std::make_unique<ErrorStream>(TransientError());
          });

  internal::AutomaticallyCreatedBackgroundThreads background;
  auto retry = DataLimitedErrorCountRetryPolicy(kNumRetries).clone();
  auto mock_b = std::make_unique<MockBackoffPolicy>();
  EXPECT_CALL(*mock_b, OnCompletion).Times(kNumRetries);
  auto idempotency = bigtable::DefaultIdempotentMutationPolicy();
  bigtable::BulkMutation mut(IdempotentMutation("r0"),
                             IdempotentMutation("r1"));

  internal::OTelScope scope(span);
  internal::OptionsSpan o(EnableTracing(Options{}));
  auto f = AsyncBulkApplier::Create(
      background.cq(), mock, std::make_shared<NoopMutateRowsLimiter>(),
      std::move(retry), std::move(mock_b), false, *idempotency, kAppProfile,
      kTableName, std::move(mut), std::make_shared<OperationContext>());

  auto overlay = opentelemetry::trace::Scope(internal::MakeSpan("overlay"));
  (void)f.get();
}

TEST_F(AsyncBulkApplyTest, TimerErrorCallsOnDone) {
  std::vector<bigtable::FailedMutation> expected = {{TransientError(), 0}};
  bigtable::BulkMutation mut(IdempotentMutation("r0"));

#ifdef GOOGLE_CLOUD_CPP_BIGTABLE_WITH_OTEL_METRICS
  auto mock_metric = std::make_shared<MockMetric>();
  EXPECT_CALL(*mock_metric, PreCall).Times(1);
  EXPECT_CALL(*mock_metric, PostCall).Times(1);
  EXPECT_CALL(*mock_metric,
              OnDone(::testing::_,
                     ::testing::Field(&OnDoneParams::operation_status,
                                      ::google::cloud::testing_util::StatusIs(
                                          StatusCode::kDeadlineExceeded))))
      .Times(1);

  auto clock = std::make_shared<testing_util::FakeSteadyClock>();
  // NOLINTNEXTLINE(modernize-make-shared)
  std::shared_ptr<OperationContext> operation_context(
      new OperationContext({mock_metric}, clock));
#else
  auto operation_context = std::make_shared<OperationContext>();
#endif

  auto mock = std::make_shared<MockBigtableStub>();
  EXPECT_CALL(*mock, AsyncMutateRows)
      .WillOnce([this](CompletionQueue const&,
                       std::shared_ptr<grpc::ClientContext> const& context,
                       internal::ImmutableOptions const&,
                       v2::MutateRowsRequest const&,
                       std::shared_ptr<OperationContext> const&) {
        metadata_fixture_.SetServerMetadata(*context, {});
        auto stream = std::make_unique<MockAsyncMutateRowsStream>();
        EXPECT_CALL(*stream, Start).WillOnce([] {
          return make_ready_future(false);
        });
        EXPECT_CALL(*stream, Finish).WillOnce([] {
          return make_ready_future(TransientError());
        });
        return stream;
      });

  auto mock_cq = std::make_shared<MockCompletionQueueImpl>();
  EXPECT_CALL(*mock_cq, MakeRelativeTimer).WillOnce([] {
    return make_ready_future<StatusOr<std::chrono::system_clock::time_point>>(
        Status(StatusCode::kDeadlineExceeded, "timer error"));
  });
  CompletionQueue cq(mock_cq);

  std::unique_ptr<bigtable::DataRetryPolicy> retry =
      DataLimitedErrorCountRetryPolicy(kNumRetries).clone();
  auto mock_b = std::make_unique<MockBackoffPolicy>();
  EXPECT_CALL(*mock_b, OnCompletion).Times(1);
  std::unique_ptr<bigtable::IdempotentMutationPolicy> idempotency =
      bigtable::DefaultIdempotentMutationPolicy();

  future<std::vector<bigtable::FailedMutation>> actual =
      AsyncBulkApplier::Create(
          cq, mock, std::make_shared<NoopMutateRowsLimiter>(), std::move(retry),
          std::move(mock_b), false, *idempotency, kAppProfile, kTableName,
          std::move(mut), std::move(operation_context));

  CheckFailedMutations(actual.get(), expected);
}

TEST_F(AsyncBulkApplyTest, CancelDuringLimiterAcquireBeforeFirstAttempt) {
  std::vector<bigtable::FailedMutation> expected = {
      {Status(StatusCode::kCancelled, "cancelled"), 0},
      {Status(StatusCode::kCancelled, "cancelled"), 1}};
  bigtable::BulkMutation mut(IdempotentMutation("r0"),
                             NonIdempotentMutation("r1"));

#ifdef GOOGLE_CLOUD_CPP_BIGTABLE_WITH_OTEL_METRICS
  auto mock_metric = std::make_shared<MockMetric>();
  EXPECT_CALL(*mock_metric, PreCall).Times(0);
  EXPECT_CALL(*mock_metric, PostCall).Times(0);
  EXPECT_CALL(*mock_metric, OnDone).Times(0);

  auto clock = std::make_shared<testing_util::FakeSteadyClock>();
  // NOLINTNEXTLINE(modernize-make-shared)
  std::shared_ptr<OperationContext> operation_context(
      new OperationContext({mock_metric}, clock));
#else
  auto operation_context = std::make_shared<OperationContext>();
#endif

  auto mock = std::make_shared<MockBigtableStub>();
  EXPECT_CALL(*mock, AsyncMutateRows).Times(0);

  auto mock_cq = std::make_shared<MockCompletionQueueImpl>();
  EXPECT_CALL(*mock_cq, MakeRelativeTimer).Times(0);
  CompletionQueue cq(mock_cq);

  std::atomic<bool> acquire_cancelled{false};
  promise<void> acquire_promise(
      [&acquire_cancelled] { acquire_cancelled.store(true); });
  auto limiter = std::make_shared<MockMutateRowsLimiter>();
  EXPECT_CALL(*limiter, AsyncAcquire).WillOnce([&acquire_promise] {
    return acquire_promise.get_future();
  });

  std::unique_ptr<bigtable::DataRetryPolicy> retry =
      DataLimitedErrorCountRetryPolicy(kNumRetries).clone();
  auto mock_b = std::make_unique<MockBackoffPolicy>();
  EXPECT_CALL(*mock_b, OnCompletion).Times(0);
  std::unique_ptr<bigtable::IdempotentMutationPolicy> idempotency =
      bigtable::DefaultIdempotentMutationPolicy();

  future<std::vector<bigtable::FailedMutation>> actual =
      AsyncBulkApplier::Create(cq, mock, limiter, std::move(retry),
                               std::move(mock_b), false, *idempotency,
                               kAppProfile, kTableName, std::move(mut),
                               std::move(operation_context));

  actual.cancel();
  EXPECT_TRUE(acquire_cancelled.load());
  acquire_promise.set_value();

  CheckFailedMutations(actual.get(), expected);
}

TEST_F(AsyncBulkApplyTest, CancelDuringLimiterAcquireBeforeRetry) {
  std::vector<bigtable::FailedMutation> expected = {
      {TransientError(), 0}, {Status(StatusCode::kCancelled, "cancelled"), 1}};
  bigtable::BulkMutation mut(IdempotentMutation("r0"),
                             IdempotentMutation("r1"));

#ifdef GOOGLE_CLOUD_CPP_BIGTABLE_WITH_OTEL_METRICS
  auto mock_metric = std::make_shared<MockMetric>();
  EXPECT_CALL(*mock_metric, PreCall).Times(1);
  EXPECT_CALL(*mock_metric, PostCall).Times(1);
  EXPECT_CALL(*mock_metric,
              OnDone(::testing::_,
                     ::testing::Field(&OnDoneParams::operation_status,
                                      ::google::cloud::testing_util::StatusIs(
                                          StatusCode::kCancelled))))
      .Times(1);

  auto clock = std::make_shared<testing_util::FakeSteadyClock>();
  // NOLINTNEXTLINE(modernize-make-shared)
  std::shared_ptr<OperationContext> operation_context(
      new OperationContext({mock_metric}, clock));
#else
  auto operation_context = std::make_shared<OperationContext>();
#endif

  auto mock = std::make_shared<MockBigtableStub>();
  EXPECT_CALL(*mock, AsyncMutateRows)
      .WillOnce([this](CompletionQueue const&,
                       std::shared_ptr<grpc::ClientContext> const& context,
                       internal::ImmutableOptions const&,
                       v2::MutateRowsRequest const&,
                       std::shared_ptr<OperationContext> const&) {
        metadata_fixture_.SetServerMetadata(*context, {});
        auto stream = std::make_unique<MockAsyncMutateRowsStream>();
        EXPECT_CALL(*stream, Start).WillOnce([] {
          return make_ready_future(true);
        });
        EXPECT_CALL(*stream, Read)
            .WillOnce([] {
              return make_ready_future(
                  MakeResponse({{0, grpc::StatusCode::UNAVAILABLE}}));
            })
            .WillOnce([] {
              return make_ready_future(std::optional<v2::MutateRowsResponse>{});
            });
        EXPECT_CALL(*stream, Finish).WillOnce([] {
          return make_ready_future(Status{});
        });
        return stream;
      });

  auto mock_cq = std::make_shared<MockCompletionQueueImpl>();
  EXPECT_CALL(*mock_cq, MakeRelativeTimer).WillOnce([] {
    return make_ready_future(make_status_or(std::chrono::system_clock::now()));
  });
  CompletionQueue cq(mock_cq);

  std::atomic<bool> acquire_cancelled{false};
  promise<void> acquire_promise(
      [&acquire_cancelled] { acquire_cancelled.store(true); });
  auto limiter = std::make_shared<MockMutateRowsLimiter>();
  EXPECT_CALL(*limiter, AsyncAcquire)
      .WillOnce([] { return make_ready_future(); })
      .WillOnce([&acquire_promise] { return acquire_promise.get_future(); });
  EXPECT_CALL(*limiter, Update).Times(1);

  std::unique_ptr<bigtable::DataRetryPolicy> retry =
      DataLimitedErrorCountRetryPolicy(kNumRetries).clone();
  auto mock_b = std::make_unique<MockBackoffPolicy>();
  EXPECT_CALL(*mock_b, OnCompletion).Times(1);
  std::unique_ptr<bigtable::IdempotentMutationPolicy> idempotency =
      bigtable::DefaultIdempotentMutationPolicy();

  future<std::vector<bigtable::FailedMutation>> actual =
      AsyncBulkApplier::Create(cq, mock, limiter, std::move(retry),
                               std::move(mock_b), false, *idempotency,
                               kAppProfile, kTableName, std::move(mut),
                               std::move(operation_context));

  actual.cancel();
  EXPECT_TRUE(acquire_cancelled.load());
  acquire_promise.set_value();

  CheckFailedMutations(actual.get(), expected);
}

TEST_F(AsyncBulkApplyTest, CancelDuringBackoff) {
  std::vector<bigtable::FailedMutation> expected = {
      {TransientError(), 0}, {Status(StatusCode::kCancelled, "cancelled"), 1}};
  bigtable::BulkMutation mut(IdempotentMutation("r0"),
                             IdempotentMutation("r1"));

#ifdef GOOGLE_CLOUD_CPP_BIGTABLE_WITH_OTEL_METRICS
  auto mock_metric = std::make_shared<MockMetric>();
  EXPECT_CALL(*mock_metric, PreCall).Times(1);
  EXPECT_CALL(*mock_metric, PostCall).Times(1);
  EXPECT_CALL(*mock_metric,
              OnDone(::testing::_,
                     ::testing::Field(&OnDoneParams::operation_status,
                                      ::google::cloud::testing_util::StatusIs(
                                          StatusCode::kCancelled))))
      .Times(1);

  auto clock = std::make_shared<testing_util::FakeSteadyClock>();
  // NOLINTNEXTLINE(modernize-make-shared)
  std::shared_ptr<OperationContext> operation_context(
      new OperationContext({mock_metric}, clock));
#else
  auto operation_context = std::make_shared<OperationContext>();
#endif

  auto mock = std::make_shared<MockBigtableStub>();
  EXPECT_CALL(*mock, AsyncMutateRows)
      .WillOnce([this](CompletionQueue const&,
                       std::shared_ptr<grpc::ClientContext> const& context,
                       internal::ImmutableOptions const&,
                       v2::MutateRowsRequest const&,
                       std::shared_ptr<OperationContext> const&) {
        metadata_fixture_.SetServerMetadata(*context, {});
        auto stream = std::make_unique<MockAsyncMutateRowsStream>();
        EXPECT_CALL(*stream, Start).WillOnce([] {
          return make_ready_future(true);
        });
        EXPECT_CALL(*stream, Read)
            .WillOnce([] {
              return make_ready_future(
                  MakeResponse({{0, grpc::StatusCode::UNAVAILABLE}}));
            })
            .WillOnce([] {
              return make_ready_future(std::optional<v2::MutateRowsResponse>{});
            });
        EXPECT_CALL(*stream, Finish).WillOnce([] {
          return make_ready_future(Status{});
        });
        return stream;
      });

  std::atomic<bool> timer_cancelled{false};
  promise<StatusOr<std::chrono::system_clock::time_point>> timer_promise(
      [&timer_cancelled] { timer_cancelled.store(true); });
  auto mock_cq = std::make_shared<MockCompletionQueueImpl>();
  EXPECT_CALL(*mock_cq, MakeRelativeTimer).WillOnce([&timer_promise] {
    return timer_promise.get_future();
  });
  CompletionQueue cq(mock_cq);

  std::unique_ptr<bigtable::DataRetryPolicy> retry =
      DataLimitedErrorCountRetryPolicy(kNumRetries).clone();
  auto mock_b = std::make_unique<MockBackoffPolicy>();
  EXPECT_CALL(*mock_b, OnCompletion).Times(1);
  std::unique_ptr<bigtable::IdempotentMutationPolicy> idempotency =
      bigtable::DefaultIdempotentMutationPolicy();

  future<std::vector<bigtable::FailedMutation>> actual =
      AsyncBulkApplier::Create(
          cq, mock, std::make_shared<NoopMutateRowsLimiter>(), std::move(retry),
          std::move(mock_b), false, *idempotency, kAppProfile, kTableName,
          std::move(mut), std::move(operation_context));

  actual.cancel();
  EXPECT_TRUE(timer_cancelled.load());
  timer_promise.set_value(Status(StatusCode::kCancelled, "timer cancelled"));

  CheckFailedMutations(actual.get(), expected);
}

TEST_F(AsyncBulkApplyTest, CancelDuringBackoffRacesWithTimerExpiry) {
  std::vector<bigtable::FailedMutation> expected = {{TransientError(), 0}};
  bigtable::BulkMutation mut(IdempotentMutation("r0"));

#ifdef GOOGLE_CLOUD_CPP_BIGTABLE_WITH_OTEL_METRICS
  auto mock_metric = std::make_shared<MockMetric>();
  EXPECT_CALL(*mock_metric, PreCall).Times(1);
  EXPECT_CALL(*mock_metric, PostCall).Times(1);
  EXPECT_CALL(*mock_metric,
              OnDone(::testing::_,
                     ::testing::Field(&OnDoneParams::operation_status,
                                      ::google::cloud::testing_util::StatusIs(
                                          StatusCode::kCancelled))))
      .Times(1);

  auto clock = std::make_shared<testing_util::FakeSteadyClock>();
  // NOLINTNEXTLINE(modernize-make-shared)
  std::shared_ptr<OperationContext> operation_context(
      new OperationContext({mock_metric}, clock));
#else
  auto operation_context = std::make_shared<OperationContext>();
#endif

  auto mock = std::make_shared<MockBigtableStub>();
  EXPECT_CALL(*mock, AsyncMutateRows)
      .WillOnce([this](CompletionQueue const&,
                       std::shared_ptr<grpc::ClientContext> const& context,
                       internal::ImmutableOptions const&,
                       v2::MutateRowsRequest const&,
                       std::shared_ptr<OperationContext> const&) {
        metadata_fixture_.SetServerMetadata(*context, {});
        auto stream = std::make_unique<MockAsyncMutateRowsStream>();
        EXPECT_CALL(*stream, Start).WillOnce([] {
          return make_ready_future(false);
        });
        EXPECT_CALL(*stream, Finish).WillOnce([] {
          return make_ready_future(TransientError());
        });
        return stream;
      });

  std::atomic<bool> timer_cancelled{false};
  promise<StatusOr<std::chrono::system_clock::time_point>> timer_promise(
      [&timer_cancelled] { timer_cancelled.store(true); });
  auto mock_cq = std::make_shared<MockCompletionQueueImpl>();
  EXPECT_CALL(*mock_cq, MakeRelativeTimer).WillOnce([&timer_promise] {
    return timer_promise.get_future();
  });
  CompletionQueue cq(mock_cq);

  std::unique_ptr<bigtable::DataRetryPolicy> retry =
      DataLimitedErrorCountRetryPolicy(kNumRetries).clone();
  auto mock_b = std::make_unique<MockBackoffPolicy>();
  EXPECT_CALL(*mock_b, OnCompletion).Times(1);
  std::unique_ptr<bigtable::IdempotentMutationPolicy> idempotency =
      bigtable::DefaultIdempotentMutationPolicy();

  future<std::vector<bigtable::FailedMutation>> actual =
      AsyncBulkApplier::Create(
          cq, mock, std::make_shared<NoopMutateRowsLimiter>(), std::move(retry),
          std::move(mock_b), false, *idempotency, kAppProfile, kTableName,
          std::move(mut), std::move(operation_context));

  actual.cancel();
  EXPECT_TRUE(timer_cancelled.load());
  timer_promise.set_value(make_status_or(std::chrono::system_clock::now()));

  CheckFailedMutations(actual.get(), expected);
}

TEST_F(AsyncBulkApplyTest, CancelAfterTransientEntryResponseBeforeOnFinish) {
  std::vector<bigtable::FailedMutation> expected = {
      {Status(StatusCode::kCancelled, "cancelled"), 3},
      {TransientError(), 1},
      {Status(StatusCode::kCancelled, "cancelled"), 2}};
  bigtable::BulkMutation mut(IdempotentMutation("r0"), IdempotentMutation("r1"),
                             IdempotentMutation("r2"),
                             NonIdempotentMutation("r3"));
  promise<std::optional<v2::MutateRowsResponse>> eof_promise;

#ifdef GOOGLE_CLOUD_CPP_BIGTABLE_WITH_OTEL_METRICS
  auto mock_metric = std::make_shared<MockMetric>();
  EXPECT_CALL(*mock_metric, PreCall).Times(1);
  EXPECT_CALL(*mock_metric, PostCall).Times(1);
  EXPECT_CALL(*mock_metric,
              OnDone(::testing::_,
                     ::testing::Field(&OnDoneParams::operation_status,
                                      ::google::cloud::testing_util::StatusIs(
                                          StatusCode::kCancelled))))
      .Times(1);

  auto clock = std::make_shared<testing_util::FakeSteadyClock>();
  // NOLINTNEXTLINE(modernize-make-shared)
  std::shared_ptr<OperationContext> operation_context(
      new OperationContext({mock_metric}, clock));
#else
  auto operation_context = std::make_shared<OperationContext>();
#endif

  auto mock = std::make_shared<MockBigtableStub>();
  EXPECT_CALL(*mock, AsyncMutateRows)
      .WillOnce([&eof_promise, this](
                    CompletionQueue const&,
                    std::shared_ptr<grpc::ClientContext> const& client_context,
                    internal::ImmutableOptions const&,
                    v2::MutateRowsRequest const&,
                    std::shared_ptr<OperationContext> const&) {
        metadata_fixture_.SetServerMetadata(*client_context, {});
        auto stream = std::make_unique<MockAsyncMutateRowsStream>();
        EXPECT_CALL(*stream, Start).WillOnce([] {
          return make_ready_future(true);
        });
        EXPECT_CALL(*stream, Read)
            .WillOnce([] {
              return make_ready_future(
                  MakeResponse({{0, grpc::StatusCode::OK},
                                {1, grpc::StatusCode::UNAVAILABLE}}));
            })
            .WillOnce([&eof_promise] { return eof_promise.get_future(); });
        EXPECT_CALL(*stream, Finish).WillOnce([] {
          return make_ready_future(Status{});
        });
        return stream;
      });

  auto mock_cq = std::make_shared<MockCompletionQueueImpl>();
  EXPECT_CALL(*mock_cq, MakeRelativeTimer).Times(0);
  CompletionQueue cq(mock_cq);

  std::unique_ptr<bigtable::DataRetryPolicy> retry =
      DataLimitedErrorCountRetryPolicy(kNumRetries).clone();
  auto mock_b = std::make_unique<MockBackoffPolicy>();
  EXPECT_CALL(*mock_b, OnCompletion).Times(0);
  std::unique_ptr<bigtable::IdempotentMutationPolicy> idempotency =
      bigtable::DefaultIdempotentMutationPolicy();

  future<std::vector<bigtable::FailedMutation>> actual =
      AsyncBulkApplier::Create(
          cq, mock, std::make_shared<NoopMutateRowsLimiter>(), std::move(retry),
          std::move(mock_b), false, *idempotency, kAppProfile, kTableName,
          std::move(mut), std::move(operation_context));

  actual.cancel();
  eof_promise.set_value(std::optional<v2::MutateRowsResponse>{});

  CheckFailedMutations(actual.get(), expected);
}

TEST_F(AsyncBulkApplyTest,
       CancelWithOnlyUnconfirmedNonIdempotentMutationReportsCancelledOnDone) {
  std::vector<bigtable::FailedMutation> expected = {
      {Status(StatusCode::kCancelled, "cancelled"), 2}};
  bigtable::BulkMutation mut(IdempotentMutation("r0"), IdempotentMutation("r1"),
                             NonIdempotentMutation("r2"));
  promise<std::optional<v2::MutateRowsResponse>> second_read_promise;

#ifdef GOOGLE_CLOUD_CPP_BIGTABLE_WITH_OTEL_METRICS
  auto mock_metric = std::make_shared<MockMetric>();
  EXPECT_CALL(*mock_metric, PreCall).Times(1);
  EXPECT_CALL(*mock_metric, PostCall).Times(1);
  EXPECT_CALL(*mock_metric,
              OnDone(::testing::_,
                     ::testing::Field(&OnDoneParams::operation_status,
                                      ::google::cloud::testing_util::StatusIs(
                                          StatusCode::kCancelled))))
      .Times(1);

  auto clock = std::make_shared<testing_util::FakeSteadyClock>();
  // NOLINTNEXTLINE(modernize-make-shared)
  std::shared_ptr<OperationContext> operation_context(
      new OperationContext({mock_metric}, clock));
#else
  auto operation_context = std::make_shared<OperationContext>();
#endif

  auto mock = std::make_shared<MockBigtableStub>();
  EXPECT_CALL(*mock, AsyncMutateRows)
      .WillOnce([&second_read_promise, this](
                    CompletionQueue const&,
                    std::shared_ptr<grpc::ClientContext> const& client_context,
                    internal::ImmutableOptions const&,
                    v2::MutateRowsRequest const&,
                    std::shared_ptr<OperationContext> const&) {
        metadata_fixture_.SetServerMetadata(*client_context, {});
        auto stream = std::make_unique<MockAsyncMutateRowsStream>();
        ::testing::InSequence s;
        EXPECT_CALL(*stream, Start).WillOnce([] {
          return make_ready_future(true);
        });
        EXPECT_CALL(*stream, Read)
            .WillOnce([] {
              return make_ready_future(
                  MakeResponse({{0, grpc::StatusCode::OK}}));
            })
            .WillOnce([&second_read_promise] {
              return second_read_promise.get_future();
            });
        EXPECT_CALL(*stream, Cancel);
        EXPECT_CALL(*stream, Read).WillOnce([] {
          return make_ready_future(std::optional<v2::MutateRowsResponse>{});
        });
        EXPECT_CALL(*stream, Finish).WillOnce([] {
          return make_ready_future(Status{});
        });
        return stream;
      });

  auto mock_cq = std::make_shared<MockCompletionQueueImpl>();
  EXPECT_CALL(*mock_cq, MakeRelativeTimer).Times(0);
  CompletionQueue cq(mock_cq);

  std::unique_ptr<bigtable::DataRetryPolicy> retry =
      DataLimitedErrorCountRetryPolicy(kNumRetries).clone();
  auto mock_b = std::make_unique<MockBackoffPolicy>();
  EXPECT_CALL(*mock_b, OnCompletion).Times(0);
  std::unique_ptr<bigtable::IdempotentMutationPolicy> idempotency =
      bigtable::DefaultIdempotentMutationPolicy();

  future<std::vector<bigtable::FailedMutation>> actual =
      AsyncBulkApplier::Create(
          cq, mock, std::make_shared<NoopMutateRowsLimiter>(), std::move(retry),
          std::move(mock_b), false, *idempotency, kAppProfile, kTableName,
          std::move(mut), std::move(operation_context));

  actual.cancel();
  second_read_promise.set_value(MakeResponse({{1, grpc::StatusCode::OK}}));

  CheckFailedMutations(actual.get(), expected);
}

class BlockingMutateRowsService final : public v2::Bigtable::Service {
 public:
  future<void> rpc_started() { return started_.get_future(); }
  int rpc_count() const { return rpc_count_.load(); }
  bool saw_cancelled() const { return saw_cancelled_.load(); }

  grpc::Status MutateRows(
      grpc::ServerContext* context, v2::MutateRowsRequest const*,
      grpc::ServerWriter<v2::MutateRowsResponse>*) override {
    if (rpc_count_.fetch_add(1) == 0) {
      started_.set_value();
    }
    std::chrono::steady_clock::time_point const deadline =
        std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (!context->IsCancelled() &&
           std::chrono::steady_clock::now() < deadline) {
      std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    saw_cancelled_.store(context->IsCancelled());
    return context->IsCancelled()
               ? grpc::Status(grpc::StatusCode::CANCELLED, "Client cancelled")
               : grpc::Status(grpc::StatusCode::DEADLINE_EXCEEDED,
                              "Timed out waiting for TryCancel");
  }

 private:
  promise<void> started_;
  std::atomic<int> rpc_count_{0};
  std::atomic<bool> saw_cancelled_{false};
};

TEST_F(AsyncBulkApplyTest, CancelInFlightRpcBeforeFirstReadCancelsContext) {
  std::vector<bigtable::FailedMutation> expected = {
      {Status(StatusCode::kCancelled, "cancelled"), 1},
      {Status(StatusCode::kCancelled, "cancelled"), 0}};
  bigtable::BulkMutation mut(IdempotentMutation("r0"),
                             NonIdempotentMutation("r1"));

#ifdef GOOGLE_CLOUD_CPP_BIGTABLE_WITH_OTEL_METRICS
  auto mock_metric = std::make_shared<MockMetric>();
  EXPECT_CALL(*mock_metric, PreCall).Times(1);
  EXPECT_CALL(*mock_metric, PostCall).Times(1);
  EXPECT_CALL(*mock_metric,
              OnDone(::testing::_,
                     ::testing::Field(&OnDoneParams::operation_status,
                                      ::google::cloud::testing_util::StatusIs(
                                          StatusCode::kCancelled))))
      .Times(1);

  auto clock = std::make_shared<testing_util::FakeSteadyClock>();
  // NOLINTNEXTLINE(modernize-make-shared)
  std::shared_ptr<OperationContext> operation_context(
      new OperationContext({mock_metric}, clock));
#else
  auto operation_context = std::make_shared<OperationContext>();
#endif

  BlockingMutateRowsService service;
  grpc::ServerBuilder builder;
  builder.RegisterService(&service);
  std::unique_ptr<grpc::Server> server = builder.BuildAndStart();

  {
    internal::AutomaticallyCreatedBackgroundThreads background;
    std::shared_ptr<grpc::Channel> channel =
        server->InProcessChannel(grpc::ChannelArguments{});
    auto stub = std::make_shared<DefaultBigtableStub>(
        v2::Bigtable::NewStub(std::move(channel)));

    std::unique_ptr<bigtable::DataRetryPolicy> retry =
        DataLimitedErrorCountRetryPolicy(kNumRetries).clone();
    auto mock_b = std::make_unique<MockBackoffPolicy>();
    EXPECT_CALL(*mock_b, OnCompletion).Times(0);
    std::unique_ptr<bigtable::IdempotentMutationPolicy> idempotency =
        bigtable::DefaultIdempotentMutationPolicy();

    future<void> started = service.rpc_started();
    future<std::vector<bigtable::FailedMutation>> actual =
        AsyncBulkApplier::Create(background.cq(), stub,
                                 std::make_shared<NoopMutateRowsLimiter>(),
                                 std::move(retry), std::move(mock_b), false,
                                 *idempotency, kAppProfile, kTableName,
                                 std::move(mut), std::move(operation_context));

    started.get();
    actual.cancel();

    CheckFailedMutations(actual.get(), expected);
  }

  server->Shutdown();
  server->Wait();

  EXPECT_TRUE(service.saw_cancelled());
  EXPECT_EQ(1, service.rpc_count());
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable_internal
}  // namespace cloud
}  // namespace google
