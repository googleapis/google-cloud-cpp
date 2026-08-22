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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INTERNAL_CLIENT_SCHEMA_METRICS_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INTERNAL_CLIENT_SCHEMA_METRICS_H

#ifdef GOOGLE_CLOUD_CPP_BIGTABLE_WITH_OTEL_METRICS

#include "google/cloud/bigtable/internal/metrics.h"
#include "google/cloud/options.h"
#include <opentelemetry/metrics/meter_provider.h>
#include <opentelemetry/metrics/sync_instruments.h>
#include <opentelemetry/sdk/resource/resource.h>
#include <memory>
#include <set>
#include <string>
#include <unordered_map>

namespace google {
namespace cloud {
namespace bigtable_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

struct ClientResourceLabels {
  std::string project_id;
  std::string instance;
  std::string app_profile;
  std::string client_name;
  std::string client_uid;
  std::string client_project;
  std::string location;
  std::string cloud_platform;
  std::string host_id;
  std::string hostname;
};

struct ClientOutstandingRpcLabels {
  TransportType transport_type;
  ChannelPoolLbPolicy channel_pool_lb_policy;
  RpcType streaming;
};

using LabelMap = std::unordered_map<std::string, std::string>;
LabelMap IntoLabelMap(ClientResourceLabels const& r,
                      ClientOutstandingRpcLabels const& d,
                      std::set<std::string> const& filtered_data_labels = {});

struct DirectAccessCompatibilityLabels {
  std::string ip_preference;
  std::string reason;
};

LabelMap IntoLabelMap(ClientResourceLabels const& r,
                      DirectAccessCompatibilityLabels const& d,
                      std::set<std::string> const& filtered_data_labels = {});

ClientResourceLabels MakeClientResourceLabels(
    std::string project_id, std::string instance, std::string app_profile,
    Options const& options, std::string const& client_uid,
    opentelemetry::sdk::resource::Resource const& detected_resource);

class ClientSchemaMetric : public Metric {
 public:
  MetricSchema schema() const final { return MetricSchema::kClient; }
  virtual std::unique_ptr<ClientSchemaMetric> clone(
      ClientResourceLabels const& resource_labels) const = 0;
};

class OutstandingRpcs : public ClientSchemaMetric {
 public:
  OutstandingRpcs(std::string const& instrumentation_scope,
                  opentelemetry::nostd::shared_ptr<
                      opentelemetry::metrics::MeterProvider> const& provider);
  void StubSelection(opentelemetry::context::Context const&,
                     StubSelectionParams const& p) override;
  std::unique_ptr<ClientSchemaMetric> clone(
      ClientResourceLabels const& resource_labels) const override;

 private:
  ClientResourceLabels resource_labels_;
  opentelemetry::nostd::shared_ptr<opentelemetry::metrics::Histogram<double>>
      outstanding_rpcs_;
};

class DirectAccessCompatibility : public ClientSchemaMetric {
 public:
  DirectAccessCompatibility(
      std::string const& instrumentation_scope,
      opentelemetry::nostd::shared_ptr<
          opentelemetry::metrics::MeterProvider> const& provider);

  void Record(opentelemetry::context::Context const& context,
              std::int64_t value,
              DirectAccessCompatibilityLabels const& data_labels);

  std::unique_ptr<ClientSchemaMetric> clone(
      ClientResourceLabels const& resource_labels) const override;

 private:
  ClientResourceLabels resource_labels_;
#if OPENTELEMETRY_ABI_VERSION_NO >= 2
  opentelemetry::nostd::shared_ptr<opentelemetry::metrics::Gauge<std::int64_t>>
      gauge_;
#else
  opentelemetry::nostd::shared_ptr<opentelemetry::metrics::Histogram<double>>
      gauge_;
#endif
};

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_BIGTABLE_WITH_OTEL_METRICS

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INTERNAL_CLIENT_SCHEMA_METRICS_H
