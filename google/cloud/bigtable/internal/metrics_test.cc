// Copyright 2025 Google LLC
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

#ifdef GOOGLE_CLOUD_CPP_BIGTABLE_WITH_OTEL_METRICS

#include "google/cloud/bigtable/internal/metrics.h"
#include "google/cloud/bigtable/version.h"
#include "google/cloud/testing_util/fake_clock.h"
#include "google/cloud/testing_util/validate_metadata.h"
#include <gmock/gmock.h>
#include <opentelemetry/context/runtime_context.h>

namespace google {
namespace cloud {
namespace bigtable_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::google::cloud::testing_util::FakeSteadyClock;
using ::google::cloud::testing_util::SetServerMetadata;

using ::testing::A;
using ::testing::Eq;
using ::testing::Pair;
using ::testing::UnorderedElementsAre;

using ::opentelemetry::metrics::Counter;
#if OPENTELEMETRY_ABI_VERSION_NO >= 2
using ::opentelemetry::metrics::Gauge;
#endif
using ::opentelemetry::metrics::Histogram;
using ::opentelemetry::metrics::ObservableInstrument;
using ::opentelemetry::metrics::UpDownCounter;

template <typename T>
class MockHistogram : public opentelemetry::metrics::Histogram<T> {
 public:
#if OPENTELEMETRY_ABI_VERSION_NO >= 2
  MOCK_METHOD(void, Record,  // NOLINT(bugprone-exception-escape)
              (T value), (noexcept, override));
  MOCK_METHOD(void, Record,  // NOLINT(bugprone-exception-escape)
              (T, opentelemetry::common::KeyValueIterable const&),
              (noexcept, override));

#endif
  MOCK_METHOD(void, Record,  // NOLINT(bugprone-exception-escape)
              (T value, opentelemetry::context::Context const& context),
              (noexcept, override));
  MOCK_METHOD(void, Record,  // NOLINT(bugprone-exception-escape)
              (T value,
               opentelemetry::common::KeyValueIterable const& attributes,
               opentelemetry::context::Context const& context),
              (noexcept, override));
};

template <typename T>
class MockCounter : public opentelemetry::metrics::Counter<T> {
 public:
  MOCK_METHOD(void, Add,  // NOLINT(bugprone-exception-escape)
              (T value), (noexcept, override));
  MOCK_METHOD(void, Add,  // NOLINT(bugprone-exception-escape)
              (T value, opentelemetry::context::Context const&),
              (noexcept, override));
  MOCK_METHOD(void, Add,  // NOLINT(bugprone-exception-escape)
              (T value, opentelemetry::common::KeyValueIterable const&),
              (noexcept, override));
  MOCK_METHOD(void, Add,  // NOLINT(bugprone-exception-escape)
              (T value, opentelemetry::common::KeyValueIterable const&,
               opentelemetry::context::Context const&),
              (noexcept, override));
};

class MockMeter : public opentelemetry::metrics::Meter {
 public:
  MOCK_METHOD(opentelemetry::nostd::unique_ptr<Counter<uint64_t>>,
              CreateUInt64Counter,  // NOLINT(bugprone-exception-escape)
              (opentelemetry::nostd::string_view,
               opentelemetry::nostd::string_view,
               opentelemetry::nostd::string_view),
              (noexcept, override));

  MOCK_METHOD(opentelemetry::nostd::unique_ptr<Counter<double>>,
              CreateDoubleCounter,  // NOLINT(bugprone-exception-escape)
              (opentelemetry::nostd::string_view,
               opentelemetry::nostd::string_view,
               opentelemetry::nostd::string_view),
              (noexcept, override));

  MOCK_METHOD(
      opentelemetry::nostd::shared_ptr<ObservableInstrument>,
      CreateInt64ObservableCounter,  // NOLINT(bugprone-exception-escape)
      (opentelemetry::nostd::string_view, opentelemetry::nostd::string_view,
       opentelemetry::nostd::string_view),
      (noexcept, override));

  MOCK_METHOD(
      opentelemetry::nostd::shared_ptr<ObservableInstrument>,
      CreateDoubleObservableCounter,  // NOLINT(bugprone-exception-escape)
      (opentelemetry::nostd::string_view, opentelemetry::nostd::string_view,
       opentelemetry::nostd::string_view),
      (noexcept, override));

  MOCK_METHOD(opentelemetry::nostd::unique_ptr<Histogram<uint64_t>>,
              CreateUInt64Histogram,  // NOLINT(bugprone-exception-escape)
              (opentelemetry::nostd::string_view,
               opentelemetry::nostd::string_view,
               opentelemetry::nostd::string_view),
              (noexcept, override));

