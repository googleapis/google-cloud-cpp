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
#include "google/cloud/storage/internal/grpc/default_options.h"
#include "google/cloud/common_options.h"
#include "google/cloud/internal/async_connection_ready.h"
#include "google/cloud/internal/call_context.h"
#include "google/cloud/internal/completion_queue_impl.h"
#include "google/cloud/internal/make_status.h"
#include "google/cloud/log.h"
#include "absl/strings/match.h"
#include "absl/strings/str_split.h"
#include <grpcpp/channel.h>
#include <grpcpp/grpcpp.h>
#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <utility>

namespace google {
namespace cloud {
namespace storage_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

/// The scheme used by the C2P resolver, which is a prerequisite for DirectPath.
constexpr char const* kC2pPrefix = "google-c2p:///";
constexpr char const* kC2pExperimentalPrefix = "google-c2p-experimental:///";

/// The query parameter that requests DirectPath over Cloud Interconnect.
constexpr char const* kForceXds = "force-xds";

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
  auto const params = absl::StrSplit(query, '&');
  return std::any_of(
      params.begin(), params.end(), [key](std::string_view param) {
        std::size_t const eq_pos = param.find('=');
        std::string_view const name =
            eq_pos == std::string_view::npos ? param : param.substr(0, eq_pos);
        return name == key;
      });
}

/**
 * Waits for a channel to become ready without extending its lifetime.
 *
 * `CompletionQueue::AsyncWaitConnectionReady()` holds a `std::shared_ptr` to
 * the channel until the wait completes, and the wait cannot be cancelled. A
 * client destroyed while the first channel is still connecting would then keep
 * the channel alive, and block the completion queue shutdown, until the
 * deadline expires.
 *
 * This class holds only weak references between iterations, like
 * `GrpcChannelRefresh`. Destroying the channel shuts it down, which completes
 * the pending state change notification, and the wait ends immediately.
 * Otherwise it mirrors `AsyncConnectionReadyFuture`, including restoring the
 * caller's `CallContext` in each continuation.
 */
class ChannelReadyWaiter
    : public std::enable_shared_from_this<ChannelReadyWaiter> {
 public:
  ChannelReadyWaiter(
      std::weak_ptr<google::cloud::internal::CompletionQueueImpl> cq,
      std::weak_ptr<grpc::Channel> channel,
      std::chrono::system_clock::time_point deadline)
      : cq_(std::move(cq)), channel_(std::move(channel)), deadline_(deadline) {}

  future<Status> Start() {
    RunIteration();
    return promise_.get_future();
  }

 private:
  void RunIteration() {
    std::shared_ptr<google::cloud::internal::CompletionQueueImpl> cq =
        cq_.lock();
    std::shared_ptr<grpc::Channel> channel = channel_.lock();
    if (!cq || !channel) {
      promise_.set_value(google::cloud::internal::CancelledError(
          "The channel or completion queue was released before the channel "
          "became ready.",
          GCP_ERROR_INFO()));
      return;
    }
    grpc_connectivity_state const state = channel->GetState(true);
    if (state == GRPC_CHANNEL_READY) {
      promise_.set_value(Status{});
      return;
    }
    if (state == GRPC_CHANNEL_SHUTDOWN) {
      promise_.set_value(google::cloud::internal::CancelledError(
          "The channel was shut down before it became ready.",
          GCP_ERROR_INFO()));
      return;
    }
    // `NotifyOnStateChange` uses the channel only to register the watch, so
    // the strong references are released when this function returns.
    (void)google::cloud::internal::NotifyOnStateChange::Start(
        std::move(cq), std::move(channel), deadline_, state)
        .then([self = shared_from_this(),
               c = google::cloud::internal::CallContext{}](future<bool> f) {
          google::cloud::internal::ScopedCallContext scope(std::move(c));
          self->OnStateChange(f.get());
        });
  }

  void OnStateChange(bool ok) {
    if (!ok) {
      promise_.set_value(google::cloud::internal::DeadlineExceededError(
          "The channel did not become ready before the deadline.",
          GCP_ERROR_INFO()));
      return;
    }
    RunIteration();
  }

  std::weak_ptr<google::cloud::internal::CompletionQueueImpl> const cq_;
  std::weak_ptr<grpc::Channel> const channel_;
  std::chrono::system_clock::time_point const deadline_;
  promise<Status> promise_;
};

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
  LogChannelConfiguration(options, GrpcForceXdsIsSupported());
}

void LogChannelConfiguration(Options const& options,
                             bool grpc_supports_force_xds) {
  std::string const& endpoint = options.get<EndpointOption>();
  TransportType const transport = DetectTransportType(endpoint);
  bool const requested =
      options.get<storage_experimental::DirectPathXdsOverInterconnectOption>();
  // Check gRPC version support before the endpoint-override warning: on gRPC <
  // 1.85 `DefaultOptionsGrpc()` intentionally leaves the default endpoint in
  // place, and an application-supplied `?force-xds` endpoint is also ignored by
  // the `google-c2p` resolver without a diagnostic.
  if (!grpc_supports_force_xds &&
      (requested || transport == TransportType::kDirectPathInterconnect)) {
    GCP_LOG(WARNING) << "DirectPath over Interconnect requires gRPC >= 1.85.0, "
                     << "as older versions of the `google-c2p` resolver ignore "
                     << "the `force-xds` query parameter, but this application "
                     << "uses gRPC " << grpc::Version()
                     << ". transport_type=" << ToString(transport)
                     << ", endpoint=" << endpoint;
    return;
  }
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
    std::chrono::milliseconds timeout, ChannelReadyCallback on_ready) {
  if (channels.empty()) return make_ready_future();
  // We create hundreds of channels in some VMs. Observing only the first
  // channel reports the time to the first usable connection without consuming
  // all the log output with uninteresting lines.
  std::chrono::system_clock::time_point const deadline =
      std::chrono::system_clock::now() + timeout;
  return std::make_shared<ChannelReadyWaiter>(
             google::cloud::internal::GetCompletionQueueImpl(std::move(cq)),
             channels.front(), deadline)
      ->Start()
      // The continuation captures values only. It holds no reference to the
      // stub, the channel refresh loop, or the completion queue, so it cannot
      // create an ownership cycle and needs no `std::weak_ptr`.
      .then(
          [transport, start, on_ready = std::move(on_ready)](future<Status> f) {
            std::chrono::steady_clock::duration const elapsed =
                std::chrono::steady_clock::now() - start;
            Status const status = f.get();
            LogChannelReady(transport, elapsed, status);
            // A failed or abandoned connection attempt has no meaningful
            // latency, and recording it would skew the histogram.
            if (status.ok() && on_ready) on_ready(transport, elapsed);
          });
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_internal
}  // namespace cloud
}  // namespace google
