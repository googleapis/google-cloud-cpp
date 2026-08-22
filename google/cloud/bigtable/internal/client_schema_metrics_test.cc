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
#include "google/cloud/bigtable/internal/data_connection_impl.h"
#include "google/cloud/bigtable/options.h"
#include "google/cloud/bigtable/version.h"
#include "google/cloud/testing_util/mock_opentelemetry_metrics.h"
#include "google/cloud/testing_util/opentelemetry_attributes.h"
#include <gmock/gmock.h>
#include <opentelemetry/context/runtime_context.h>

namespace google {
namespace cloud {
namespace bigtable_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::google::cloud::testing_util::MakeAttributesMap;
using ::google::cloud::testing_util::MockHistogram;
using ::google::cloud::testing_util::MockMeter;
using ::google::cloud::testing_util::MockMeterProvider;
using ::testing::A;
using ::testing::Eq;
using ::testing::IsEmpty;
using ::testing::Pair;
using ::testing::UnorderedElementsAre;

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

TEST(ClientSchemaMetricsTest, IntoLabelMapDirectAccessCompatibility) {
  ClientResourceLabels resource{"p-1",    "i-1",       "app-1", "client-1",
                                "uid-1",  "cp-1",      "loc-1", "cloud-1",
                                "host-1", "hostname-1"};
  DirectAccessCompatibilityLabels data{"ipv4", "test_reason"};
  LabelMap actual = IntoLabelMap(resource, data);
  EXPECT_THAT(
      actual,
      UnorderedElementsAre(
          Pair("project_id", "p-1"), Pair("instance", "i-1"),
          Pair("app_profile", "app-1"), Pair("client_name", "client-1"),
          Pair("client_uid", "uid-1"), Pair("client_project", "cp-1"),
          Pair("location", "loc-1"), Pair("cloud_platform", "cloud-1"),
          Pair("host_id", "host-1"), Pair("hostname", "hostname-1"),
          Pair("ip_preference", "ipv4"), Pair("reason", "test_reason")));
}

TEST(ClientSchemaMetricsTest, IntoLabelMapDirectAccessCompatibilityFiltered) {
  ClientResourceLabels resource{"p-1",    "i-1",       "app-1", "client-1",
                                "uid-1",  "cp-1",      "loc-1", "cloud-1",
                                "host-1", "hostname-1"};
  DirectAccessCompatibilityLabels data{"ipv6", ""};
  LabelMap actual = IntoLabelMap(resource, data, {"reason"});
  EXPECT_THAT(actual,
              UnorderedElementsAre(
                  Pair("project_id", "p-1"), Pair("instance", "i-1"),
                  Pair("app_profile", "app-1"), Pair("client_name", "client-1"),
                  Pair("client_uid", "uid-1"), Pair("client_project", "cp-1"),
                  Pair("location", "loc-1"), Pair("cloud_platform", "cloud-1"),
                  Pair("host_id", "host-1"), Pair("hostname", "hostname-1"),
                  Pair("ip_preference", "ipv6")));
}

TEST(ClientSchemaMetricsTest, DirectAccessCompatibilityMetric) {
#if OPENTELEMETRY_ABI_VERSION_NO >= 2
  auto mock_gauge = std::make_unique<MockGauge<std::int64_t>>();
  EXPECT_CALL(*mock_gauge,
              Record(A<std::int64_t>(),
                     A<opentelemetry::common::KeyValueIterable const&>(),
                     A<opentelemetry::context::Context const&>()))
      .WillOnce([](std::int64_t value,
                   opentelemetry::common::KeyValueIterable const& attrs,
                   opentelemetry::context::Context const&) {
        EXPECT_THAT(value, Eq(1));
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
                Pair("ip_preference", "ipv4"), Pair("reason", "")));
      });

  opentelemetry::nostd::shared_ptr<MockMeter> mock_meter =
      std::make_shared<MockMeter>();
  EXPECT_CALL(*mock_meter, CreateInt64Gauge)
      .WillOnce([mock = std::move(mock_gauge)](
                    opentelemetry::nostd::string_view name,
                    opentelemetry::nostd::string_view,
                    opentelemetry::nostd::string_view) mutable {
        EXPECT_THAT(name, Eq("direct_access/compatible"));
        return std::move(mock);
      });
#else
  auto mock_histogram = std::make_unique<MockHistogram<double>>();
  EXPECT_CALL(
      *mock_histogram,
      Record(A<double>(), A<opentelemetry::common::KeyValueIterable const&>(),
             A<opentelemetry::context::Context const&>()))
      .WillOnce([](double value,
                   opentelemetry::common::KeyValueIterable const& attrs,
                   opentelemetry::context::Context const&) {
        EXPECT_THAT(value, Eq(1.0));
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
                Pair("ip_preference", "ipv4"), Pair("reason", "")));
      });

  opentelemetry::nostd::shared_ptr<MockMeter> mock_meter =
      std::make_shared<MockMeter>();
  EXPECT_CALL(*mock_meter, CreateDoubleHistogram)
      .WillOnce([mock = std::move(mock_histogram)](
                    opentelemetry::nostd::string_view name,
                    opentelemetry::nostd::string_view,
                    opentelemetry::nostd::string_view) mutable {
        EXPECT_THAT(name, Eq("direct_access/compatible"));
        return std::move(mock);
      });
#endif

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

  DirectAccessCompatibility compatibility("my-instrument-scope", mock_provider);
  ClientResourceLabels resource_labels{
      "my-project", "my-instance",       "my-app-profile", "my-client-name",
      "my-uid",     "my-client-project", "us-east1",       "gcp",
      "my-host",    "my-hostname"};
  auto clone = compatibility.clone(resource_labels);

  auto const otel_context =
      opentelemetry::context::RuntimeContext::GetCurrent();
  static_cast<DirectAccessCompatibility*>(clone.get())
      ->Record(otel_context, 1, DirectAccessCompatibilityLabels{"ipv4", ""});
}

TEST(ClientSchemaMetricsTest, MakeClientResourceLabelsExtractsFromOptions) {
  Options options;
  options.set<bigtable_internal::InstanceChannelAffinityOption>(
      {bigtable::InstanceResource(Project("proj-1"), "inst-1")});
  options.set<bigtable::AppProfileIdOption>("profile-1");

  ClientResourceLabels const labels = MakeClientResourceLabels(
      /*project_id=*/"", /*instance=*/"", /*app_profile=*/"", options,
      "test-uid", opentelemetry::sdk::resource::Resource::GetEmpty());

  EXPECT_THAT(labels.project_id, Eq("proj-1"));
  EXPECT_THAT(labels.instance, Eq("inst-1"));
  EXPECT_THAT(labels.app_profile, Eq("profile-1"));
  EXPECT_THAT(labels.client_uid, Eq("test-uid"));
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_BIGTABLE_WITH_OTEL_METRICS
#endif  // !defined(_WIN32) && !defined(__APPLE_)