  MOCK_METHOD(opentelemetry::nostd::unique_ptr<Histogram<double>>,
              CreateDoubleHistogram,  // NOLINT(bugprone-exception-escape)
              (opentelemetry::nostd::string_view,
               opentelemetry::nostd::string_view,
               opentelemetry::nostd::string_view),
              (noexcept, override));

#if OPENTELEMETRY_ABI_VERSION_NO >= 2
  MOCK_METHOD(opentelemetry::nostd::unique_ptr<Gauge<int64_t>>,
              CreateInt64Gauge,  // NOLINT(bugprone-exception-escape)
              (opentelemetry::nostd::string_view,
               opentelemetry::nostd::string_view,
               opentelemetry::nostd::string_view),
              (noexcept, override));

  MOCK_METHOD(opentelemetry::nostd::unique_ptr<Gauge<double>>,
              CreateDoubleGauge,  // NOLINT(bugprone-exception-escape)
              (opentelemetry::nostd::string_view,
               opentelemetry::nostd::string_view,
               opentelemetry::nostd::string_view),
              (noexcept, override));
#endif

  MOCK_METHOD(opentelemetry::nostd::shared_ptr<ObservableInstrument>,
              CreateInt64ObservableGauge,  // NOLINT(bugprone-exception-escape)
              (opentelemetry::nostd::string_view,
               opentelemetry::nostd::string_view,
               opentelemetry::nostd::string_view),
              (noexcept, override));

  MOCK_METHOD(opentelemetry::nostd::shared_ptr<ObservableInstrument>,
              CreateDoubleObservableGauge,  // NOLINT(bugprone-exception-escape)
              (opentelemetry::nostd::string_view,
               opentelemetry::nostd::string_view,
               opentelemetry::nostd::string_view),
              (noexcept, override));

  MOCK_METHOD(opentelemetry::nostd::unique_ptr<UpDownCounter<int64_t>>,
              CreateInt64UpDownCounter,  // NOLINT(bugprone-exception-escape)
              (opentelemetry::nostd::string_view,
               opentelemetry::nostd::string_view,
               opentelemetry::nostd::string_view),
              (noexcept, override));

  MOCK_METHOD(opentelemetry::nostd::unique_ptr<UpDownCounter<double>>,
              CreateDoubleUpDownCounter,  // NOLINT(bugprone-exception-escape)
              (opentelemetry::nostd::string_view,
               opentelemetry::nostd::string_view,
               opentelemetry::nostd::string_view),
              (noexcept, override));

  MOCK_METHOD(
      opentelemetry::nostd::shared_ptr<ObservableInstrument>,
      CreateInt64ObservableUpDownCounter,  // NOLINT(bugprone-exception-escape)
      (opentelemetry::nostd::string_view, opentelemetry::nostd::string_view,
       opentelemetry::nostd::string_view),
      (noexcept, override));

  MOCK_METHOD(
      opentelemetry::nostd::shared_ptr<ObservableInstrument>,
      CreateDoubleObservableUpDownCounter,  // NOLINT(bugprone-exception-escape)
      (opentelemetry::nostd::string_view, opentelemetry::nostd::string_view,
       opentelemetry::nostd::string_view),
      (noexcept, override));
};

class MockMeterProvider : public opentelemetry::metrics::MeterProvider {
 public:
#if OPENTELEMETRY_ABI_VERSION_NO >= 2
  MOCK_METHOD(opentelemetry::nostd::shared_ptr<opentelemetry::metrics::Meter>,
              GetMeter,  // NOLINT(bugprone-exception-escape)
              (opentelemetry::nostd::string_view,
               opentelemetry::nostd::string_view,
               opentelemetry::nostd::string_view,
               opentelemetry::common::KeyValueIterable const*),
              (noexcept, override));

