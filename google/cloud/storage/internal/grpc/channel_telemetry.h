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
#include <functional>
#include <memory>
#include <string_view>
#include <vector>

namespace google {
namespace cloud {
namespace storage_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

/// The schemes of the C2P resolver, which is a prerequisite for DirectPath.
constexpr std::string_view kC2pPrefix = "google-c2p:///";
constexpr std::string_view kC2pExperimentalPrefix =
    "google-c2p-experimental:///";

/// The hosts Cloud Storage accepts as the `:authority` of DirectPath over
/// Interconnect RPCs, in production and in preprod. The service routes RPCs
/// with any other authority as regular DirectPath, even on a `force-xds`
/// channel.
constexpr std::string_view kDirectPathInterconnectHost =
    "storage-direct.googleapis.com";
constexpr std::string_view kDirectPathInterconnectPreprodHost =
    "storage-preprod-direct.sandbox.googleapis.com";

/// Returns true if @p host is one of the DirectPath over Interconnect hosts.
bool IsDirectPathInterconnectHost(std::string_view host);

/**
 * Returns the host of a `google-c2p` or `google-c2p-experimental` @p endpoint.
 *
 * The host ends at the first `:`, `/`, `?` or `#`, so a port, path, query or
 * fragment is not part of it. Returns an empty string for any other scheme.
 */
std::string_view C2pTargetHost(std::string_view endpoint);

/**
 * The transport used to reach the GCS service.
 *
 * This describes the channel, not individual RPCs: with DirectPath, the
 * control plane may still route some RPCs through the Google Front End, and
 * that per-RPC decision is not observable from the client library.
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
 * Infers the *requested* transport from the effective @p endpoint.
 *
 * Only the target string is available at this point. gRPC may select a
 * different transport at runtime; see `SelectedTransportType()`.
 */
TransportType DetectTransportType(std::string_view endpoint);

/**
 * Determines the transport gRPC selected for a channel that became ready.
 *
 * @p requested_transport comes from `DetectTransportType()`,
 * @p lb_policy_name from `grpc::Channel::GetLoadBalancingPolicyName()`, and
 * @p grpc_supports_force_xds from `GrpcForceXdsIsSupported()`. Only a
 * recognized DNS policy downgrades the result to `kCloudPath`; an unrecognized
 * or empty @p lb_policy_name keeps @p requested_transport, adjusted for
 * @p grpc_supports_force_xds.
 */
TransportType SelectedTransportType(TransportType requested_transport,
                                    std::string_view lb_policy_name,
                                    bool grpc_supports_force_xds);

/**
 * Logs the requested connectivity configuration.
 *
 * Emits a `WARNING` when the application asked for DirectPath over
 * Interconnect but the effective endpoint does not use it (for instance, when
 * `EndpointOption` or `UniverseDomainOption` is also set), when `force-xds`
 * is requested on a gRPC version that ignores it, or when a `force-xds`
 * endpoint is used with an `:authority` that is not a DirectPath over
 * Interconnect host. Otherwise emits an `INFO` line describing the requested
 * transport.
 */
void LogChannelConfiguration(Options const& options);

/// Overload of `LogChannelConfiguration` that takes an explicit `force-xds`
/// support flag for testing.
void LogChannelConfiguration(Options const& options,
                             bool grpc_supports_force_xds);

/**
 * Logs the outcome of waiting for the first channel to become ready.
 *
 * @p selected_transport is only logged when @p status is OK; no transport is
 * selected otherwise.
 */
void LogChannelReady(TransportType requested_transport,
                     TransportType selected_transport,
                     std::string_view lb_policy_name,
                     std::chrono::steady_clock::duration elapsed,
                     Status const& status);

/**
 * How long to wait for the first channel to become ready.
 *
 * An unbounded wait would keep a pending completion queue operation alive for
 * the lifetime of the client. The wait holds only a `std::weak_ptr` to the
 * channel, so destroying the client ends it immediately, and this timeout
 * never delays shutdown.
 */
auto constexpr kDefaultChannelReadyTimeout = std::chrono::seconds(30);

/**
 * Invoked with the selected transport and the elapsed time once the first
 * channel becomes ready.
 *
 * The transport is the result of `SelectedTransportType()`. Only successful
 * connections are reported. The callback may be empty, in which case nothing
 * is recorded. It runs on a completion queue thread.
 *
 * This is a seam: it keeps the OpenTelemetry dependency out of this header,
 * which is included from translation units compiled without
 * `GOOGLE_CLOUD_CPP_STORAGE_WITH_OTEL_METRICS`.
 */
using ChannelReadyCallback =
    std::function<void(TransportType, std::chrono::steady_clock::duration)>;

/**
 * Asynchronously reports how long the first channel takes to become ready.
 *
 * @p requested_transport comes from `DetectTransportType()`. The ready log and
 * @p on_ready report the transport gRPC selected, which may differ.
 *
 * Production callers pass `kDefaultChannelReadyTimeout` as @p timeout. Returns
 * the future satisfied once the outcome has been reported; production callers
 * discard it, tests use it to avoid racing with the continuation.
 */
future<void> StartChannelTelemetry(
    CompletionQueue cq,
    std::vector<std::shared_ptr<grpc::Channel>> const& channels,
    TransportType requested_transport,
    std::chrono::steady_clock::time_point start,
    std::chrono::milliseconds timeout, ChannelReadyCallback on_ready);

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_GRPC_CHANNEL_TELEMETRY_H
