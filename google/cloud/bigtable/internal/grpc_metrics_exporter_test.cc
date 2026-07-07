// Copyright 2026 Google LLC
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

#ifdef GOOGLE_CLOUD_CPP_BIGTABLE_WITH_OTEL_METRICS

#include "google/cloud/bigtable/internal/grpc_metrics_exporter.h"
#include "google/cloud/bigtable/options.h"
#include "google/cloud/bigtable/version.h"
#include "google/cloud/grpc_options.h"
#include "google/cloud/options.h"
#include <gmock/gmock.h>
#include <opentelemetry/sdk/metrics/data/metric_data.h>
#include <opentelemetry/sdk/metrics/data/point_data.h>
#include <string>

namespace google {
namespace cloud {
namespace bigtable_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::testing::Eq;

TEST(GrpcMetricsExporterRegistryTest, SingletonAndClear) {
  auto& registry = GrpcMetricsExporterRegistry::Singleton();
  registry.Clear();

  EXPECT_TRUE(registry.Register("test-authority-1"));
  EXPECT_FALSE(registry.Register("test-authority-1"));
  EXPECT_TRUE(registry.Register("test-authority-2"));

  registry.Clear();
  EXPECT_TRUE(registry.Register("test-authority-1"));
}

TEST(GrpcMetricsExporterTest, MakeMonitoredResource) {
  Options options;
  options.set<bigtable::AppProfileIdOption>("test-app-profile");
  std::string const client_uid = "test-client-uid";

  opentelemetry::sdk::metrics::PointDataAttributes pda;
  pda.attributes = opentelemetry::sdk::metrics::PointAttributes({
      {"project_id", "test-project"},
      {"instance", "test-instance"},
  });

  auto result = MakeMonitoredResource(pda, options, client_uid);
  EXPECT_THAT(result.project_id, Eq("test-project"));

  auto const& resource = result.resource;
  EXPECT_THAT(resource.type(), Eq("bigtable.googleapis.com/Client"));

  auto const& labels = resource.labels();
  EXPECT_THAT(labels.at("project_id"), Eq("test-project"));
  EXPECT_THAT(labels.at("instance"), Eq("test-instance"));
  EXPECT_THAT(labels.at("app_profile"), Eq("test-app-profile"));
  EXPECT_THAT(labels.at("client_name"),
              Eq("cpp.Bigtable/" + bigtable::version_string()));
  EXPECT_THAT(labels.at("uuid"), Eq("test-client-uid"));
}

TEST(GrpcMetricsExporterTest, MakeMonitoredResourceMissingAttributes) {
  Options options;
  std::string const client_uid = "test-client-uid";

  opentelemetry::sdk::metrics::PointDataAttributes pda;
  pda.attributes = opentelemetry::sdk::metrics::PointAttributes({});

  auto result = MakeMonitoredResource(pda, options, client_uid);
  EXPECT_THAT(result.project_id, Eq(""));

  auto const& resource = result.resource;
  EXPECT_THAT(resource.type(), Eq("bigtable.googleapis.com/Client"));

  auto const& labels = resource.labels();
  EXPECT_THAT(labels.at("project_id"), Eq(""));
  EXPECT_THAT(labels.at("instance"), Eq(""));
  EXPECT_THAT(labels.at("app_profile"), Eq(""));
  EXPECT_THAT(labels.at("client_name"),
              Eq("cpp.Bigtable/" + bigtable::version_string()));
  EXPECT_THAT(labels.at("uuid"), Eq("test-client-uid"));
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_BIGTABLE_WITH_OTEL_METRICS
