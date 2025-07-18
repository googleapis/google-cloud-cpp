// Copyright 2024 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "google/cloud/bigtable/internal/operation_context.h"
#include "google/cloud/bigtable/internal/metrics.h"
#include "google/cloud/testing_util/fake_clock.h"
#include "google/cloud/testing_util/validate_metadata.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace bigtable_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::google::cloud::testing_util::FakeSteadyClock;

using ::testing::Eq;
using ::testing::Pair;
using ::testing::UnorderedElementsAre;

/*
 * Use a test fixture because initializing `ValidateMetadataFixture` once is
 * slightly more performant than creating one per test.
 */
class OperationContextTest : public ::testing::Test {
 protected:
  testing_util::ValidateMetadataFixture metadata_fixture_;

  /**
   * Simulate receiving server metadata, using the given `OperationContext`.
   *
   * Returns the headers that would be set by the `OperationContext`, before the
   * next RPC.
   *
   * While it may seem odd to simulate the second half of an RPC, and the first
   * half of another, it makes the tests simpler.
   */
  std::multimap<std::string, std::string> SimulateRequest(
      OperationContext& operation_context,
      RpcMetadata const& server_metadata = {}) {
    grpc::ClientContext c1;
    metadata_fixture_.SetServerMetadata(c1, server_metadata);
    operation_context.PostCall(c1, {});

    // Return the headers set by the client.
    grpc::ClientContext c2;
    operation_context.PreCall(c2);
    return metadata_fixture_.GetMetadata(c2);
  }
};

TEST_F(OperationContextTest, StartsWithoutBigtableCookies) {
  OperationContext operation_context;

  grpc::ClientContext c;
  operation_context.PreCall(c);
  auto headers = metadata_fixture_.GetMetadata(c);
  EXPECT_THAT(headers, UnorderedElementsAre(Pair("bigtable-attempt", "0")));
}

TEST_F(OperationContextTest, ParrotsBigtableCookies) {
  OperationContext operation_context;

  RpcMetadata server_metadata;
  server_metadata.headers = {
      {"ignored-key-header", "ignored-value"},
      {"x-goog-cbt-cookie-header-only", "header"},
      {"x-goog-cbt-cookie-both", "header"},
  };
  server_metadata.trailers = {
      {"ignored-key-trailer", "ignored-value"},
      {"x-goog-cbt-cookie-trailer-only", "trailer"},
      {"x-goog-cbt-cookie-both", "trailer"},
  };

  auto headers = SimulateRequest(operation_context, server_metadata);
  EXPECT_THAT(headers, UnorderedElementsAre(
                           Pair("x-goog-cbt-cookie-header-only", "header"),
                           Pair("x-goog-cbt-cookie-trailer-only", "trailer"),
                           Pair("x-goog-cbt-cookie-both", "trailer"),
                           Pair("bigtable-attempt", "0")));
}

TEST_F(OperationContextTest, Retries) {
  OperationContext operation_context;

  auto headers = SimulateRequest(
      operation_context, {{}, {{"x-goog-cbt-cookie-routing", "request-0"}}});
  EXPECT_THAT(headers, UnorderedElementsAre(
                           Pair("x-goog-cbt-cookie-routing", "request-0"),
                           Pair("bigtable-attempt", "0")));

  // Simulate receiving no `RpcMetadata` from the server. We should remember the
  // cookie from the first response.
  headers = SimulateRequest(operation_context, {});
  EXPECT_THAT(headers, UnorderedElementsAre(
                           Pair("x-goog-cbt-cookie-routing", "request-0"),
                           Pair("bigtable-attempt", "1")));

  // Simulate receiving a new routing cookie. We should overwrite the cookie
  // from the first response.
  headers = SimulateRequest(operation_context,
                            {{}, {{"x-goog-cbt-cookie-routing", "request-2"}}});
  EXPECT_THAT(headers, UnorderedElementsAre(
                           Pair("x-goog-cbt-cookie-routing", "request-2"),
                           Pair("bigtable-attempt", "2")));
}

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

TEST(OperationContextMetricTest, MetricPreCall) {
  auto first_attempt = std::chrono::steady_clock::now();
  auto mock_metric = std::make_unique<MockMetric>();

  EXPECT_CALL(*mock_metric, PreCall)
      .WillOnce(
          [&](opentelemetry::context::Context const&, PreCallParams const& p) {
            EXPECT_THAT(p.attempt_start, Eq(first_attempt));
            EXPECT_TRUE(p.first_attempt);
          })
      .WillOnce(
          [&](opentelemetry::context::Context const&, PreCallParams const& p) {
            EXPECT_THAT(p.attempt_start,
                        Eq(first_attempt + std::chrono::milliseconds(5)));
            EXPECT_FALSE(p.first_attempt);
          });

  auto fake_metric = std::make_shared<CloningMetric>(std::move(mock_metric));
  auto clock = std::make_shared<FakeSteadyClock>();
  OperationContext operation_context({}, {}, {fake_metric}, clock);
  grpc::ClientContext client_context;

  clock->SetTime(first_attempt);
  operation_context.PreCall(client_context);
  clock->AdvanceTime(std::chrono::milliseconds(5));
  operation_context.PreCall(client_context);
}

