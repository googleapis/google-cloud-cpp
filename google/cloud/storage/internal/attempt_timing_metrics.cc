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

#include "google/cloud/storage/internal/attempt_timing_metrics.h"
#include "google/cloud/storage/internal/metrics_constants.h"
#include "google/cloud/version.h"
#include <opentelemetry/common/attribute_value.h>
#include <opentelemetry/common/key_value_iterable_view.h>
#include <opentelemetry/context/context.h>
#include <opentelemetry/nostd/string_view.h>
#include <array>
#include <chrono>
#include <string>
#include <string_view>
#include <utility>

namespace google {
namespace cloud {
namespace storage_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using Attr = std::pair<opentelemetry::nostd::string_view,
                       opentelemetry::common::AttributeValue>;

opentelemetry::nostd::string_view ToNostd(std::string_view sv) {
  return opentelemetry::nostd::string_view(sv.data(), sv.size());
}

std::string SemanticVersionString() {
  return std::to_string(google::cloud::version_major()) + "." +
         std::to_string(google::cloud::version_minor()) + "." +
         std::to_string(google::cloud::version_patch());
}

double DurationToNonNegativeSeconds(
    std::chrono::steady_clock::duration elapsed) {
  return elapsed < std::chrono::steady_clock::duration::zero()
             ? 0.0
             : std::chrono::duration<double>(elapsed).count();
}

}  // namespace

AttemptTimingMetrics::AttemptTimingMetrics(
    opentelemetry::nostd::shared_ptr<
        opentelemetry::metrics::MeterProvider> const& provider,
    std::string server_address)
    : client_version_(SemanticVersionString()),
      server_address_(std::move(server_address)) {
  if (!provider) return;
  meter_ =
      provider->GetMeter(kStorageMeterName, google::cloud::version_string());
  if (!meter_) return;
  ttfb_ = meter_->CreateDoubleHistogram(
      ToNostd(kTtfbMetric), "Time to first byte of a Storage response.", "s");
  rpc_call_duration_ =
      meter_->CreateDoubleHistogram(ToNostd(kRpcCallDurationMetric),
                                    "Duration of a single gRPC attempt.", "s");
}

void AttemptTimingMetrics::RecordTtfb(
    TtfbLabels const& labels, std::chrono::steady_clock::duration elapsed,
    opentelemetry::context::Context const& ctx) const {
  if (!ttfb_) return;
  std::array<Attr, 7> const attrs{{
      {"gcp.client.version", ToNostd(client_version_)},
      {"gcp.client.artifact",
       opentelemetry::nostd::string_view("google-cloud-cpp")},
      {"gcp.client.service", opentelemetry::nostd::string_view("storage")},
      {"server.address", ToNostd(server_address_)},
      {"rpc.system.name", ToNostd(labels.rpc_system)},
      {"gcp.storage.operation", ToNostd(labels.operation)},
      {"network.path", ToNostd(labels.network_path)},
  }};
  ttfb_->Record(DurationToNonNegativeSeconds(elapsed),
                opentelemetry::common::MakeKeyValueIterableView(attrs), ctx);
}

void AttemptTimingMetrics::RecordRpcCallDuration(
    RpcCallDurationLabels const& labels,
    std::chrono::steady_clock::duration elapsed,
    opentelemetry::context::Context const& ctx) const {
  if (!rpc_call_duration_) return;
  std::array<Attr, 9> const attrs{{
      {"gcp.client.version", ToNostd(client_version_)},
      {"gcp.client.artifact",
       opentelemetry::nostd::string_view("google-cloud-cpp")},
      {"gcp.client.service", opentelemetry::nostd::string_view("storage")},
      {"server.address", ToNostd(server_address_)},
      {"rpc.system.name", opentelemetry::nostd::string_view("grpc")},
      {"rpc.method", ToNostd(labels.rpc_method)},
      {"rpc.response.status_code", ToNostd(labels.status_code)},
      {"error.type", ToNostd(labels.error_type)},
      {"network.path", ToNostd(labels.network_path)},
  }};
  rpc_call_duration_->Record(
      DurationToNonNegativeSeconds(elapsed),
      opentelemetry::common::MakeKeyValueIterableView(attrs), ctx);
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_internal
}  // namespace cloud
}  // namespace google
