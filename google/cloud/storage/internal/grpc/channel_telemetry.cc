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

#include "google/cloud/storage/internal/grpc/channel_telemetry.h"
#include "google/cloud/storage/grpc_plugin.h"
#include "google/cloud/common_options.h"
#include "google/cloud/log.h"
#include "absl/strings/match.h"
#include "absl/strings/str_split.h"
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>

namespace google {
namespace cloud {
namespace storage_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

/// The scheme used by the C2P resolver, which is a prerequisite for DirectPath.
auto constexpr kC2pPrefix = "google-c2p:///";
auto constexpr kC2pExperimentalPrefix = "google-c2p-experimental:///";

/// The query parameter that requests DirectPath over Cloud Interconnect.
auto constexpr kForceXds = "force-xds";

bool HasQueryParameter(std::string_view uri, std::string_view key) {
  // Per RFC 3986 the fragment follows the query, so anything after `#` cannot
  // contain query parameters. Strip it before looking for the `?`, otherwise
  // `scheme://host#frag?key=val` would appear to have a query.
  std::size_t const fragment_pos = uri.find('#');
  std::string_view const addressable = fragment_pos == std::string_view::npos
                                           ? uri
                                           : uri.substr(0, fragment_pos);
  std::size_t const query_pos = addressable.find('?');
  if (query_pos == std::string_view::npos) return false;
  std::string_view const query = addressable.substr(query_pos + 1);
  for (std::string_view const param : absl::StrSplit(query, '&')) {
    std::size_t const eq_pos = param.find('=');
    std::string_view const name =
        eq_pos == std::string_view::npos ? param : param.substr(0, eq_pos);
    if (name == key) return true;
  }
  return false;
}

}  // namespace

std::string_view ToString(TransportType type) {
  switch (type) {
    case TransportType::kCloudPath:
      return "CloudPath";
    case TransportType::kDirectPath:
      return "DirectPath";
    case TransportType::kDirectPathInterconnect:
      return "DirectPathInterconnect";
  }
  return "CloudPath";
}

TransportType DetectTransportType(std::string_view endpoint) {
  if (!absl::StartsWith(endpoint, kC2pPrefix) &&
      !absl::StartsWith(endpoint, kC2pExperimentalPrefix)) {
    return TransportType::kCloudPath;
  }
  if (HasQueryParameter(endpoint, kForceXds)) {
    return TransportType::kDirectPathInterconnect;
  }
  return TransportType::kDirectPath;
}

void LogChannelConfiguration(Options const& options) {
  std::string const& endpoint = options.get<EndpointOption>();
  TransportType const transport = DetectTransportType(endpoint);
  bool const requested =
      options.get<storage_experimental::DirectPathXdsOverInterconnectOption>();
  if (requested && transport != TransportType::kDirectPathInterconnect) {
    GCP_LOG(WARNING) << "DirectPath over Interconnect is enabled, but the "
                     << "effective endpoint does not request it. Setting "
                     << "`EndpointOption` or `UniverseDomainOption` disables "
                     << "the feature. transport_type=" << ToString(transport)
                     << ", endpoint=" << endpoint;
    return;
  }
  std::string_view const authority =
      options.has<AuthorityOption>()
          ? std::string_view{options.get<AuthorityOption>()}
          : std::string_view{"(default)"};
  GCP_LOG(INFO) << "Configured gRPC transport. transport_type="
                << ToString(transport) << ", endpoint=" << endpoint
                << ", authority=" << authority;
}

void LogChannelReady(TransportType transport,
                     std::chrono::steady_clock::duration elapsed,
                     Status const& status) {
  std::int64_t const elapsed_ms =
      std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();
  if (!status.ok()) {
    // This is not necessarily an error. A completion queue that shuts down
    // before the channel connects also reports `kDeadlineExceeded`, and that
    // is indistinguishable from a real timeout, so report at INFO and include
    // the status.
    GCP_LOG(INFO) << "gRPC channel [0] did not become ready. "
                  << "transport_type=" << ToString(transport)
                  << ", elapsed_ms=" << elapsed_ms << ", status=" << status;
    return;
  }
  GCP_LOG(INFO) << "gRPC channel [0] is ready. transport_type="
                << ToString(transport) << ", elapsed_ms=" << elapsed_ms;
}

future<void> StartChannelTelemetry(
    CompletionQueue cq,
    std::vector<std::shared_ptr<grpc::Channel>> const& channels,
    TransportType transport, std::chrono::steady_clock::time_point start,
    std::chrono::milliseconds timeout) {
  if (channels.empty()) return make_ready_future();
  // We create hundreds of channels in some VMs. Observing only the first
  // channel reports the time to the first usable connection without consuming
  // all the log output with uninteresting lines.
  std::chrono::system_clock::time_point const deadline =
      std::chrono::system_clock::now() + timeout;
  return cq
      .AsyncWaitConnectionReady(channels.front(), deadline)
      // The continuation captures values only. It holds no reference to the
      // stub, the channel refresh loop, or the completion queue, so it cannot
      // create an ownership cycle and needs no `std::weak_ptr`.
      .then([transport, start](future<Status> f) {
        LogChannelReady(transport, std::chrono::steady_clock::now() - start,
                        f.get());
      });
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_internal
}  // namespace cloud
}  // namespace google
