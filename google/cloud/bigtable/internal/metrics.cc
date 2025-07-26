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

namespace google {
namespace cloud {
namespace bigtable_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {
auto constexpr kMeterInstrumentationScope = "bigtable";
auto constexpr kMeterInstrumentationScopeVersion = "v1";
}  // namespace

LabelMap IntoLabelMap(ResourceLabels const& r, DataLabels const& d) {
  return {
      {"project_id", r.project_id},
      {"instance", r.instance},
      {"table", r.table},
      {"cluster", r.cluster},
      {"zone", r.zone},
      {"method", d.method},
      {"streaming", d.streaming},
      {"client_name", d.client_name},
      {"client_uid", d.client_uid},
      {"app_profile", d.app_profile},
      {"status", d.status},
  };
}

absl::optional<google::bigtable::v2::ResponseParams>
GetResponseParamsFromTrailingMetadata(
    grpc::ClientContext const& client_context) {
  auto metadata = client_context.GetServerTrailingMetadata();
  auto iter = metadata.find("x-goog-ext-425905942-bin");
  if (iter == metadata.end()) return absl::nullopt;
  google::bigtable::v2::ResponseParams p;
  // The value for this key should always be the same in a response, so we
  // return the first value we find.
  std::string value{iter->second.data(), iter->second.size()};
  if (p.ParseFromString(value)) return p;
  return absl::nullopt;
}

Metric::~Metric() = default;

OperationLatency::OperationLatency(
    std::shared_ptr<opentelemetry::metrics::MeterProvider> const& provider)
    : operation_latencies_(provider
                               ->GetMeter(kMeterInstrumentationScope,
                                          kMeterInstrumentationScopeVersion)
                               ->CreateDoubleHistogram("operation_latencies")
                               .release()) {}

void OperationLatency::PreCall(opentelemetry::context::Context const&,
                               PreCallParams const& p) {
  if (p.first_attempt) {
    operation_start_ = p.attempt_start;
  }
}

void OperationLatency::PostCall(opentelemetry::context::Context const&,
                                grpc::ClientContext const& client_context,
                                PostCallParams const&) {
  auto response_params = GetResponseParamsFromTrailingMetadata(client_context);
  if (response_params) {
    resource_labels_.cluster = response_params->cluster_id();
    resource_labels_.zone = response_params->zone_id();
  }
}

void OperationLatency::OnDone(opentelemetry::context::Context const& context,
                              OnDoneParams const& p) {
  data_labels_.status = StatusCodeToString(p.operation_status.code());
  auto operation_elapsed = std::chrono::duration_cast<LatencyDuration>(
      p.operation_end - operation_start_);
  operation_latencies_->Record(operation_elapsed.count(),
                               IntoLabelMap(resource_labels_, data_labels_),
                               context);
}

std::unique_ptr<Metric> OperationLatency::clone(ResourceLabels resource_labels,
                                                DataLabels data_labels) const {
  auto m = std::make_unique<OperationLatency>(*this);
  m->resource_labels_ = std::move(resource_labels);
  m->data_labels_ = std::move(data_labels);
  return m;
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_BIGTABLE_WITH_OTEL_METRICS
