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
#include "google/cloud/common_options.h"
#include "google/cloud/internal/invocation_id_generator.h"
#include "google/cloud/universe_domain_options.h"
#include "absl/types/variant.h"
#include <google/api/monitored_resource.pb.h>
#include <opentelemetry/sdk/resource/semantic_conventions.h>
#include <algorithm>
#include <type_traits>

namespace google {
namespace cloud {
namespace storage_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

auto ByName(opentelemetry::sdk::resource::ResourceAttributes const& attributes,
            std::string const& name, std::string default_value) {
  auto const l = attributes.find(name);
  if (l == attributes.end()) return default_value;
  return absl::get<std::string>(l->second);
}

}  // namespace

Options MetricsExporterOptions(
    Project const& project,
    opentelemetry::sdk::resource::Resource const& resource) {
  namespace sc = ::opentelemetry::sdk::resource::SemanticConventions;

  auto const& attributes = resource.GetAttributes();
  auto monitored_resource = google::api::MonitoredResource{};
  monitored_resource.set_type("storage.googleapis.com/Client");
  auto& labels = *monitored_resource.mutable_labels();
  labels["project_id"] = project.project_id();
  labels["location"] = ByName(attributes, sc::kCloudAvailabilityZone,
                              ByName(attributes, sc::kCloudRegion, "global"));
  labels["cloud_platform"] = ByName(attributes, sc::kCloudPlatform, "unknown");
  labels["host_id"] =
      ByName(attributes, "faas.id", ByName(attributes, sc::kHostId, "unknown"));

  // We need a UUID because there may be multiple monitored resources within the
  // same process, and we need these monitored resources to be globally unique
  // or GCM may complain about the publication rate. There is no information
  // available that can make this unique enough, all the clients in a service
  // may be using the same project and bucket (not that buckets are available).
  //
  // This is fairly expensive, it requires initializing a new PRNG, and fetching
  // entropy from the OS. Outside tests, this function will be called a handful
  // of times, so the performance is not that important.
  labels["instance_id"] =
      google::cloud::internal::InvocationIdGenerator().MakeInvocationId();
  labels["api"] = "GRPC";

  return Options{}
      .set<ServiceTimeSeriesOption>(true)
      .set<MetricNameFormatterOption>([](std::string s) {
        std::replace(s.begin(), s.end(), '.', '/');
        return "storage.googleapis.com/client/" + std::move(s);
      })
      .set<MonitoredResourceOption>(std::move(monitored_resource));
}

Options MetricsExporterConnectionOptions(Options const& options) {
  auto result = Options{};
  auto ep_canonical = std::string{"storage.googleapis.com"};
  auto ep_private = std::string{"private.googleapis.com"};
  auto ep_restricted = std::string{"restricted.googleapis.com"};
  if (options.has<internal::UniverseDomainOption>()) {
    auto const& ud = options.get<internal::UniverseDomainOption>();
    result.set<internal::UniverseDomainOption>(ud);
    ep_canonical = std::string{"storage."} + ud;
    ep_private = std::string{"private."} + ud;
    ep_restricted = std::string{"restricted."} + ud;
  }
  if (!options.has<EndpointOption>()) return result;
  auto const& ep = options.get<EndpointOption>();
  auto matches = [&ep](auto const& candidate) {
    return ep == candidate || ep == (std::string{"google-c2p:///"} + candidate);
  };
  if (matches(ep_canonical)) return result;
  if (matches(ep_private)) result.set<EndpointOption>(ep_private);
  if (matches(ep_restricted)) result.set<EndpointOption>(ep_restricted);
  return result;
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_STORAGE_WITH_OTEL_METRICS
