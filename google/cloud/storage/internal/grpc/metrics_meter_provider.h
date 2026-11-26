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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_GRPC_METRICS_METER_PROVIDER_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_GRPC_METRICS_METER_PROVIDER_H

#ifdef GOOGLE_CLOUD_CPP_STORAGE_WITH_OTEL_METRICS

#include "google/cloud/storage/internal/metrics_constants.h"
#include "google/cloud/version.h"
#include <opentelemetry/metrics/meter_provider.h>
#include <opentelemetry/sdk/metrics/export/periodic_exporting_metric_reader.h>
#include <opentelemetry/sdk/metrics/push_metric_exporter.h>
#include <array>
#include <memory>
#include <string>
#include <string_view>

namespace google {
namespace cloud {
namespace storage_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

/**
 * The fully qualified name of the `channel_creation_latency` instrument.
 *
 * Returns `internal/client/channel_creation_latency`. The exporter's name
 * formatter turns that into
 * `storage.googleapis.com/internal/client/channel_creation_latency`.
 *
 * `MakeGrpcMeterProvider()` registers a latency view for this instrument and
 * `MakeChannelMetricsCallback()` creates it; both go through this function so
 * the two cannot drift apart.
 */
std::string ChannelCreationLatencyInstrument();

/// Storage SDK latency histogram instruments registered with explicit buckets.
inline constexpr std::array<std::string_view, 9>
    kStorageLatencyHistogramInstruments = {{
        kRequestDurationMetric,
        kTtfbMetric,
        kRpcCallDurationMetric,
        "gcp.storage.client.stall.duration",
        "gcp.storage.client.server.duration",
        "gcp.storage.client.network.tls.handshake.duration",
        "gcp.storage.client.network.dns.lookup.duration",
        "gcp.storage.client.network.tcp.connect.duration",
        "gcp.storage.client.auth.credential_refresh.duration",
    }};

/// Storage SDK payload size histogram instruments registered with explicit
/// buckets.
inline constexpr std::array<std::string_view, 2>
    kStorageSizeHistogramInstruments = {{
        "gcp.storage.client.request.body.size",
        "gcp.storage.client.response.body.size",
    }};

/**
 * Create a meter provider used for gRPC metrics.
 */
std::shared_ptr<opentelemetry::metrics::MeterProvider> MakeGrpcMeterProvider(
    std::unique_ptr<opentelemetry::sdk::metrics::PushMetricExporter> exporter,
    opentelemetry::sdk::metrics::PeriodicExportingMetricReaderOptions
        reader_options);

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_internal
}  // namespace cloud
}  // namespace google

#endif  //  GOOGLE_CLOUD_CPP_STORAGE_WITH_OTEL_METRICS

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_GRPC_METRICS_METER_PROVIDER_H
