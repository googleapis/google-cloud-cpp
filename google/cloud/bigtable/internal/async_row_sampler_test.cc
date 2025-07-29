// Copyright 2021 Google LLC
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

#include "google/cloud/bigtable/internal/async_row_sampler.h"
#include "google/cloud/bigtable/testing/mock_bigtable_stub.h"
#include "google/cloud/grpc_options.h"
#include "google/cloud/internal/async_streaming_read_rpc_impl.h"
#include "google/cloud/internal/background_threads_impl.h"
#include "google/cloud/internal/make_status.h"
#ifdef GOOGLE_CLOUD_CPP_BIGTABLE_WITH_OTEL_METRICS
#include "google/cloud/bigtable/internal/metrics.h"
#include "google/cloud/testing_util/fake_clock.h"
#endif
#include "google/cloud/internal/opentelemetry.h"
#include "google/cloud/testing_util/mock_backoff_policy.h"
#include "google/cloud/testing_util/mock_completion_queue_impl.h"
#include "google/cloud/testing_util/opentelemetry_matchers.h"
#include "google/cloud/testing_util/status_matchers.h"
#include "google/cloud/testing_util/validate_metadata.h"
#include <gmock/gmock.h>
#include <chrono>

namespace google {
namespace cloud {
namespace bigtable_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

namespace v2 = ::google::bigtable::v2;
using ms = std::chrono::milliseconds;
using ::google::cloud::bigtable::DataLimitedErrorCountRetryPolicy;
using ::google::cloud::bigtable::testing::MockAsyncSampleRowKeysStream;
using ::google::cloud::bigtable::testing::MockBigtableStub;
using ::google::cloud::testing_util::MockBackoffPolicy;
using ::google::cloud::testing_util::MockCompletionQueueImpl;
using ::google::cloud::testing_util::StatusIs;
using ::testing::ByMove;
using ::testing::Contains;
using ::testing::ElementsAre;
using ::testing::HasSubstr;
using ::testing::MockFunction;
using ::testing::Pair;
using ::testing::Return;

auto constexpr kNumRetries = 2;
auto const* const kTableName =
    "projects/the-project/instances/the-instance/tables/the-table";
auto const* const kAppProfile = "the-profile";

struct RowKeySampleVectors {
  explicit RowKeySampleVectors(std::vector<bigtable::RowKeySample> samples) {
    row_keys.reserve(samples.size());
    offset_bytes.reserve(samples.size());
    for (auto& sample : samples) {
      row_keys.emplace_back(std::move(sample.row_key));
      offset_bytes.emplace_back(std::move(sample.offset_bytes));
    }
  }

  std::vector<std::string> row_keys;
  std::vector<std::int64_t> offset_bytes;
};

absl::optional<v2::SampleRowKeysResponse> MakeResponse(std::string row_key,
                                                       std::int64_t offset) {
  v2::SampleRowKeysResponse r;
  r.set_row_key(std::move(row_key));
  r.set_offset_bytes(offset);
  return absl::make_optional(r);
};

#ifdef GOOGLE_CLOUD_CPP_BIGTABLE_WITH_OTEL_METRICS

class MockMetric : public Metric {
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
  MOCK_METHOD(std::unique_ptr<Metric>, clone,
              (ResourceLabels resource_labels, DataLabels data_labels),
              (const, override));
};

// This class is a vehicle to get a MockMetric into the OperationContext object.
class CloningMetric : public Metric {
 public:
  explicit CloningMetric(std::unique_ptr<MockMetric> metric)
      : metric_(std::move(metric)) {}
  std::unique_ptr<Metric> clone(ResourceLabels, DataLabels) const override {
    return std::move(metric_);
  }

