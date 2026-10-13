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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_GRPC_CHANNEL_METRICS_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_GRPC_CHANNEL_METRICS_H

#include "google/cloud/storage/internal/grpc/channel_telemetry.h"
#include "google/cloud/storage/version.h"
#include "google/cloud/options.h"
#ifdef GOOGLE_CLOUD_CPP_STORAGE_WITH_OTEL_METRICS
#include <opentelemetry/metrics/meter_provider.h>
#include <memory>
#endif  // GOOGLE_CLOUD_CPP_STORAGE_WITH_OTEL_METRICS

namespace google {
namespace cloud {
namespace storage_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

/**
 * Returns a callback that records the `channel_creation_latency` histogram.
 *
 * The histogram is published as
 * `storage.googleapis.com/internal/client/channel_creation_latency`, in
 * seconds, labeled by `transport_type`. It shares the meter provider that
 * `EnableGrpcMetrics()` builds for gRPC's own metrics, so it inherits the
 * customer's `EnableGrpcMetricsOption` opt-out, their export period, and their
 * monitoring project without creating a second exporter.
 *
 * `storage::internal::HedgedReadMetrics` records into the application's global
 * meter provider instead. That suits customer-facing metrics, but not this
 * one: the global provider is a no-op unless the application installs one, and
 * when it does, it exports to the application's backend rather than to the
 * Cloud Storage service namespace this metric belongs to.
 *
 * Returns an empty callback when the library is built without
 * `GOOGLE_CLOUD_CPP_STORAGE_WITH_OTEL_METRICS`, when @p options disables
 * `EnableGrpcMetricsOption`, or when no meter provider exists for the
 * authority in @p options (for example, when running outside Google Cloud).
 *
 * The option is checked here, and not only in `EnableGrpcMetrics()`, because
 * the provider registry is process-wide: an earlier client may have enabled
 * metrics for the same authority, and this client's opt-out must still win.
 */
ChannelReadyCallback MakeChannelMetricsCallback(Options const& options);

#ifdef GOOGLE_CLOUD_CPP_STORAGE_WITH_OTEL_METRICS

/**
 * Returns a callback recording into @p provider.
 *
 * Taking the provider as a parameter, as `storage::internal::HedgedReadMetrics`
 * does, keeps the instrument testable with a mock exporter.
 */
ChannelReadyCallback MakeChannelMetricsCallback(
    std::shared_ptr<opentelemetry::metrics::MeterProvider> const& provider);

#endif  // GOOGLE_CLOUD_CPP_STORAGE_WITH_OTEL_METRICS

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_GRPC_CHANNEL_METRICS_H
