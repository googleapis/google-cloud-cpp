// Copyright 2026 Google LLC
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

// Getting vcpkg, cmake, and opentelemetry to actually build with the STL
// enabled is more trouble than it's worth for these unit tests.
#if !defined(_WIN32) && !defined(__MACH__)
#ifdef GOOGLE_CLOUD_CPP_BIGTABLE_WITH_OTEL_METRICS

#include "google/cloud/bigtable/internal/client_schema_metrics.h"
#include "google/cloud/bigtable/options.h"
#include "google/cloud/bigtable/version.h"
#include <gmock/gmock.h>
#include <opentelemetry/context/runtime_context.h>
#include <opentelemetry/metrics/async_instruments.h>
#include <opentelemetry/metrics/meter.h>

namespace google {
namespace cloud {
namespace bigtable_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::testing::A;
using ::testing::Eq;
using ::testing::IsEmpty;
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

  MOCK_METHOD(uintptr_t,
              RegisterCallback,  // NOLINT(bugprone-exception-escape)
              (opentelemetry::metrics::MultiObservableCallbackPtr, void*,
               opentelemetry::nostd::span<
                   opentelemetry::metrics::ObservableInstrument*>),
              (noexcept, override));

  MOCK_METHOD(void,
              DeregisterCallback,  // NOLINT(bugprone-exception-escape)
              (uintptr_t), (noexcept, override));
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

TEST(ClientSchemaMetricsTest, IntoLabelMapClient) {
  ClientResourceLabels resource{"p-1",    "i-1",       "app-1", "client-1",
                                "uid-1",  "cp-1",      "loc-1", "cloud-1",
                                "host-1", "hostname-1"};
  ClientOutstandingRpcLabels data{TransportType::kDirectPath,
                                  ChannelPoolLbPolicy::kRandomTwoLeastUsed,
                                  RpcType::kStreaming};
  LabelMap actual = IntoLabelMap(resource, data);
  EXPECT_THAT(actual,
              UnorderedElementsAre(
                  Pair("project_id", "p-1"), Pair("instance", "i-1"),
                  Pair("app_profile", "app-1"), Pair("client_name", "client-1"),
                  Pair("client_uid", "uid-1"), Pair("client_project", "cp-1"),
                  Pair("location", "loc-1"), Pair("cloud_platform", "cloud-1"),
                  Pair("host_id", "host-1"), Pair("hostname", "hostname-1"),
                  Pair("transport_type", "DirectPath"),
                  Pair("channel_pool_lb_policy", "RANDOM_TWO_LEAST_USED"),
                  Pair("streaming", "true")));
}

TEST(ClientSchemaMetricsTest, IntoLabelMapClientRoundRobinCloudPathUnary) {
  ClientResourceLabels resource{"p-1",    "i-1",       "app-1", "client-1",
                                "uid-1",  "cp-1",      "loc-1", "cloud-1",
                                "host-1", "hostname-1"};
  ClientOutstandingRpcLabels data{TransportType::kCloudPath,
                                  ChannelPoolLbPolicy::kRoundRobin,
                                  RpcType::kUnary};
  LabelMap actual = IntoLabelMap(resource, data);
  EXPECT_THAT(actual,
              UnorderedElementsAre(
                  Pair("project_id", "p-1"), Pair("instance", "i-1"),
                  Pair("app_profile", "app-1"), Pair("client_name", "client-1"),
                  Pair("client_uid", "uid-1"), Pair("client_project", "cp-1"),
                  Pair("location", "loc-1"), Pair("cloud_platform", "cloud-1"),
                  Pair("host_id", "host-1"), Pair("hostname", "hostname-1"),
                  Pair("transport_type", "CloudPath"),
                  Pair("channel_pool_lb_policy", "ROUND_ROBIN"),
                  Pair("streaming", "false")));
}

TEST(ClientSchemaMetricsTest, IntoLabelMapFilteredDataLabels) {
  ClientResourceLabels resource{"p-1",    "i-1",       "app-1", "client-1",
                                "uid-1",  "cp-1",      "loc-1", "cloud-1",
                                "host-1", "hostname-1"};
  ClientOutstandingRpcLabels data{TransportType::kDirectPath,
                                  ChannelPoolLbPolicy::kRandomTwoLeastUsed,
                                  RpcType::kStreaming};
  LabelMap actual = IntoLabelMap(resource, data, {"streaming"});
  EXPECT_THAT(actual,
              UnorderedElementsAre(
                  Pair("project_id", "p-1"), Pair("instance", "i-1"),
                  Pair("app_profile", "app-1"), Pair("client_name", "client-1"),
                  Pair("client_uid", "uid-1"), Pair("client_project", "cp-1"),
                  Pair("location", "loc-1"), Pair("cloud_platform", "cloud-1"),
                  Pair("host_id", "host-1"), Pair("hostname", "hostname-1"),
                  Pair("transport_type", "DirectPath"),
                  Pair("channel_pool_lb_policy", "RANDOM_TWO_LEAST_USED")));
}

TEST(ClientSchemaMetricsTest, OutstandingRpcsMetric) {
  auto mock_histogram = std::make_unique<MockHistogram<double>>();
  EXPECT_CALL(
      *mock_histogram,
      Record(A<double>(), A<opentelemetry::common::KeyValueIterable const&>(),
             A<opentelemetry::context::Context const&>()))
      .WillOnce([](double value,
                   opentelemetry::common::KeyValueIterable const& attrs,
                   opentelemetry::context::Context const&) {
        EXPECT_THAT(value, Eq(42.0));
        EXPECT_THAT(
            MakeAttributesMap(attrs),
            UnorderedElementsAre(
                Pair("project_id", "my-project"),
                Pair("instance", "my-instance"),
                Pair("app_profile", "my-app-profile"),
                Pair("client_name", "my-client-name"),
                Pair("client_uid", "my-uid"),
                Pair("client_project", "my-client-project"),
                Pair("location", "us-east1"), Pair("cloud_platform", "gcp"),
                Pair("host_id", "my-host"), Pair("hostname", "my-hostname"),
                Pair("transport_type", "DirectPath"),
                Pair("channel_pool_lb_policy", "RANDOM_TWO_LEAST_USED"),
                Pair("streaming", "false")));
      });

  opentelemetry::nostd::shared_ptr<MockMeter> mock_meter =
      std::make_shared<MockMeter>();
  EXPECT_CALL(*mock_meter, CreateDoubleHistogram)
      .WillOnce([mock = std::move(mock_histogram)](
                    opentelemetry::nostd::string_view name,
                    opentelemetry::nostd::string_view,
                    opentelemetry::nostd::string_view) mutable {
        EXPECT_THAT(name, Eq("connection_pool/outstanding_rpcs"));
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

  OutstandingRpcs outstanding_rpcs("my-instrument-scope", mock_provider);
  ClientResourceLabels resource_labels{
      "my-project", "my-instance",       "my-app-profile", "my-client-name",
      "my-uid",     "my-client-project", "us-east1",       "gcp",
      "my-host",    "my-hostname"};
  auto clone = outstanding_rpcs.clone(resource_labels);

  auto otel_context = opentelemetry::context::RuntimeContext::GetCurrent();
  clone->StubSelection(
      otel_context,
      StubSelectionParams{42, ChannelPoolLbPolicy::kRandomTwoLeastUsed,
                          TransportType::kDirectPath, RpcType::kUnary});
}

TEST(ClientSchemaMetricsTest, MakeClientResourceLabels) {
  Options options;
  options.set<bigtable::AppProfileIdOption>("test-app-profile");
  std::string const client_uid = "test-client-uid";

  ClientResourceLabels labels = MakeClientResourceLabels(
      "test-project", "test-instance", "test-app-profile", options, client_uid,
      opentelemetry::sdk::resource::Resource::Create({}));

  EXPECT_THAT(labels.project_id, Eq("test-project"));
  EXPECT_THAT(labels.instance, Eq("test-instance"));
  EXPECT_THAT(labels.app_profile, Eq("test-app-profile"));
  EXPECT_THAT(labels.client_name,
              Eq("cpp.Bigtable/" + bigtable::version_string()));
  EXPECT_THAT(labels.client_uid, Eq("test-client-uid"));
  EXPECT_THAT(labels.client_project, Eq("test-project"));
  EXPECT_THAT(labels.location, Eq("global"));
  EXPECT_THAT(labels.cloud_platform, Eq("unknown"));
  EXPECT_THAT(labels.host_id, Eq("unknown"));
  EXPECT_THAT(labels.hostname, IsEmpty());
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_BIGTABLE_WITH_OTEL_METRICS
#endif  // !defined(_WIN32) && !defined(__APPLE_)