 private:
  mutable std::unique_ptr<MockMetric> metric_;
};

#endif  // GOOGLE_CLOUD_CPP_BIGTABLE_WITH_OTEL_METRICS

class AsyncSampleRowKeysTest : public ::testing::Test {
 protected:
  testing_util::ValidateMetadataFixture metadata_fixture_;
};

TEST_F(AsyncSampleRowKeysTest, Simple) {
#ifdef GOOGLE_CLOUD_CPP_BIGTABLE_WITH_OTEL_METRICS
  auto mock_metric = std::make_unique<MockMetric>();
  EXPECT_CALL(*mock_metric, PreCall).Times(1);
  EXPECT_CALL(*mock_metric, PostCall).Times(1);
  EXPECT_CALL(*mock_metric, OnDone).Times(1);

  auto fake_metric = std::make_shared<CloningMetric>(std::move(mock_metric));
  auto clock = std::make_shared<testing_util::FakeSteadyClock>();

  // Normally std::make_shared would be used here, but some weird type deduction
  // is preventing it.
  // NOLINTNEXTLINE(modernize-make-shared)
  auto operation_context = std::shared_ptr<OperationContext>(
      new OperationContext({}, {}, {fake_metric}, clock));
#else
  auto operation_context = std::make_shared<OperationContext>();
#endif
  auto mock = std::make_shared<MockBigtableStub>();
  EXPECT_CALL(*mock, AsyncSampleRowKeys)
      .WillOnce([this](CompletionQueue const&, auto client_context, auto,
                       v2::SampleRowKeysRequest const& request) {
        metadata_fixture_.SetServerMetadata(*client_context, {});
        EXPECT_EQ(kAppProfile, request.app_profile_id());
        EXPECT_EQ(kTableName, request.table_name());
        auto stream = std::make_unique<MockAsyncSampleRowKeysStream>();
        EXPECT_CALL(*stream, Start).WillOnce([] {
          return make_ready_future(true);
        });
        EXPECT_CALL(*stream, Read)
            .WillOnce(
                [] { return make_ready_future(MakeResponse("test1", 11)); })
            .WillOnce(
                [] { return make_ready_future(MakeResponse("test2", 22)); })
            .WillOnce([] {
              return make_ready_future(
                  absl::optional<v2::SampleRowKeysResponse>{});
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

  MockFunction<void(grpc::ClientContext&)> mock_setup;
  EXPECT_CALL(mock_setup, Call).Times(1);
  internal::OptionsSpan span(
      Options{}.set<internal::GrpcSetupOption>(mock_setup.AsStdFunction()));

  auto sor = AsyncRowSampler::Create(cq, mock, std::move(retry),
                                     std::move(mock_b), false, kAppProfile,
                                     kTableName, std::move(operation_context))
                 .get();

  ASSERT_STATUS_OK(sor);
  auto samples = RowKeySampleVectors(*sor);
  EXPECT_THAT(samples.row_keys, ElementsAre("test1", "test2"));
  EXPECT_THAT(samples.offset_bytes, ElementsAre(11, 22));
}

TEST_F(AsyncSampleRowKeysTest, RetryResetsSamples) {
#ifdef GOOGLE_CLOUD_CPP_BIGTABLE_WITH_OTEL_METRICS
  auto mock_metric = std::make_unique<MockMetric>();
  EXPECT_CALL(*mock_metric, PreCall).Times(2);
  EXPECT_CALL(*mock_metric, PostCall).Times(2);
  EXPECT_CALL(*mock_metric, OnDone).Times(1);

  auto fake_metric = std::make_shared<CloningMetric>(std::move(mock_metric));
  auto clock = std::make_shared<testing_util::FakeSteadyClock>();

  // Normally std::make_shared would be used here, but some weird type deduction
  // is preventing it.
  // NOLINTNEXTLINE(modernize-make-shared)
  auto operation_context = std::shared_ptr<OperationContext>(
      new OperationContext({}, {}, {fake_metric}, clock));
#else
  auto operation_context = std::make_shared<OperationContext>();
#endif

  auto mock = std::make_shared<MockBigtableStub>();
  EXPECT_CALL(*mock, AsyncSampleRowKeys)
      .WillOnce([this](CompletionQueue const&, auto context, auto,
                       v2::SampleRowKeysRequest const& request) {
        metadata_fixture_.SetServerMetadata(*context, {});
        EXPECT_EQ(kAppProfile, request.app_profile_id());
        EXPECT_EQ(kTableName, request.table_name());
        auto stream = std::make_unique<MockAsyncSampleRowKeysStream>();
        EXPECT_CALL(*stream, Start).WillOnce([] {
          return make_ready_future(true);
        });
        EXPECT_CALL(*stream, Read)
            .WillOnce(
                [] { return make_ready_future(MakeResponse("forgotten", 11)); })
            .WillOnce([] {
              return make_ready_future(
                  absl::optional<v2::SampleRowKeysResponse>{});
            });
        EXPECT_CALL(*stream, Finish).WillOnce([] {
          return make_ready_future(
              Status(StatusCode::kUnavailable, "try again"));
        });
        return stream;
      })
      .WillOnce([this](CompletionQueue const&, auto client_context, auto,
                       v2::SampleRowKeysRequest const& request) {
        metadata_fixture_.SetServerMetadata(*client_context, {});
        EXPECT_EQ(kAppProfile, request.app_profile_id());
        EXPECT_EQ(kTableName, request.table_name());
        auto stream = std::make_unique<MockAsyncSampleRowKeysStream>();
        EXPECT_CALL(*stream, Start).WillOnce([] {
          return make_ready_future(true);
        });
        EXPECT_CALL(*stream, Read)
            .WillOnce(
                [] { return make_ready_future(MakeResponse("returned", 22)); })
            .WillOnce([] {
              return make_ready_future(
                  absl::optional<v2::SampleRowKeysResponse>{});
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

  MockFunction<void(grpc::ClientContext&)> mock_setup;
  EXPECT_CALL(mock_setup, Call).Times(2);
  internal::OptionsSpan span(
      Options{}.set<internal::GrpcSetupOption>(mock_setup.AsStdFunction()));

  auto sor = AsyncRowSampler::Create(cq, mock, std::move(retry),
                                     std::move(mock_b), false, kAppProfile,
                                     kTableName, std::move(operation_context))
                 .get();

  ASSERT_STATUS_OK(sor);
  auto samples = RowKeySampleVectors(*sor);
  EXPECT_THAT(samples.row_keys, ElementsAre("returned"));
  EXPECT_THAT(samples.offset_bytes, ElementsAre(22));
}

TEST_F(AsyncSampleRowKeysTest, TooManyFailures) {
#ifdef GOOGLE_CLOUD_CPP_BIGTABLE_WITH_OTEL_METRICS
  auto mock_metric = std::make_unique<MockMetric>();
  EXPECT_CALL(*mock_metric, PreCall).Times(kNumRetries + 1);
  EXPECT_CALL(*mock_metric, PostCall).Times(kNumRetries + 1);
  EXPECT_CALL(*mock_metric, OnDone).Times(1);

  auto fake_metric = std::make_shared<CloningMetric>(std::move(mock_metric));
  auto clock = std::make_shared<testing_util::FakeSteadyClock>();

  // Normally std::make_shared would be used here, but some weird type deduction
  // is preventing it.
  // NOLINTNEXTLINE(modernize-make-shared)
  auto operation_context = std::shared_ptr<OperationContext>(
      new OperationContext({}, {}, {fake_metric}, clock));
#else
  auto operation_context = std::make_shared<OperationContext>();
#endif

  auto mock = std::make_shared<MockBigtableStub>();
  EXPECT_CALL(*mock, AsyncSampleRowKeys)
      .Times(kNumRetries + 1)
      .WillRepeatedly([this](CompletionQueue const&, auto context, auto,
                             v2::SampleRowKeysRequest const& request) {
        metadata_fixture_.SetServerMetadata(*context, {});
        EXPECT_EQ(kAppProfile, request.app_profile_id());
        EXPECT_EQ(kTableName, request.table_name());
        auto stream = std::make_unique<MockAsyncSampleRowKeysStream>();
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

  MockFunction<void(grpc::ClientContext&)> mock_setup;
  EXPECT_CALL(mock_setup, Call).Times(kNumRetries + 1);
  internal::OptionsSpan span(
      Options{}.set<internal::GrpcSetupOption>(mock_setup.AsStdFunction()));

  auto sor = AsyncRowSampler::Create(cq, mock, std::move(retry),
                                     std::move(mock_b), false, kAppProfile,
                                     kTableName, std::move(operation_context))
                 .get();

  EXPECT_THAT(sor, StatusIs(StatusCode::kUnavailable, HasSubstr("try again")));
}

TEST_F(AsyncSampleRowKeysTest, RetryInfoHeeded) {
#ifdef GOOGLE_CLOUD_CPP_BIGTABLE_WITH_OTEL_METRICS
  auto mock_metric = std::make_unique<MockMetric>();
  EXPECT_CALL(*mock_metric, PreCall).Times(2);
  EXPECT_CALL(*mock_metric, PostCall).Times(2);
  EXPECT_CALL(*mock_metric, OnDone).Times(1);

  auto fake_metric = std::make_shared<CloningMetric>(std::move(mock_metric));
  auto clock = std::make_shared<testing_util::FakeSteadyClock>();

  // Normally std::make_shared would be used here, but some weird type deduction
  // is preventing it.
  // NOLINTNEXTLINE(modernize-make-shared)
  auto operation_context = std::shared_ptr<OperationContext>(
      new OperationContext({}, {}, {fake_metric}, clock));
#else
  auto operation_context = std::make_shared<OperationContext>();
#endif

  auto mock = std::make_shared<MockBigtableStub>();
  EXPECT_CALL(*mock, AsyncSampleRowKeys)
      .WillOnce([this](CompletionQueue const&, auto context, auto,
                       v2::SampleRowKeysRequest const&) {
        metadata_fixture_.SetServerMetadata(*context, {});
        auto stream = std::make_unique<MockAsyncSampleRowKeysStream>();
        EXPECT_CALL(*stream, Start).WillOnce([] {
          return make_ready_future(false);
        });
        EXPECT_CALL(*stream, Finish).WillOnce([] {
          auto status = internal::ResourceExhaustedError("try again");
          internal::SetRetryInfo(status, internal::RetryInfo{ms(10)});
          return make_ready_future(status);
        });
        return stream;
      })
      .WillOnce([this](CompletionQueue const&, auto context, auto,
                       v2::SampleRowKeysRequest const&) {
        metadata_fixture_.SetServerMetadata(*context, {});
        auto stream = std::make_unique<MockAsyncSampleRowKeysStream>();
        EXPECT_CALL(*stream, Start).WillOnce([] {
          return make_ready_future(true);
        });
        EXPECT_CALL(*stream, Read)
            .WillOnce(
                [] { return make_ready_future(MakeResponse("returned", 22)); })
            .WillOnce([] {
              return make_ready_future(
                  absl::optional<v2::SampleRowKeysResponse>{});
            });
        EXPECT_CALL(*stream, Finish).WillOnce([] {
          return make_ready_future(Status{});
        });
        return stream;
      });

  auto mock_cq = std::make_shared<MockCompletionQueueImpl>();
  EXPECT_CALL(*mock_cq, MakeRelativeTimer(std::chrono::nanoseconds(ms(10))))
      .WillOnce(Return(ByMove(make_ready_future(
          make_status_or(std::chrono::system_clock::now())))));
  CompletionQueue cq(mock_cq);

  auto retry = DataLimitedErrorCountRetryPolicy(kNumRetries).clone();
  auto mock_b = std::make_unique<MockBackoffPolicy>();
  EXPECT_CALL(*mock_b, OnCompletion);

  auto sor = AsyncRowSampler::Create(cq, mock, std::move(retry),
                                     std::move(mock_b), true, kAppProfile,
                                     kTableName, std::move(operation_context))
                 .get();
  EXPECT_STATUS_OK(sor);
}

TEST_F(AsyncSampleRowKeysTest, RetryInfoIgnored) {
#ifdef GOOGLE_CLOUD_CPP_BIGTABLE_WITH_OTEL_METRICS
  auto mock_metric = std::make_unique<MockMetric>();
  EXPECT_CALL(*mock_metric, PreCall).Times(1);
  EXPECT_CALL(*mock_metric, PostCall).Times(1);
  EXPECT_CALL(*mock_metric, OnDone).Times(1);

  auto fake_metric = std::make_shared<CloningMetric>(std::move(mock_metric));
  auto clock = std::make_shared<testing_util::FakeSteadyClock>();

  // Normally std::make_shared would be used here, but some weird type deduction
  // is preventing it.
  // NOLINTNEXTLINE(modernize-make-shared)
  auto operation_context = std::shared_ptr<OperationContext>(
      new OperationContext({}, {}, {fake_metric}, clock));
#else
  auto operation_context = std::make_shared<OperationContext>();
#endif

  auto mock = std::make_shared<MockBigtableStub>();
  EXPECT_CALL(*mock, AsyncSampleRowKeys)
      .WillOnce([this](CompletionQueue const&, auto context, auto,
                       v2::SampleRowKeysRequest const&) {
        metadata_fixture_.SetServerMetadata(*context, {});
        auto stream = std::make_unique<MockAsyncSampleRowKeysStream>();
        EXPECT_CALL(*stream, Start).WillOnce([] {
          return make_ready_future(false);
        });
        EXPECT_CALL(*stream, Finish).WillOnce([] {
          auto status = internal::ResourceExhaustedError("try again");
          internal::SetRetryInfo(status, internal::RetryInfo{ms(10)});
          return make_ready_future(status);
        });
        return stream;
      });

  auto mock_cq = std::make_shared<MockCompletionQueueImpl>();
  EXPECT_CALL(*mock_cq, MakeRelativeTimer).Times(0);
  CompletionQueue cq(mock_cq);

  auto retry = DataLimitedErrorCountRetryPolicy(kNumRetries).clone();
  auto mock_b = std::make_unique<MockBackoffPolicy>();
  EXPECT_CALL(*mock_b, OnCompletion).Times(0);

  auto sor = AsyncRowSampler::Create(cq, mock, std::move(retry),
                                     std::move(mock_b), false, kAppProfile,
                                     kTableName, std::move(operation_context))
                 .get();
  EXPECT_THAT(sor, StatusIs(StatusCode::kResourceExhausted));
}

TEST_F(AsyncSampleRowKeysTest, TimerError) {
  auto mock = std::make_shared<MockBigtableStub>();
  EXPECT_CALL(*mock, AsyncSampleRowKeys)
      .WillOnce([this](CompletionQueue const&, auto context, auto,
                       v2::SampleRowKeysRequest const& request) {
        metadata_fixture_.SetServerMetadata(*context);
        EXPECT_EQ(kAppProfile, request.app_profile_id());
        EXPECT_EQ(kTableName, request.table_name());
        auto stream = std::make_unique<MockAsyncSampleRowKeysStream>();
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

  MockFunction<void(grpc::ClientContext&)> mock_setup;
  EXPECT_CALL(mock_setup, Call).Times(1);
  internal::OptionsSpan span(
      Options{}.set<internal::GrpcSetupOption>(mock_setup.AsStdFunction()));

  auto sor = AsyncRowSampler::Create(
                 cq, mock, std::move(retry), std::move(mock_b), false,
                 kAppProfile, kTableName, std::make_shared<OperationContext>())
                 .get();
  // If the TimerFuture returns a bad status, it is almost always because the
  // call has been cancelled. So it is more informative for the sampler to
  // return "call cancelled" than to pass along the exact error.
  EXPECT_THAT(sor,
              StatusIs(StatusCode::kCancelled, HasSubstr("call cancelled")));
  EXPECT_THAT(sor.status().error_info().metadata(),
              Contains(Pair("gl-cpp.error.origin", "client")));
}

TEST_F(AsyncSampleRowKeysTest, CancelAfterSuccess) {
  promise<absl::optional<v2::SampleRowKeysResponse>> p;
#ifdef GOOGLE_CLOUD_CPP_BIGTABLE_WITH_OTEL_METRICS
  auto mock_metric = std::make_unique<MockMetric>();
  EXPECT_CALL(*mock_metric, PreCall).Times(1);
  EXPECT_CALL(*mock_metric, PostCall).Times(1);
  EXPECT_CALL(*mock_metric, OnDone).Times(1);

  auto fake_metric = std::make_shared<CloningMetric>(std::move(mock_metric));
  auto clock = std::make_shared<testing_util::FakeSteadyClock>();

  // Normally std::make_shared would be used here, but some weird type deduction
  // is preventing it.
  // NOLINTNEXTLINE(modernize-make-shared)
  auto operation_context = std::shared_ptr<OperationContext>(
      new OperationContext({}, {}, {fake_metric}, clock));
#else
  auto operation_context = std::make_shared<OperationContext>();
#endif

  auto mock = std::make_shared<MockBigtableStub>();
  EXPECT_CALL(*mock, AsyncSampleRowKeys)
      .WillOnce([&p, this](CompletionQueue const&, auto client_context, auto,
                           v2::SampleRowKeysRequest const& request) {
        metadata_fixture_.SetServerMetadata(*client_context, {});
        EXPECT_EQ(kAppProfile, request.app_profile_id());
        EXPECT_EQ(kTableName, request.table_name());
        auto stream = std::make_unique<MockAsyncSampleRowKeysStream>();
        EXPECT_CALL(*stream, Start).WillOnce([] {
          return make_ready_future(true);
        });
        EXPECT_CALL(*stream, Read)
            .WillOnce(
                [] { return make_ready_future(MakeResponse("test1", 11)); })
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

  MockFunction<void(grpc::ClientContext&)> mock_setup;
  EXPECT_CALL(mock_setup, Call).Times(1);
  internal::OptionsSpan span(
      Options{}.set<internal::GrpcSetupOption>(mock_setup.AsStdFunction()));

  auto fut = AsyncRowSampler::Create(cq, mock, std::move(retry),
                                     std::move(mock_b), false, kAppProfile,
                                     kTableName, std::move(operation_context));
  // Cancel the call after performing the one and only read of this test stream.
  fut.cancel();
  // Proceed with the rest of the stream. In this test, there are no more
  // responses to be read. The client call should succeed.
  p.set_value(absl::optional<v2::SampleRowKeysResponse>{});
  auto sor = fut.get();
  ASSERT_STATUS_OK(sor);
  auto samples = RowKeySampleVectors(*sor);
  EXPECT_THAT(samples.row_keys, ElementsAre("test1"));
  EXPECT_THAT(samples.offset_bytes, ElementsAre(11));
}

TEST_F(AsyncSampleRowKeysTest, CancelMidStream) {
  promise<absl::optional<v2::SampleRowKeysResponse>> p;
#ifdef GOOGLE_CLOUD_CPP_BIGTABLE_WITH_OTEL_METRICS
  auto mock_metric = std::make_unique<MockMetric>();
  EXPECT_CALL(*mock_metric, PreCall).Times(1);
  EXPECT_CALL(*mock_metric, PostCall).Times(1);
  EXPECT_CALL(*mock_metric, OnDone).Times(1);

  auto fake_metric = std::make_shared<CloningMetric>(std::move(mock_metric));
  auto clock = std::make_shared<testing_util::FakeSteadyClock>();

  // Normally std::make_shared would be used here, but some weird type deduction
  // is preventing it.
  // NOLINTNEXTLINE(modernize-make-shared)
  auto operation_context = std::shared_ptr<OperationContext>(
      new OperationContext({}, {}, {fake_metric}, clock));
#else
  auto operation_context = std::make_shared<OperationContext>();
#endif

  auto mock = std::make_shared<MockBigtableStub>();
  EXPECT_CALL(*mock, AsyncSampleRowKeys)
      .WillOnce([&p, this](CompletionQueue const&, auto client_context, auto,
                           v2::SampleRowKeysRequest const& request) {
        metadata_fixture_.SetServerMetadata(*client_context, {});
        EXPECT_EQ(kAppProfile, request.app_profile_id());
        EXPECT_EQ(kTableName, request.table_name());
        auto stream = std::make_unique<MockAsyncSampleRowKeysStream>();
        ::testing::InSequence s;
        EXPECT_CALL(*stream, Start).WillOnce([] {
          return make_ready_future(true);
        });
        EXPECT_CALL(*stream, Read)
            .WillOnce([] {
              return make_ready_future(MakeResponse("forgotten1", 11));
            })
            // We block here so the caller can cancel the request. The value
            // returned will be a response, meaning the stream is still active
            // and needs to be drained.
            .WillOnce([&p] { return p.get_future(); });
        EXPECT_CALL(*stream, Cancel);
        EXPECT_CALL(*stream, Read)
            .WillOnce(
                [] { return make_ready_future(MakeResponse("discarded", 33)); })
            .WillOnce([] {
              return make_ready_future(
                  absl::optional<v2::SampleRowKeysResponse>{});
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

  MockFunction<void(grpc::ClientContext&)> mock_setup;
  EXPECT_CALL(mock_setup, Call).Times(1);
  internal::OptionsSpan span(
      Options{}.set<internal::GrpcSetupOption>(mock_setup.AsStdFunction()));

  auto fut = AsyncRowSampler::Create(cq, mock, std::move(retry),
                                     std::move(mock_b), false, kAppProfile,
                                     kTableName, std::move(operation_context));
  // Cancel the call after performing one read of this test stream.
  fut.cancel();
  // Proceed with the rest of the stream. In this test, there are more responses
  // to be read, which we must drain. The client call should fail.
  p.set_value(MakeResponse("forgotten2", 22));
  auto sor = fut.get();
  EXPECT_THAT(sor,
              StatusIs(StatusCode::kCancelled, HasSubstr("User cancelled")));
}

TEST_F(AsyncSampleRowKeysTest, CurrentOptionsContinuedOnRetries) {
  struct TestOption {
    using Type = int;
  };

  auto mock = std::make_shared<MockBigtableStub>();
  EXPECT_CALL(*mock, AsyncSampleRowKeys)
      .Times(2)
      .WillRepeatedly([this](CompletionQueue const&, auto context, auto,
                             v2::SampleRowKeysRequest const&) {
        EXPECT_EQ(5, internal::CurrentOptions().get<TestOption>());
        metadata_fixture_.SetServerMetadata(*context);
        auto stream = std::make_unique<MockAsyncSampleRowKeysStream>();
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

  MockFunction<void(grpc::ClientContext&)> mock_setup;
  EXPECT_CALL(mock_setup, Call).Times(2);

  internal::OptionsSpan span(
      Options{}
          .set<internal::GrpcSetupOption>(mock_setup.AsStdFunction())
          .set<TestOption>(5));
  auto fut = AsyncRowSampler::Create(
      cq, mock, std::move(retry), std::move(mock_b), false, kAppProfile,
      kTableName, std::make_shared<OperationContext>());

  // Simulate the timer being satisfied in a thread with different prevailing
  // options than the calling thread.
  internal::OptionsSpan clear(Options{});
  timer_promise.set_value(make_status_or(std::chrono::system_clock::now()));
}

TEST_F(AsyncSampleRowKeysTest, BigtableCookie) {
  auto mock = std::make_shared<MockBigtableStub>();
  EXPECT_CALL(*mock, AsyncSampleRowKeys)
      .WillOnce([this](CompletionQueue const&, auto context, auto,
                       v2::SampleRowKeysRequest const&) {
        // Return a bigtable cookie in the first request.
        metadata_fixture_.SetServerMetadata(
            *context, {{}, {{"x-goog-cbt-cookie-routing", "routing"}}});
        auto stream = std::make_unique<MockAsyncSampleRowKeysStream>();
        EXPECT_CALL(*stream, Start).WillOnce([] {
          return make_ready_future(false);
        });
        EXPECT_CALL(*stream, Finish).WillOnce([] {
          return make_ready_future(internal::UnavailableError("try again"));
        });
        return stream;
      })
      .WillOnce([this](CompletionQueue const&, auto context, auto,
                       v2::SampleRowKeysRequest const&) {
        // Verify that the next request includes the bigtable cookie from above.
        auto headers = metadata_fixture_.GetMetadata(*context);
        EXPECT_THAT(headers,
                    Contains(Pair("x-goog-cbt-cookie-routing", "routing")));
        auto stream = std::make_unique<MockAsyncSampleRowKeysStream>();
        EXPECT_CALL(*stream, Start).WillOnce([] {
          return make_ready_future(false);
        });
        EXPECT_CALL(*stream, Finish).WillOnce([] {
          return make_ready_future(internal::PermissionDeniedError("fail"));
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

  auto sor = AsyncRowSampler::Create(
                 cq, mock, std::move(retry), std::move(mock_b), false,
                 kAppProfile, kTableName, std::make_shared<OperationContext>())
                 .get();

  EXPECT_THAT(sor, StatusIs(StatusCode::kPermissionDenied, HasSubstr("fail")));
}

#ifdef GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY
using ::google::cloud::testing_util::EnableTracing;
using ::google::cloud::testing_util::IsActive;
using ::google::cloud::testing_util::SpanNamed;
using ::testing::AllOf;
using ::testing::Each;
using ::testing::SizeIs;
using ErrorStream =
    internal::AsyncStreamingReadRpcError<v2::SampleRowKeysResponse>;

TEST_F(AsyncSampleRowKeysTest, TracedBackoff) {
  auto span_catcher = testing_util::InstallSpanCatcher();

  auto mock = std::make_shared<MockBigtableStub>();
  EXPECT_CALL(*mock, AsyncSampleRowKeys)
      .Times(kNumRetries + 1)
      .WillRepeatedly([this](auto&, auto context, auto, auto const&) {
        metadata_fixture_.SetServerMetadata(*context, {});
        return std::make_unique<ErrorStream>(
            internal::UnavailableError("try again"));
      });

  internal::AutomaticallyCreatedBackgroundThreads background;
  auto retry = DataLimitedErrorCountRetryPolicy(kNumRetries).clone();
  auto mock_b = std::make_unique<MockBackoffPolicy>();
  EXPECT_CALL(*mock_b, OnCompletion).Times(kNumRetries);

  internal::OptionsSpan o(EnableTracing(Options{}));
  (void)AsyncRowSampler::Create(
      background.cq(), mock, std::move(retry), std::move(mock_b), false,
      kAppProfile, kTableName, std::make_shared<OperationContext>())
      .get();

  EXPECT_THAT(span_catcher->GetSpans(),
              AllOf(SizeIs(kNumRetries), Each(SpanNamed("Async Backoff"))));
}

TEST_F(AsyncSampleRowKeysTest, CallSpanActiveThroughout) {
  auto span_catcher = testing_util::InstallSpanCatcher();

  auto span = internal::MakeSpan("span");

  auto mock = std::make_shared<MockBigtableStub>();
  EXPECT_CALL(*mock, AsyncSampleRowKeys)
      .Times(kNumRetries + 1)
      .WillRepeatedly([this, span](auto&, auto context, auto, auto const&) {
        metadata_fixture_.SetServerMetadata(*context, {});
        EXPECT_THAT(span, IsActive());
        return std::make_unique<ErrorStream>(
            internal::UnavailableError("try again"));
      });

  internal::AutomaticallyCreatedBackgroundThreads background;
  auto retry = DataLimitedErrorCountRetryPolicy(kNumRetries).clone();
  auto mock_b = std::make_unique<MockBackoffPolicy>();
  EXPECT_CALL(*mock_b, OnCompletion).Times(kNumRetries);

  internal::OTelScope scope(span);
  internal::OptionsSpan o(EnableTracing(Options{}));
  auto f = AsyncRowSampler::Create(
      background.cq(), mock, std::move(retry), std::move(mock_b), false,
      kAppProfile, kTableName, std::make_shared<OperationContext>());

  auto overlay = opentelemetry::trace::Scope(internal::MakeSpan("overlay"));
  (void)f.get();
}
#endif  // GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable_internal
}  // namespace cloud
}  // namespace google
