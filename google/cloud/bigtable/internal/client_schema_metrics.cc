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
#include "absl/strings/str_join.h"
#include "absl/strings/str_split.h"
#include <opentelemetry/metrics/meter.h>
#include <opentelemetry/semconv/incubating/cloud_attributes.h>
#include <opentelemetry/semconv/incubating/faas_attributes.h>
#include <opentelemetry/semconv/incubating/host_attributes.h>
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

LabelMap BaseLabels(ClientResourceLabels const& r) {
  return {{"project_id", r.project_id},   {"instance", r.instance},
          {"app_profile", r.app_profile}, {"client_name", r.client_name},
          {"client_uid", r.client_uid},   {"client_project", r.client_project},
          {"region", r.region},           {"cloud_platform", r.cloud_platform},
          {"host_id", r.host_id},         {"host_name", r.host_name}};
}
}  // namespace

LabelMap IntoLabelMap(ClientResourceLabels const& r,
                      ClientOutstandingRpcLabels const& d,
                      std::set<std::string> const& filtered_data_labels) {
  LabelMap labels = BaseLabels(r);

  auto emplace_if_not_filtered = [&](std::string_view key,
                                     std::string_view value) {
    if (filtered_data_labels.empty() ||
        filtered_data_labels.find(std::string(key)) ==
            filtered_data_labels.end()) {
      labels.emplace(key, value);
    }
  };

  emplace_if_not_filtered("transport_type", ToString(d.transport_type));
  emplace_if_not_filtered("channel_pool_lb_policy",
                          ToString(d.channel_pool_lb_policy));
  emplace_if_not_filtered("streaming", IsStreamingAsString(d.streaming));

  return labels;
}

LabelMap IntoLabelMap(ClientResourceLabels const& r,
                      DirectAccessCompatibilityLabels const& d,
                      std::set<std::string> const& filtered_data_labels) {
  LabelMap labels = BaseLabels(r);

  auto emplace_if_not_filtered = [&](std::string_view key,
                                     std::string_view value) {
    if (filtered_data_labels.empty() ||
        filtered_data_labels.find(std::string(key)) ==
            filtered_data_labels.end()) {
      labels.emplace(key, value);
    }
  };

  emplace_if_not_filtered("ip_preference", d.ip_preference);
  emplace_if_not_filtered("reason", d.reason);

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

  if (project_id.empty() && options.has<InstanceChannelAffinityOption>()) {
    auto const& instances = options.get<InstanceChannelAffinityOption>();
    if (!instances.empty()) {
      project_id = instances[0].project_id();
    }
  }
  if (project_id.empty()) {
    project_id = by_name(sc::cloud::kCloudAccountId);
  }

  if (instance.empty() && options.has<InstanceChannelAffinityOption>()) {
    auto const& instances = options.get<InstanceChannelAffinityOption>();
    if (!instances.empty()) {
      instance = instances[0].instance_id();
    }
  }

  if (app_profile.empty() && options.has<bigtable::AppProfileIdOption>()) {
    app_profile = options.get<bigtable::AppProfileIdOption>();
  }

  std::string client_project = by_name(sc::cloud::kCloudAccountId);
  if (client_project.empty()) {
    client_project = project_id;
  }

  std::string region = by_name(sc::cloud::kCloudRegion);
  if (region.empty()) {
    std::string zone = by_name(sc::cloud::kCloudAvailabilityZone);
    std::vector<std::string_view> parts = absl::StrSplit(zone, '-');
    if (parts.size() >= 3 && parts.back().size() == 1) {
      parts.pop_back();
      region = absl::StrJoin(parts, "-");
    } else {
      region = std::move(zone);
    }
  }
  if (region.empty()) {
    region = "global";
  }

  ClientResourceLabels labels;
  labels.project_id = std::move(project_id);
  labels.instance = std::move(instance);
  labels.app_profile = std::move(app_profile);
  labels.client_name = "cpp.Bigtable/" + bigtable::version_string();
  labels.client_uid = client_uid;
  labels.client_project = std::move(client_project);
  labels.region = std::move(region);
  labels.cloud_platform = by_name(sc::cloud::kCloudPlatform, "unknown");
  labels.host_id = by_name("faas.id");
  if (labels.host_id.empty()) {
    labels.host_id = by_name(sc::host::kHostId, "unknown");
  }
  labels.host_name = by_name(sc::host::kHostName);
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
  // `channel_pool_lb_policy` is filtered out because the Cloud Monitoring
  // metric descriptor for
  // `bigtable.googleapis.com/internal/client/connection_pool/outstanding_rpcs`
  // does not recognize this label.
  static auto const* const kFilteredDataLabels =
      new std::set<std::string>{"channel_pool_lb_policy"};
  outstanding_rpcs_->Record(
      static_cast<double>(p.outstanding_rpcs),
      IntoLabelMap(resource_labels_, data_labels, *kFilteredDataLabels),
      context);
}