TEST(OperationContextMetricTest, MetricPostCall) {
  auto attempt_end = std::chrono::steady_clock::now();
  Status status{StatusCode::kUnavailable, "unavailable"};
  auto mock_metric = std::make_unique<MockMetric>();

  EXPECT_CALL(*mock_metric, PostCall)
      .WillOnce([&](opentelemetry::context::Context const&,
                    grpc::ClientContext const&, PostCallParams const& p) {
        EXPECT_THAT(p.attempt_end, Eq(attempt_end));
        EXPECT_THAT(p.attempt_status, Eq(status));
      });

  auto fake_metric = std::make_shared<CloningMetric>(std::move(mock_metric));
  auto clock = std::make_shared<FakeSteadyClock>();
  OperationContext operation_context({}, {}, {fake_metric}, clock);

  testing_util::ValidateMetadataFixture metadata_fixture;
  grpc::ClientContext client_context;
  RpcMetadata server_metadata;
  metadata_fixture.SetServerMetadata(client_context, server_metadata);

  clock->SetTime(attempt_end);
  operation_context.PostCall(client_context, status);
}

TEST(OperationContextMetricTest, MetricOnDone) {
  auto operation_end = std::chrono::steady_clock::now();
  Status status{StatusCode::kUnavailable, "unavailable"};
  auto mock_metric = std::make_unique<MockMetric>();

  EXPECT_CALL(*mock_metric, OnDone)
      .WillOnce(
          [&](opentelemetry::context::Context const&, OnDoneParams const& p) {
            EXPECT_THAT(p.operation_end, Eq(operation_end));
            EXPECT_THAT(p.operation_status, Eq(status));
          });

  auto fake_metric = std::make_shared<CloningMetric>(std::move(mock_metric));
  auto clock = std::make_shared<FakeSteadyClock>();
  OperationContext operation_context({}, {}, {fake_metric}, clock);

  clock->SetTime(operation_end);
  operation_context.OnDone(status);
}

TEST(OperationContextMetricTest, MetricElementRequest) {
  auto element_request = std::chrono::steady_clock::now();
  auto mock_metric = std::make_unique<MockMetric>();

  EXPECT_CALL(*mock_metric, ElementRequest)
      .WillOnce([&](opentelemetry::context::Context const&,
                    ElementRequestParams const& p) {
        EXPECT_THAT(p.element_request, Eq(element_request));
      });

  auto fake_metric = std::make_shared<CloningMetric>(std::move(mock_metric));
  auto clock = std::make_shared<FakeSteadyClock>();
  OperationContext operation_context({}, {}, {fake_metric}, clock);
  grpc::ClientContext client_context;

  clock->SetTime(element_request);
  operation_context.ElementRequest(client_context);
}

TEST(OperationContextMetricTest, MetricElementDelivery) {
  auto element_delivery = std::chrono::steady_clock::now();
  auto mock_metric = std::make_unique<MockMetric>();

  EXPECT_CALL(*mock_metric, ElementDelivery)
      .WillOnce([&](opentelemetry::context::Context const&,
                    ElementDeliveryParams const& p) {
        EXPECT_THAT(p.element_delivery, Eq(element_delivery));
        EXPECT_TRUE(p.first_response);
      })
      .WillOnce([&](opentelemetry::context::Context const&,
                    ElementDeliveryParams const& p) {
        EXPECT_THAT(p.element_delivery,
                    Eq(element_delivery + std::chrono::milliseconds(5)));
        EXPECT_FALSE(p.first_response);
      });

  auto fake_metric = std::make_shared<CloningMetric>(std::move(mock_metric));
  auto clock = std::make_shared<FakeSteadyClock>();
  OperationContext operation_context({}, {}, {fake_metric}, clock);
  grpc::ClientContext client_context;

  clock->SetTime(element_delivery);
  operation_context.ElementDelivery(client_context);
  clock->AdvanceTime(std::chrono::milliseconds(5));
  operation_context.ElementDelivery(client_context);
}

#endif  // GOOGLE_CLOUD_CPP_BIGTABLE_WITH_OTEL_METRICS

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable_internal
}  // namespace cloud
}  // namespace google
