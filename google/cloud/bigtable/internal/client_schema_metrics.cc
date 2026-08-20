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

#ifdef GOOGLE_CLOUD_CPP_BIGTABLE_WITH_OTEL_METRICS

#include "google/cloud/bigtable/internal/client_schema_metrics.h"
#include "google/cloud/bigtable/internal/data_connection_impl.h"
#include "google/cloud/bigtable/options.h"
#include "google/cloud/bigtable/version.h"
#include <opentelemetry/metrics/meter.h>
#include <opentelemetry/semconv/incubating/cloud_attributes.h>
#include <opentelemetry/semconv/incubating/faas_attributes.h>
#include <opentelemetry/semconv/incubating/host_attributes.h>
#include <algorithm>
#include <map>
#include <set>
#include <string_view>

namespace google {
namespace cloud {
namespace bigtable_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {
auto constexpr kMeterInstrumentationScopeVersion = "v1";

std::string_view ToString(ChannelPoolLbPolicy policy) {
  switch (policy) {
    case ChannelPoolLbPolicy::kRoundRobin:
      return "ROUND_ROBIN";
    case ChannelPoolLbPolicy::kRandomTwoLeastUsed:
      return "RANDOM_TWO_LEAST_USED";
  }
  return "ROUND_ROBIN";
}

std::string_view ToString(TransportType type) {
  switch (type) {
    case TransportType::kCloudPath:
      return "CloudPath";
    case TransportType::kDirectPath:
      return "DirectPath";
  }
  return "CloudPath";
}

std::string_view IsStreamingAsString(RpcType type) {
  switch (type) {
    case RpcType::kStreaming:
      return "true";
    case RpcType::kUnary:
      return "false";
  }
  return "false";
}
}  // namespace

LabelMap IntoLabelMap(ClientResourceLabels const& r,
                      ClientOutstandingRpcLabels const& d,
                      std::set<std::string> const& filtered_data_labels) {
  LabelMap labels = {
      {"project_id", r.project_id},   {"instance", r.instance},
      {"app_profile", r.app_profile}, {"client_name", r.client_name},
      {"client_uid", r.client_uid},   {"client_project", r.client_project},
      {"location", r.location},       {"cloud_platform", r.cloud_platform},
      {"host_id", r.host_id},         {"hostname", r.hostname}};

  struct {
    std::string key;
    std::string value;
  } data[] = {
      {"transport_type", std::string(ToString(d.transport_type))},
      {"channel_pool_lb_policy",
       std::string(ToString(d.channel_pool_lb_policy))},
      {"streaming", std::string(IsStreamingAsString(d.streaming))},
  };

  for (auto& item : data) {
    if (filtered_data_labels.find(item.key) == filtered_data_labels.end()) {
      labels.emplace(std::move(item.key), std::move(item.value));
    }
  }

  return labels;
}

ClientResourceLabels MakeClientResourceLabels(
    std::string project_id, std::string instance, std::string app_profile,
    Options const& options, std::string const& client_uid,
    opentelemetry::sdk::resource::Resource const& detected_resource) {
  namespace sc = ::opentelemetry::semconv;
  auto const& detected_attributes = detected_resource.GetAttributes();
  auto by_name = [&](std::string const& name, std::string default_value = {}) {
    auto const l = detected_attributes.find(name);
    if (l == detected_attributes.end() ||
        !opentelemetry::nostd::holds_alternative<std::string>(l->second)) {
      return default_value;
    }
    return opentelemetry::nostd::get<std::string>(l->second);
  };

  if (project_id.empty() &&
      options.has<bigtable_internal::InstanceChannelAffinityOption>()) {
    auto const& instances =
        options.get<bigtable_internal::InstanceChannelAffinityOption>();
    if (!instances.empty()) {
      project_id = instances[0].project_id();
    }
  }
  if (project_id.empty()) {
    project_id = by_name(sc::cloud::kCloudAccountId);
  }

  std::string client_project = by_name(sc::cloud::kCloudAccountId);
  if (client_project.empty()) {
    client_project = project_id;
  }

  ClientResourceLabels labels;
  labels.project_id = std::move(project_id);
  labels.instance = std::move(instance);
  labels.app_profile = std::move(app_profile);
  labels.client_name = "cpp.Bigtable/" + bigtable::version_string();
  labels.client_uid = client_uid;
  labels.client_project = std::move(client_project);
  labels.location = by_name(sc::cloud::kCloudAvailabilityZone);
  if (labels.location.empty()) {
    labels.location = by_name(sc::cloud::kCloudRegion, "global");
  }
  labels.cloud_platform = by_name(sc::cloud::kCloudPlatform, "unknown");
  labels.host_id = by_name("faas.id");
  if (labels.host_id.empty()) {
    labels.host_id = by_name(sc::host::kHostId, "unknown");
  }
  labels.hostname = by_name(sc::host::kHostName);
  return labels;
}

OutstandingRpcs::OutstandingRpcs(
    std::string const& instrumentation_scope,
    opentelemetry::nostd::shared_ptr<
        opentelemetry::metrics::MeterProvider> const& provider)
    : outstanding_rpcs_(
          provider
              ->GetMeter(instrumentation_scope,
                         kMeterInstrumentationScopeVersion)
              ->CreateDoubleHistogram(
                  "connection_pool/outstanding_rpcs",
                  "Instantaneous count of outstanding RPCs on the selected "
                  "channel.",
                  "1")) {}

void OutstandingRpcs::StubSelection(
    opentelemetry::context::Context const& context,
    StubSelectionParams const& p) {
  ClientOutstandingRpcLabels data_labels{p.transport_type,
                                         p.channel_pool_lb_policy, p.streaming};
  outstanding_rpcs_->Record(static_cast<double>(p.outstanding_rpcs),
                            IntoLabelMap(resource_labels_, data_labels),
                            context);
}

std::unique_ptr<ClientSchemaMetric> OutstandingRpcs::clone(
    ClientResourceLabels const& resource_labels) const {
  auto m = std::make_unique<OutstandingRpcs>(*this);
  m->resource_labels_ = resource_labels;
  return m;
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_BIGTABLE_WITH_OTEL_METRICS