std::unique_ptr<ClientSchemaMetric> OutstandingRpcs::clone(
    ClientResourceLabels const& resource_labels) const {
  auto m = std::make_unique<OutstandingRpcs>(*this);
  m->resource_labels_ = resource_labels;
  return m;
}

#if OPENTELEMETRY_ABI_VERSION_NO >= 2
DirectAccessCompatibility::DirectAccessCompatibility(
    std::string const& instrumentation_scope,
    opentelemetry::nostd::shared_ptr<
        opentelemetry::metrics::MeterProvider> const& provider,
    ClientResourceLabels resource_labels)
    : resource_labels_(std::move(resource_labels)),
      gauge_(provider
                 ->GetMeter(instrumentation_scope,
                            kMeterInstrumentationScopeVersion)
                 ->CreateInt64Gauge(
                     "direct_access/compatible",
                     "Compatibility check result for Bigtable DirectPath.",
                     "1")) {}

void DirectAccessCompatibility::Record(
    opentelemetry::context::Context const& context, std::int64_t value,
    DirectAccessCompatibilityLabels const& data_labels) {
  if (gauge_ == nullptr) return;
  LabelMap const labels = IntoLabelMap(resource_labels_, data_labels, {});
  gauge_->Record(value, labels, context);
}
#else
void DirectAccessCompatibility::ObserveCallback(
    opentelemetry::metrics::ObserverResult observer_result, void* state_ptr) {
  auto* state = static_cast<State*>(state_ptr);
  if (state == nullptr) return;
  std::lock_guard<std::mutex> lk(state->mu);
  if (!state->has_value) return;

  if (opentelemetry::nostd::holds_alternative<opentelemetry::nostd::shared_ptr<
          opentelemetry::metrics::ObserverResultT<std::int64_t> > >(
          observer_result)) {
    auto observer = opentelemetry::nostd::get<opentelemetry::nostd::shared_ptr<
        opentelemetry::metrics::ObserverResultT<std::int64_t> > >(
        observer_result);
    observer->Observe(state->value, state->labels);
  }
}

DirectAccessCompatibility::DirectAccessCompatibility(
    std::string const& instrumentation_scope,
    opentelemetry::nostd::shared_ptr<
        opentelemetry::metrics::MeterProvider> const& provider,
    ClientResourceLabels resource_labels)
    : resource_labels_(std::move(resource_labels)),
      state_(std::make_shared<State>()),
      gauge_(provider
                 ->GetMeter(instrumentation_scope,
                            kMeterInstrumentationScopeVersion)
                 ->CreateInt64ObservableGauge(
                     "direct_access/compatible",
                     "Compatibility check result for Bigtable DirectPath.",
                     "1")) {
  if (gauge_ != nullptr) {
    gauge_->AddCallback(ObserveCallback, state_.get());
  }
}

DirectAccessCompatibility::~DirectAccessCompatibility() {
  if (gauge_ != nullptr) {
    gauge_->RemoveCallback(ObserveCallback, state_.get());
  }
}

DirectAccessCompatibility::DirectAccessCompatibility(
    DirectAccessCompatibility const& other)
    : resource_labels_(other.resource_labels_),
      state_(std::make_shared<State>()),
      gauge_(other.gauge_) {
  if (gauge_ != nullptr) {
    gauge_->AddCallback(ObserveCallback, state_.get());
  }
}

void DirectAccessCompatibility::Record(
    opentelemetry::context::Context const&, std::int64_t value,
    DirectAccessCompatibilityLabels const& data_labels) {
  LabelMap labels = IntoLabelMap(resource_labels_, data_labels, {});
  std::lock_guard<std::mutex> lk(state_->mu);
  state_->value = value;
  state_->labels = std::move(labels);
  state_->has_value = true;
}
#endif

std::unique_ptr<ClientSchemaMetric> DirectAccessCompatibility::clone(
    ClientResourceLabels const& resource_labels) const {
  auto m = std::make_unique<DirectAccessCompatibility>(*this);
  m->resource_labels_ = resource_labels;
  return m;
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_BIGTABLE_WITH_OTEL_METRICS