  MOCK_METHOD(void, RemoveMeter,  // NOLINT(bugprone-exception-escape)
              (opentelemetry::nostd::string_view,
               opentelemetry::nostd::string_view,
               opentelemetry::nostd::string_view),
              (noexcept, override));

#else
  MOCK_METHOD(opentelemetry::nostd::shared_ptr<opentelemetry::metrics::Meter>,
              GetMeter,  // NOLINT(bugprone-exception-escape)
              (opentelemetry::nostd::string_view,
               opentelemetry::nostd::string_view,
               opentelemetry::nostd::string_view),
              (noexcept, override));
#endif
};

TEST(LabelMap, IntoLabelMap) {
  ResourceLabels r{"my-project", "my-instance", "my-table", "my-cluster",
                   "my-zone"};
  DataLabels d{"my-method",     "my-streaming",   "my-client-name",
               "my-client-uid", "my-app-profile", "my-status"};
  auto label_map = IntoLabelMap(r, d);
  EXPECT_THAT(
      label_map,
      UnorderedElementsAre(
          Pair("project_id", "my-project"), Pair("instance", "my-instance"),
          Pair("table", "my-table"), Pair("cluster", "my-cluster"),
          Pair("zone", "my-zone"), Pair("method", "my-method"),
          Pair("streaming", "my-streaming"),
          Pair("client_name", "my-client-name"),
          Pair("client_uid", "my-client-uid"),
          Pair("app_profile", "my-app-profile"), Pair("status", "my-status")));
}

TEST(GetResponseParamsFromMetadata, NonEmptyHeader) {
  google::bigtable::v2::ResponseParams expected_response_params;
  expected_response_params.set_cluster_id("my-cluster");
  expected_response_params.set_zone_id("my-zone");
  grpc::ClientContext client_context;
  RpcMetadata server_metadata;
  server_metadata.trailers.emplace(
      "x-goog-ext-425905942-bin", expected_response_params.SerializeAsString());
  SetServerMetadata(client_context, server_metadata);

  auto result = GetResponseParamsFromTrailingMetadata(client_context);
  ASSERT_TRUE(result);
  EXPECT_THAT(result->cluster_id(), Eq("my-cluster"));
  EXPECT_THAT(result->zone_id(), Eq("my-zone"));
}

TEST(GetResponseParamsFromMetadata, EmptyHeader) {
  grpc::ClientContext client_context;
  RpcMetadata server_metadata;
  SetServerMetadata(client_context, server_metadata);

  auto result = GetResponseParamsFromTrailingMetadata(client_context);
  EXPECT_FALSE(result);
}

std::unordered_map<std::string, std::string> MakeAttributesMap(
    opentelemetry::common::KeyValueIterable const& attributes) {
  std::unordered_map<std::string, std::string> m;
  attributes.ForEachKeyValue([&](opentelemetry::nostd::string_view k,
                                 opentelemetry::common::AttributeValue v) {
    if (opentelemetry::nostd::holds_alternative<
            opentelemetry::nostd::string_view>(v)) {
      m.emplace(
          std::string{k},
          opentelemetry::nostd::get<opentelemetry::nostd::string_view>(v));
    }
    return true;
  });
  return m;
}

void SetClusterZone(grpc::ClientContext& client_context,
                    std::string const& cluster, std::string const& zone) {
  google::bigtable::v2::ResponseParams expected_response_params;
  expected_response_params.set_cluster_id(cluster);
  expected_response_params.set_zone_id(zone);
  RpcMetadata server_metadata;
  server_metadata.trailers.emplace(
      "x-goog-ext-425905942-bin", expected_response_params.SerializeAsString());
  SetServerMetadata(client_context, server_metadata);
}

TEST(OperationLatencyTest, FirstAttemptSuccess) {
  auto mock_histogram = std::make_unique<MockHistogram<double>>();
  EXPECT_CALL(
      *mock_histogram,
      Record(A<double>(), A<opentelemetry::common::KeyValueIterable const&>(),
             A<opentelemetry::context::Context const&>()))
      .WillOnce([](double value,
                   opentelemetry::common::KeyValueIterable const& attributes,
                   opentelemetry::context::Context const&) {
        EXPECT_THAT(value, Eq(10.0));
        EXPECT_THAT(
            MakeAttributesMap(attributes),
            UnorderedElementsAre(
                Pair("project_id", "my-project-id"),
                Pair("instance", "my-instance"), Pair("cluster", "my-cluster"),
                Pair("table", "my-table"), Pair("zone", "my-zone"),
                Pair("method", "my-method"), Pair("streaming", "my-streaming"),
                Pair("status", "OK"), Pair("client_name", "my-client-name"),
                Pair("client_uid", "my-client-uid"),
                Pair("app_profile", "my-app-profile")));
      });

  opentelemetry::nostd::shared_ptr<MockMeter> mock_meter =
      std::make_shared<MockMeter>();
  EXPECT_CALL(*mock_meter, CreateDoubleHistogram)
      .WillOnce([mock = std::move(mock_histogram)](
                    opentelemetry::nostd::string_view name,
                    opentelemetry::nostd::string_view,
                    opentelemetry::nostd::string_view) mutable {
        EXPECT_THAT(name, Eq("operation_latencies"));
        return std::move(mock);
      });

  opentelemetry::nostd::shared_ptr<MockMeterProvider> mock_provider =
      std::make_shared<MockMeterProvider>();
  EXPECT_CALL(*mock_provider, GetMeter)
#if OPENTELEMETRY_ABI_VERSION_NO >= 2
      .WillOnce([&](opentelemetry::nostd::string_view scope,
                    opentelemetry::nostd::string_view scope_version,
                    opentelemetry::nostd::string_view,
                    opentelemetry::common::KeyValueIterable const*) mutable {
#else
      .WillOnce([&](opentelemetry::nostd::string_view scope,
                    opentelemetry::nostd::string_view scope_version,
                    opentelemetry::nostd::string_view) mutable {
#endif
        EXPECT_THAT(scope, Eq("my-instrument-scope"));
        EXPECT_THAT(scope_version, Eq("v1"));
        return mock_meter;
      });

  OperationLatency operation_latency("my-instrument-scope", mock_provider);
  ResourceLabels resource_labels{"my-project-id", "my-instance", "my-table", "",
                                 ""};
  DataLabels data_labels{"my-method",     "my-streaming",   "my-client-name",
                         "my-client-uid", "my-app-profile", ""};
  auto clone = operation_latency.clone(resource_labels, data_labels);

  grpc::ClientContext client_context;
  SetClusterZone(client_context, "my-cluster", "my-zone");

  auto otel_context = opentelemetry::context::RuntimeContext::GetCurrent();
  auto clock = std::make_shared<FakeSteadyClock>();

  clock->SetTime(std::chrono::steady_clock::now());
  clone->PreCall(otel_context, {clock->Now(), true});
  clock->AdvanceTime(std::chrono::milliseconds(5));
  clone->PostCall(otel_context, client_context,
                  {clock->Now(), Status{StatusCode::kOk, "ok"}});
  clock->AdvanceTime(std::chrono::milliseconds(5));
  clone->OnDone(otel_context, {clock->Now(), Status{StatusCode::kOk, "ok"}});
}

TEST(OperationLatencyTest, ThirdAttemptSuccess) {
  auto mock_histogram = std::make_unique<MockHistogram<double>>();
  EXPECT_CALL(
      *mock_histogram,
      Record(A<double>(), A<opentelemetry::common::KeyValueIterable const&>(),
             A<opentelemetry::context::Context const&>()))
      .WillOnce([](double value,
                   opentelemetry::common::KeyValueIterable const& attributes,
                   opentelemetry::context::Context const&) {
        EXPECT_THAT(value, Eq(20.0));
        EXPECT_THAT(
            MakeAttributesMap(attributes),
            UnorderedElementsAre(
                Pair("project_id", "my-project-id"),
                Pair("instance", "my-instance"), Pair("cluster", "my-cluster"),
                Pair("table", "my-table"), Pair("zone", "my-zone"),
                Pair("method", "my-method"), Pair("streaming", "my-streaming"),
                Pair("status", "OK"), Pair("client_name", "my-client-name"),
                Pair("client_uid", "my-client-uid"),
                Pair("app_profile", "my-app-profile")));
      });

  opentelemetry::nostd::shared_ptr<MockMeter> mock_meter =
      std::make_shared<MockMeter>();
  EXPECT_CALL(*mock_meter, CreateDoubleHistogram)
      .WillOnce([mock = std::move(mock_histogram)](
                    opentelemetry::nostd::string_view name,
                    opentelemetry::nostd::string_view,
                    opentelemetry::nostd::string_view) mutable {
        EXPECT_THAT(name, Eq("operation_latencies"));
        return std::move(mock);
      });

  opentelemetry::nostd::shared_ptr<MockMeterProvider> mock_provider =
      std::make_shared<MockMeterProvider>();
  EXPECT_CALL(*mock_provider, GetMeter)
#if OPENTELEMETRY_ABI_VERSION_NO >= 2
      .WillOnce([&](opentelemetry::nostd::string_view scope,
                    opentelemetry::nostd::string_view scope_version,
                    opentelemetry::nostd::string_view,
                    opentelemetry::common::KeyValueIterable const*) mutable {
#else
      .WillOnce([&](opentelemetry::nostd::string_view scope,
                    opentelemetry::nostd::string_view scope_version,
                    opentelemetry::nostd::string_view) mutable {
#endif
        EXPECT_THAT(scope, Eq("my-instrument-scope"));
        EXPECT_THAT(scope_version, Eq("v1"));
        return mock_meter;
      });

  OperationLatency operation_latency("my-instrument-scope", mock_provider);
  ResourceLabels resource_labels{"my-project-id", "my-instance", "my-table", "",
                                 ""};
  DataLabels data_labels{"my-method",     "my-streaming",   "my-client-name",
                         "my-client-uid", "my-app-profile", ""};
  auto clone = operation_latency.clone(resource_labels, data_labels);

  grpc::ClientContext client_context;
  SetClusterZone(client_context, "my-cluster", "my-zone");

  auto otel_context = opentelemetry::context::RuntimeContext::GetCurrent();
  auto clock = std::make_shared<FakeSteadyClock>();

  clock->SetTime(std::chrono::steady_clock::now());

  clone->PreCall(otel_context, {clock->Now(), true});
  clock->AdvanceTime(std::chrono::milliseconds(5));
  clone->PostCall(
      otel_context, client_context,
      {clock->Now(), Status{StatusCode::kUnavailable, "unavailable"}});

  clone->PreCall(otel_context, {clock->Now(), false});
  clock->AdvanceTime(std::chrono::milliseconds(5));
  clone->PostCall(
      otel_context, client_context,
      {clock->Now(), Status{StatusCode::kUnavailable, "unavailable"}});

  clone->PreCall(otel_context, {clock->Now(), false});
  clock->AdvanceTime(std::chrono::milliseconds(5));
  clone->PostCall(otel_context, client_context,
                  {clock->Now(), Status{StatusCode::kOk, "ok"}});

  clock->AdvanceTime(std::chrono::milliseconds(5));
  clone->OnDone(otel_context, {clock->Now(), Status{StatusCode::kOk, "ok"}});
}

TEST(AttemptLatencyTest, NoRetry) {
  auto mock_histogram = std::make_unique<MockHistogram<double>>();
  EXPECT_CALL(
      *mock_histogram,
      Record(A<double>(), A<opentelemetry::common::KeyValueIterable const&>(),
             A<opentelemetry::context::Context const&>()))
      .WillOnce([](double value,
                   opentelemetry::common::KeyValueIterable const& attributes,
                   opentelemetry::context::Context const&) {
        EXPECT_THAT(value, Eq(1.234));
        EXPECT_THAT(
            MakeAttributesMap(attributes),
            UnorderedElementsAre(
                Pair("project_id", "my-project-id"),
                Pair("instance", "my-instance"), Pair("cluster", "my-cluster"),
                Pair("table", "my-table"), Pair("zone", "my-zone"),
                Pair("method", "my-method"), Pair("streaming", "my-streaming"),
                Pair("status", "OK"), Pair("client_name", "my-client-name"),
                Pair("client_uid", "my-client-uid"),
                Pair("app_profile", "my-app-profile")));
      });

  opentelemetry::nostd::shared_ptr<MockMeter> mock_meter =
      std::make_shared<MockMeter>();
  EXPECT_CALL(*mock_meter, CreateDoubleHistogram)
      .WillOnce([mock = std::move(mock_histogram)](
                    opentelemetry::nostd::string_view name,
                    opentelemetry::nostd::string_view,
                    opentelemetry::nostd::string_view) mutable {
        EXPECT_THAT(name, Eq("attempt_latencies"));
        return std::move(mock);
      });

  opentelemetry::nostd::shared_ptr<MockMeterProvider> mock_provider =
      std::make_shared<MockMeterProvider>();
  EXPECT_CALL(*mock_provider, GetMeter)
#if OPENTELEMETRY_ABI_VERSION_NO >= 2
      .WillOnce([&](opentelemetry::nostd::string_view scope,
                    opentelemetry::nostd::string_view scope_version,
                    opentelemetry::nostd::string_view,
                    opentelemetry::common::KeyValueIterable const*) mutable {
#else
      .WillOnce([&](opentelemetry::nostd::string_view scope,
                    opentelemetry::nostd::string_view scope_version,
                    opentelemetry::nostd::string_view) mutable {
#endif
        EXPECT_THAT(scope, Eq("my-instrument-scope"));
        EXPECT_THAT(scope_version, Eq("v1"));
        return mock_meter;
      });

  AttemptLatency attempt_latency("my-instrument-scope", mock_provider);
  ResourceLabels resource_labels{"my-project-id", "my-instance", "my-table", "",
                                 ""};
  DataLabels data_labels{"my-method",     "my-streaming",   "my-client-name",
                         "my-client-uid", "my-app-profile", ""};
  auto clone = attempt_latency.clone(resource_labels, data_labels);

  grpc::ClientContext client_context;
  SetClusterZone(client_context, "my-cluster", "my-zone");

  auto otel_context = opentelemetry::context::RuntimeContext::GetCurrent();
  auto clock = std::make_shared<FakeSteadyClock>();

  clock->SetTime(std::chrono::steady_clock::now());
  clone->PreCall(otel_context, {clock->Now(), true});
  clock->AdvanceTime(std::chrono::microseconds(1234));
  clone->PostCall(otel_context, client_context,
                  {clock->Now(), Status{StatusCode::kOk, "ok"}});
  clock->AdvanceTime(std::chrono::milliseconds(100));
  clone->OnDone(otel_context, {clock->Now(), Status{StatusCode::kOk, "ok"}});
}

TEST(AttemptLatencyTest, ThreeAttempts) {
  auto mock_histogram = std::make_unique<MockHistogram<double>>();
  EXPECT_CALL(
      *mock_histogram,
      Record(A<double>(), A<opentelemetry::common::KeyValueIterable const&>(),
             A<opentelemetry::context::Context const&>()))
      .WillOnce([](double value,
                   opentelemetry::common::KeyValueIterable const& attributes,
                   opentelemetry::context::Context const&) {
        EXPECT_THAT(value, Eq(1.0));
        EXPECT_THAT(
            MakeAttributesMap(attributes),
            UnorderedElementsAre(
                Pair("project_id", "my-project-id"),
                Pair("instance", "my-instance"), Pair("cluster", "my-cluster"),
                Pair("table", "my-table"), Pair("zone", "my-zone"),
                Pair("method", "my-method"), Pair("streaming", "my-streaming"),
                Pair("status", "OK"), Pair("client_name", "my-client-name"),
                Pair("client_uid", "my-client-uid"),
                Pair("app_profile", "my-app-profile")));
      })
      .WillOnce([](double value,
                   opentelemetry::common::KeyValueIterable const& attributes,
                   opentelemetry::context::Context const&) {
        EXPECT_THAT(value, Eq(2.0));
        EXPECT_THAT(
            MakeAttributesMap(attributes),
            UnorderedElementsAre(
                Pair("project_id", "my-project-id"),
                Pair("instance", "my-instance"), Pair("cluster", "my-cluster"),
                Pair("table", "my-table"), Pair("zone", "my-zone"),
                Pair("method", "my-method"), Pair("streaming", "my-streaming"),
                Pair("status", "OK"), Pair("client_name", "my-client-name"),
                Pair("client_uid", "my-client-uid"),
                Pair("app_profile", "my-app-profile")));
      })
      .WillOnce([](double value,
                   opentelemetry::common::KeyValueIterable const& attributes,
                   opentelemetry::context::Context const&) {
        EXPECT_THAT(value, Eq(3.0));
        EXPECT_THAT(
            MakeAttributesMap(attributes),
            UnorderedElementsAre(
                Pair("project_id", "my-project-id"),
                Pair("instance", "my-instance"), Pair("cluster", "my-cluster"),
                Pair("table", "my-table"), Pair("zone", "my-zone"),
                Pair("method", "my-method"), Pair("streaming", "my-streaming"),
                Pair("status", "OK"), Pair("client_name", "my-client-name"),
                Pair("client_uid", "my-client-uid"),
                Pair("app_profile", "my-app-profile")));
      });

  opentelemetry::nostd::shared_ptr<MockMeter> mock_meter =
      std::make_shared<MockMeter>();
  EXPECT_CALL(*mock_meter, CreateDoubleHistogram)
      .WillOnce([mock = std::move(mock_histogram)](
                    opentelemetry::nostd::string_view name,
                    opentelemetry::nostd::string_view,
                    opentelemetry::nostd::string_view) mutable {
        EXPECT_THAT(name, Eq("attempt_latencies"));
        return std::move(mock);
      });

  opentelemetry::nostd::shared_ptr<MockMeterProvider> mock_provider =
      std::make_shared<MockMeterProvider>();
  EXPECT_CALL(*mock_provider, GetMeter)
#if OPENTELEMETRY_ABI_VERSION_NO >= 2
      .WillOnce([&](opentelemetry::nostd::string_view scope,
                    opentelemetry::nostd::string_view scope_version,
                    opentelemetry::nostd::string_view,
                    opentelemetry::common::KeyValueIterable const*) mutable {
#else
      .WillOnce([&](opentelemetry::nostd::string_view scope,
                    opentelemetry::nostd::string_view scope_version,
                    opentelemetry::nostd::string_view) mutable {
#endif
        EXPECT_THAT(scope, Eq("my-instrument-scope"));
        EXPECT_THAT(scope_version, Eq("v1"));
        return mock_meter;
      });

  AttemptLatency attempt_latency("my-instrument-scope", mock_provider);
  ResourceLabels resource_labels{"my-project-id", "my-instance", "my-table", "",
                                 ""};
  DataLabels data_labels{"my-method",     "my-streaming",   "my-client-name",
                         "my-client-uid", "my-app-profile", ""};
  auto clone = attempt_latency.clone(resource_labels, data_labels);

  grpc::ClientContext client_context;
  SetClusterZone(client_context, "my-cluster", "my-zone");

  auto otel_context = opentelemetry::context::RuntimeContext::GetCurrent();
  auto clock = std::make_shared<FakeSteadyClock>();

  clock->SetTime(std::chrono::steady_clock::now());
  clone->PreCall(otel_context, {clock->Now(), true});
  clock->AdvanceTime(std::chrono::milliseconds(1));
  clone->PostCall(otel_context, client_context,
                  {clock->Now(), Status{StatusCode::kOk, "ok"}});
  clock->AdvanceTime(std::chrono::milliseconds(100));
  clone->PreCall(otel_context, {clock->Now(), false});
  clock->AdvanceTime(std::chrono::milliseconds(2));
  clone->PostCall(otel_context, client_context,
                  {clock->Now(), Status{StatusCode::kOk, "ok"}});
  clone->PreCall(otel_context, {clock->Now(), false});
  clock->AdvanceTime(std::chrono::milliseconds(3));
  clone->PostCall(otel_context, client_context,
                  {clock->Now(), Status{StatusCode::kOk, "ok"}});
  clock->AdvanceTime(std::chrono::milliseconds(100));
  clone->OnDone(otel_context, {clock->Now(), Status{StatusCode::kOk, "ok"}});
}

TEST(RetryCountTest, NoRetry) {
  auto mock_counter = std::make_unique<MockCounter<std::uint64_t>>();
  EXPECT_CALL(*mock_counter,
              Add(A<std::uint64_t>(),
                  A<opentelemetry::common::KeyValueIterable const&>(),
                  A<opentelemetry::context::Context const&>()))
      .WillOnce([](std::uint64_t value,
                   opentelemetry::common::KeyValueIterable const& attributes,
                   opentelemetry::context::Context const&) {
        EXPECT_THAT(value, Eq(0));
        EXPECT_THAT(
            MakeAttributesMap(attributes),
            UnorderedElementsAre(
                Pair("project_id", "my-project-id"),
                Pair("instance", "my-instance"), Pair("cluster", "my-cluster"),
                Pair("table", "my-table"), Pair("zone", "my-zone"),
                Pair("method", "my-method"), Pair("status", "OK"),
                Pair("client_name", "my-client-name"),
                Pair("client_uid", "my-client-uid"),
                Pair("app_profile", "my-app-profile")));
      });

  opentelemetry::nostd::shared_ptr<MockMeter> mock_meter =
      std::make_shared<MockMeter>();
  EXPECT_CALL(*mock_meter, CreateUInt64Counter)
      .WillOnce([mock = std::move(mock_counter)](
                    opentelemetry::nostd::string_view name,
                    opentelemetry::nostd::string_view,
                    opentelemetry::nostd::string_view) mutable {
        EXPECT_THAT(name, Eq("retry_count"));
        return std::move(mock);
      });

  opentelemetry::nostd::shared_ptr<MockMeterProvider> mock_provider =
      std::make_shared<MockMeterProvider>();
  EXPECT_CALL(*mock_provider, GetMeter)
#if OPENTELEMETRY_ABI_VERSION_NO >= 2
      .WillOnce([&](opentelemetry::nostd::string_view scope,
                    opentelemetry::nostd::string_view scope_version,
                    opentelemetry::nostd::string_view,
                    opentelemetry::common::KeyValueIterable const*) mutable {
#else
      .WillOnce([&](opentelemetry::nostd::string_view scope,
                    opentelemetry::nostd::string_view scope_version,
                    opentelemetry::nostd::string_view) mutable {
#endif
        EXPECT_THAT(scope, Eq("my-instrument-scope"));
        EXPECT_THAT(scope_version, Eq("v1"));
        return mock_meter;
      });

  RetryCount retry_count("my-instrument-scope", mock_provider);
  ResourceLabels resource_labels{"my-project-id", "my-instance", "my-table", "",
                                 ""};
  DataLabels data_labels{"my-method",     "my-streaming",   "my-client-name",
                         "my-client-uid", "my-app-profile", ""};
  auto clone = retry_count.clone(resource_labels, data_labels);

  grpc::ClientContext client_context;
  SetClusterZone(client_context, "my-cluster", "my-zone");

  auto otel_context = opentelemetry::context::RuntimeContext::GetCurrent();
  auto clock = std::make_shared<FakeSteadyClock>();

  clock->SetTime(std::chrono::steady_clock::now());
  clone->PreCall(otel_context, {clock->Now(), true});
  clock->AdvanceTime(std::chrono::microseconds(1234));
  clone->PostCall(otel_context, client_context,
                  {clock->Now(), Status{StatusCode::kOk, "ok"}});
  clock->AdvanceTime(std::chrono::milliseconds(100));
  clone->OnDone(otel_context, {clock->Now(), Status{StatusCode::kOk, "ok"}});
}

TEST(RetryCountTest, ThreeAttempts) {
  auto mock_counter = std::make_unique<MockCounter<std::uint64_t>>();
  EXPECT_CALL(*mock_counter,
              Add(A<std::uint64_t>(),
                  A<opentelemetry::common::KeyValueIterable const&>(),
                  A<opentelemetry::context::Context const&>()))
      .WillOnce([](std::uint64_t value,
                   opentelemetry::common::KeyValueIterable const& attributes,
                   opentelemetry::context::Context const&) {
        EXPECT_THAT(value, Eq(2));
        EXPECT_THAT(
            MakeAttributesMap(attributes),
            UnorderedElementsAre(
                Pair("project_id", "my-project-id"),
                Pair("instance", "my-instance"), Pair("cluster", "my-cluster"),
                Pair("table", "my-table"), Pair("zone", "my-zone"),
                Pair("method", "my-method"), Pair("status", "OK"),
                Pair("client_name", "my-client-name"),
                Pair("client_uid", "my-client-uid"),
                Pair("app_profile", "my-app-profile")));
      });

  opentelemetry::nostd::shared_ptr<MockMeter> mock_meter =
      std::make_shared<MockMeter>();
  EXPECT_CALL(*mock_meter, CreateUInt64Counter)
      .WillOnce([mock = std::move(mock_counter)](
                    opentelemetry::nostd::string_view name,
                    opentelemetry::nostd::string_view,
                    opentelemetry::nostd::string_view) mutable {
        EXPECT_THAT(name, Eq("retry_count"));
        return std::move(mock);
      });

  opentelemetry::nostd::shared_ptr<MockMeterProvider> mock_provider =
      std::make_shared<MockMeterProvider>();
  EXPECT_CALL(*mock_provider, GetMeter)
#if OPENTELEMETRY_ABI_VERSION_NO >= 2
      .WillOnce([&](opentelemetry::nostd::string_view scope,
                    opentelemetry::nostd::string_view scope_version,
                    opentelemetry::nostd::string_view,
                    opentelemetry::common::KeyValueIterable const*) mutable {
#else
      .WillOnce([&](opentelemetry::nostd::string_view scope,
                    opentelemetry::nostd::string_view scope_version,
                    opentelemetry::nostd::string_view) mutable {
#endif
        EXPECT_THAT(scope, Eq("my-instrument-scope"));
        EXPECT_THAT(scope_version, Eq("v1"));
        return mock_meter;
      });

  RetryCount retry_count("my-instrument-scope", mock_provider);
  ResourceLabels resource_labels{"my-project-id", "my-instance", "my-table", "",
                                 ""};
  DataLabels data_labels{"my-method",     "my-streaming",   "my-client-name",
                         "my-client-uid", "my-app-profile", ""};
  auto clone = retry_count.clone(resource_labels, data_labels);

  grpc::ClientContext client_context;
  SetClusterZone(client_context, "my-cluster", "my-zone");

  auto otel_context = opentelemetry::context::RuntimeContext::GetCurrent();
  auto clock = std::make_shared<FakeSteadyClock>();

  clock->SetTime(std::chrono::steady_clock::now());
  clone->PreCall(otel_context, {clock->Now(), true});
  clock->AdvanceTime(std::chrono::microseconds(1));
  clone->PostCall(otel_context, client_context,
                  {clock->Now(), Status{StatusCode::kOk, "ok"}});
  clock->AdvanceTime(std::chrono::milliseconds(100));
  clone->PreCall(otel_context, {clock->Now(), false});
  clock->AdvanceTime(std::chrono::microseconds(2));
  clone->PostCall(otel_context, client_context,
                  {clock->Now(), Status{StatusCode::kOk, "ok"}});
  clock->AdvanceTime(std::chrono::milliseconds(100));
  clone->PreCall(otel_context, {clock->Now(), false});
  clock->AdvanceTime(std::chrono::microseconds(3));
  clone->PostCall(otel_context, client_context,
                  {clock->Now(), Status{StatusCode::kOk, "ok"}});
  clock->AdvanceTime(std::chrono::milliseconds(100));
  clone->OnDone(otel_context, {clock->Now(), Status{StatusCode::kOk, "ok"}});
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_BIGTABLE_WITH_OTEL_METRICS
