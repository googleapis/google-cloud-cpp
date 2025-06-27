// Copyright 2024 Google LLC
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

#ifdef GOOGLE_CLOUD_CPP_STORAGE_WITH_OTEL_METRICS

#include "google/cloud/storage/internal/grpc/metrics_exporter_options.h"
#include "google/cloud/opentelemetry/monitoring_exporter.h"
#include "google/cloud/storage/internal/grpc/default_options.h"
#include "google/cloud/storage/options.h"
#include "google/cloud/options.h"
#include "google/cloud/project.h"
#include "google/cloud/testing_util/is_proto_equal.h"
#include "google/cloud/testing_util/scoped_environment.h"
#include "google/cloud/universe_domain_options.h"
#include "google/api/monitored_resource.pb.h"
#include <google/protobuf/text_format.h>
#include <gmock/gmock.h>
#include <regex>
#include <utility>

namespace google {
namespace cloud {
namespace storage_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::google::cloud::testing_util::IsProtoEqual;
using ::google::protobuf::TextFormat;
using ::testing::Contains;
using ::testing::Pair;

TEST(MetricsExporterConnectionOptions, DefaultEndpoint) {
  // In all these cases the default monitoring endpoint is either the best
  // choice, or the least bad choice.
  struct TestCase {
    std::string storage_endpoint;
    std::string universe_domain;
  } const cases[] = {
      {"", ""},
      {"storage.googleapis.com", ""},
      {"storage.ud.net", ""},
      {"storage-foo.p.googleapis.com", ""},
      {"", "ud.net"},
      {"storage.ud.net", "ud.net"},
      {"storage.googleapis.com", "ud.net"},
      {"private.googleapis.com", "ud.net"},
      {"restricted.googleapis.com", "ud.net"},
      {"storage-foo.p.googleapis.com", "ud.net"},

      {"google-c2p:///storage.googleapis.com", ""},
      {"google-c2p:///storage.ud.net", ""},
      {"google-c2p:///storage.ud.net", "ud.net"},
      {"google-c2p:///storage.googleapis.com", "ud.net"},
      {"google-c2p:///private.googleapis.com", "ud.net"},
      {"google-c2p:///restricted.googleapis.com", "ud.net"},
  };

  for (auto const& t : cases) {
    SCOPED_TRACE("Testing with " + t.storage_endpoint + " and " +
                 t.universe_domain);
    auto options = Options{};
    if (!t.storage_endpoint.empty()) {
      options.set<EndpointOption>(t.storage_endpoint);
    }
    auto expected_ud = std::string{};
    if (!t.universe_domain.empty()) {
      options.set<internal::UniverseDomainOption>(t.universe_domain);
      expected_ud = t.universe_domain;
    }
    options = DefaultOptionsGrpc(std::move(options));
    auto const actual = MetricsExporterConnectionOptions(options);
    EXPECT_FALSE(actual.has<EndpointOption>());
    EXPECT_EQ(actual.get<internal::UniverseDomainOption>(), expected_ud);
  }
}

TEST(MetricsExporterConnectionOptions, PrivateDefaultUD) {
  for (std::string prefix : {"", "google-c2p:///"}) {
    SCOPED_TRACE("Testing with prefix = " + prefix);
    auto actual = MetricsExporterConnectionOptions(
        Options{}.set<EndpointOption>(prefix + "private.googleapis.com"));
    EXPECT_THAT(actual.get<EndpointOption>(), "private.googleapis.com");
  }
}

TEST(MetricsExporterConnectionOptions, PrivateUD) {
  for (std::string prefix : {"", "google-c2p:///"}) {
    SCOPED_TRACE("Testing with prefix = " + prefix);
    auto actual = MetricsExporterConnectionOptions(
        Options{}
            .set<EndpointOption>(prefix + "private.ud.net")
            .set<internal::UniverseDomainOption>("ud.net"));
    EXPECT_THAT(actual.get<EndpointOption>(), "private.ud.net");
  }
}

TEST(MetricsExporterConnectionOptions, RestrictedDefaultUD) {
  for (std::string prefix : {"", "google-c2p:///"}) {
    SCOPED_TRACE("Testing with prefix = " + prefix);
    auto actual = MetricsExporterConnectionOptions(
        Options{}.set<EndpointOption>(prefix + "restricted.googleapis.com"));
    EXPECT_THAT(actual.get<EndpointOption>(), "restricted.googleapis.com");
  }
}

TEST(MetricsExporterConnectionOptions, RestrictedUD) {
  for (std::string prefix : {"", "google-c2p:///"}) {
    SCOPED_TRACE("Testing with prefix = " + prefix);
    auto actual = MetricsExporterConnectionOptions(
        Options{}
            .set<EndpointOption>(prefix + "restricted.ud.net")
            .set<internal::UniverseDomainOption>("ud.net"));
    EXPECT_THAT(actual.get<EndpointOption>(), "restricted.ud.net");
  }
}

MATCHER(MatchesInstanceId, "looks like an instance id") {
  std::regex re("[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}");
  return std::regex_match(arg, re);
}

TEST(MetricsExporterOptions, MonitoredResource) {
  auto actual = MetricsExporterOptions(
      Project("test-project"), opentelemetry::sdk::resource::Resource::Create({
                                   {"cloud.availability_zone", "us-central1-c"},
                                   {"cloud.region", "us-central1"},
                                   {"cloud.platform", "gcp"},
                                   {"host.id", "test-host-id"},
                               }));

  EXPECT_TRUE(actual.has<otel::MonitoredResourceOption>());
  auto mr = actual.get<otel::MonitoredResourceOption>();
  auto labels = mr.labels();
  // The `instance_id` label has unpredictable values,
  EXPECT_THAT(labels, Contains(Pair("instance_id", MatchesInstanceId())));
  mr.mutable_labels()->erase("instance_id");

  auto constexpr kExpected = R"pb(
    type: "storage.googleapis.com/Client"
    labels { key: "project_id" value: "test-project" }
    labels { key: "location" value: "us-central1-c" }
    labels { key: "cloud_platform" value: "gcp" }
    labels { key: "host_id" value: "test-host-id" }
    labels { key: "api" value: "GRPC" }
  )pb";
  auto expected = google::api::MonitoredResource{};
  ASSERT_TRUE(TextFormat::ParseFromString(kExpected, &expected));
  EXPECT_THAT(mr, IsProtoEqual(expected));
}

TEST(MetricsExporterOptions, DefaultMonitoredResource) {
  auto actual = MetricsExporterOptions(
      Project("test-project"),
      opentelemetry::sdk::resource::Resource::Create({}));

  EXPECT_TRUE(actual.has<otel::MonitoredResourceOption>());
  auto mr = actual.get<otel::MonitoredResourceOption>();
  auto labels = mr.labels();
  // The `instance_id` label has unpredictable values,
  EXPECT_THAT(labels, Contains(Pair("instance_id", MatchesInstanceId())));
  mr.mutable_labels()->erase("instance_id");

  auto constexpr kExpected = R"pb(
    type: "storage.googleapis.com/Client"
    labels { key: "project_id" value: "test-project" }
    labels { key: "location" value: "global" }
    labels { key: "cloud_platform" value: "unknown" }
    labels { key: "host_id" value: "unknown" }
    labels { key: "api" value: "GRPC" }
  )pb";
  auto expected = google::api::MonitoredResource{};
  ASSERT_TRUE(TextFormat::ParseFromString(kExpected, &expected));
  EXPECT_THAT(mr, IsProtoEqual(expected));
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_STORAGE_WITH_OTEL_METRICS
