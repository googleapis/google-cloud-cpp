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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_GRPC_CHANNEL_TELEMETRY_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_GRPC_CHANNEL_TELEMETRY_H

#include "google/cloud/storage/version.h"
#include "google/cloud/completion_queue.h"
#include "google/cloud/future.h"
#include "google/cloud/options.h"
#include "google/cloud/status.h"
#include <grpcpp/grpcpp.h>
#include <chrono>
#include <memory>
#include <string_view>
#include <vector>

namespace google {
namespace cloud {
namespace storage_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

/**
 * The transport used to reach the GCS service.
 */
enum class TransportType {
  /// The default path, through the Google Front End.
  kCloudPath,
  /// DirectPath, as negotiated by the `google-c2p` resolver.
  kDirectPath,
  /// DirectPath over Cloud Interconnect.
  kDirectPathInterconnect,
};

/// Returns the canonical name of @p type.
std::string_view ToString(TransportType type);

/**
 * Infers the transport from the effective @p endpoint.
 *
 * Only the target string is available to the client library. The `google-c2p`
 * resolver may still fall back to CloudPath at runtime, and that decision is
 * not observable from here. Treat the result as the *requested* transport.
 */
TransportType DetectTransportType(std::string_view endpoint);

/**
 * Logs the effective connectivity configuration.
 *
 * Emits a `WARNING` when the application asked for DirectPath over
 * Interconnect but the effective endpoint does not use it, which happens, for
 * instance, when the application also sets `EndpointOption` or
 * `UniverseDomainOption`. Otherwise emits an `INFO` line describing the
 * transport in use.
 */
void LogChannelConfiguration(Options const& options);

/// Logs the outcome of waiting for the first channel to become ready.
void LogChannelReady(TransportType transport,
                     std::chrono::steady_clock::duration elapsed,
                     Status const& status);

/**
 * How long to wait for the first channel to become ready.
 *
 * An unbounded wait would keep a pending completion queue operation (and the
 * channel it references) alive for the lifetime of the client.
 */
auto constexpr kDefaultChannelReadyTimeout = std::chrono::seconds(30);

/**
 * Asynchronously reports how long the first channel takes to become ready.
 *
 * Production callers pass `kDefaultChannelReadyTimeout` as @p timeout. Returns
 * the future satisfied once the outcome has been reported; production callers
 * discard it, tests use it to avoid racing with the continuation.
 */
future<void> StartChannelTelemetry(
    CompletionQueue cq,
    std::vector<std::shared_ptr<grpc::Channel>> const& channels,
    TransportType transport, std::chrono::steady_clock::time_point start,
    std::chrono::milliseconds timeout);

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_GRPC_CHANNEL_TELEMETRY_H
