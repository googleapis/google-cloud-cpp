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

#include "google/cloud/storage/internal/grpc/channel_metrics.h"

#ifdef GOOGLE_CLOUD_CPP_STORAGE_WITH_OTEL_METRICS

#include "google/cloud/storage/grpc_plugin.h"
#include "google/cloud/storage/internal/grpc/metrics_exporter_impl.h"
#include "google/cloud/storage/internal/grpc/metrics_meter_provider.h"
#include "google/cloud/common_options.h"
#include "google/cloud/version.h"
#include <opentelemetry/context/context.h>
#include <opentelemetry/metrics/meter.h>
#include <opentelemetry/metrics/sync_instruments.h>
#include <opentelemetry/nostd/shared_ptr.h>
#include <opentelemetry/nostd/string_view.h>
#include <chrono>
#include <memory>
#include <string_view>
#include <utility>

namespace google {
namespace cloud {
namespace storage_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

// The only label on the histogram. Deliberately low cardinality: no channel
// id and no raw endpoint, both of which would multiply the time series.
auto constexpr kTransportTypeLabel = "transport_type";

auto constexpr kChannelCreationLatencyDescription =
    "Time from client construction until the first gRPC channel became ready.";

}  // namespace

ChannelReadyCallback MakeChannelMetricsCallback(
    std::shared_ptr<opentelemetry::metrics::MeterProvider> const& provider) {
  if (!provider) return {};
  // `MeterProvider::GetMeter()` does not promise a non-null meter. Record
  // nothing in that case.
  opentelemetry::nostd::shared_ptr<opentelemetry::metrics::Meter> meter =
      provider->GetMeter(kStorageMeterName, version_string());
  if (!meter) return {};
  // Create the instrument once, when the client is created. It must never be
  // created on a per-request path.
  opentelemetry::nostd::shared_ptr<opentelemetry::metrics::Histogram<double>>
      histogram =
          meter->CreateDoubleHistogram(ChannelCreationLatencyInstrument(),
                                       kChannelCreationLatencyDescription, "s");
  if (!histogram) return {};
  return [histogram = std::move(histogram)](
             TransportType transport,
             std::chrono::steady_clock::duration elapsed) {
    // Seconds, as a double, matching gRPC's own latency instruments (for
    // example `grpc.client.attempt.duration`) exported by the same provider.
    double const seconds =
        std::chrono::duration_cast<std::chrono::duration<double>>(elapsed)
            .count();
    std::string_view const transport_name = ToString(transport);
    histogram->Record(seconds,
                      {{kTransportTypeLabel,
                        opentelemetry::nostd::string_view(
                            transport_name.data(), transport_name.size())}},
                      opentelemetry::context::Context{});
  };
}

ChannelReadyCallback MakeChannelMetricsCallback(Options const& options) {
  // The registry is process-wide and keyed by authority, so a provider may
  // exist because an earlier client enabled metrics. This client's own opt-out
  // must still win.
  if (!options.get<storage_experimental::EnableGrpcMetricsOption>()) return {};
  return MakeChannelMetricsCallback(
      FindMeterProvider(options.get<AuthorityOption>()));
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_internal
}  // namespace cloud
}  // namespace google

#else

namespace google {
namespace cloud {
namespace storage_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

ChannelReadyCallback MakeChannelMetricsCallback(Options const&) { return {}; }

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_STORAGE_WITH_OTEL_METRICS
