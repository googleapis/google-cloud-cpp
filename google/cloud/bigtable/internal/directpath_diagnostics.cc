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

#include "google/cloud/bigtable/internal/directpath_diagnostics.h"
#include "google/cloud/bigtable/options.h"
#include "google/cloud/internal/detect_gcp.h"
#include "google/cloud/internal/make_status.h"
#ifdef GOOGLE_CLOUD_CPP_BIGTABLE_WITH_OTEL_METRICS
#include <opentelemetry/context/runtime_context.h>
#endif
#include <chrono>
#include <cstdint>
#include <cstring>
#include <memory>
#include <string>

#ifndef _WIN32
#include <arpa/inet.h>
#include <netinet/in.h>
#include <ifaddrs.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#endif

namespace google {
namespace cloud {
namespace bigtable_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

std::string ToString(DiagnosticFailureReason reason) {
  switch (reason) {
    case DiagnosticFailureReason::kNotInGcp:
      return "not_in_gcp";
    case DiagnosticFailureReason::kMetadataUnreachable:
      return "metadata_unreachable";
    case DiagnosticFailureReason::kNoIpAssigned:
      return "no_ip_assigned";
    case DiagnosticFailureReason::kLoopbackMisconfigured:
      return "loopback_misconfigured";
    case DiagnosticFailureReason::kLoopbackMisconfiguredIpv4:
      return "loopback_misconfigured_ipv4";
    case DiagnosticFailureReason::kLoopbackMisconfiguredIpv6:
      return "loopback_misconfigured_ipv6";
    case DiagnosticFailureReason::kMetadataMissing:
      return "metadata_missing";
    case DiagnosticFailureReason::kXdsReachabilityFailed:
      return "xds_reachability_failed";
    case DiagnosticFailureReason::kXdsEdsFailed:
      return "xds_eds_failed";
    case DiagnosticFailureReason::kXdsMalformedEndpoint:
      return "xds_malformed_endpoint";
    case DiagnosticFailureReason::kRouteUnreachable:
      return "route_unreachable";
    case DiagnosticFailureReason::kAltsHandshakeFailed:
      return "alts_handshake_failed";
    case DiagnosticFailureReason::kTimeout:
      return "timeout";
    case DiagnosticFailureReason::kUnknown:
      return "unknown";
  }
  return "unknown";
}

#ifndef _WIN32
namespace {

class DefaultDirectPathNetworkSystem : public DirectPathNetworkSystem {
 public:
  bool CanConnectTcp(std::string const& host, std::uint16_t port,
                     std::chrono::milliseconds timeout) override {
    int const sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) return false;

    std::int64_t const total_usec =
        std::chrono::duration_cast<std::chrono::microseconds>(timeout).count();
    struct timeval tv;
    tv.tv_sec = static_cast<time_t>(total_usec / 1000000);
    tv.tv_usec = static_cast<suseconds_t>(total_usec % 1000000);
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));

    struct sockaddr_in addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    if (inet_pton(AF_INET, host.c_str(), &addr.sin_addr) <= 0) {
      close(sock);
      return false;
    }

    int const res =
        connect(sock, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr));
    close(sock);
    return res == 0;
  }

  DiagnosticFailureReason CheckLoopbackConfiguration() override {
    struct ifaddrs* ifaddr = nullptr;
    if (getifaddrs(&ifaddr) == -1) return DiagnosticFailureReason::kUnknown;

    bool has_ipv4_lo = false;
    bool has_ipv6_lo = false;

    for (struct ifaddrs* ifa = ifaddr; ifa != nullptr; ifa = ifa->ifa_next) {
      if (ifa->ifa_addr == nullptr) continue;
      if (ifa->ifa_name != nullptr && std::strcmp(ifa->ifa_name, "lo") == 0) {
        if (ifa->ifa_addr->sa_family == AF_INET) {
          has_ipv4_lo = true;
        } else if (ifa->ifa_addr->sa_family == AF_INET6) {
          has_ipv6_lo = true;
        }
      }
    }
    freeifaddrs(ifaddr);

    if (!has_ipv4_lo && !has_ipv6_lo) {
      return DiagnosticFailureReason::kLoopbackMisconfigured;
    }
    if (!has_ipv4_lo) {
      return DiagnosticFailureReason::kLoopbackMisconfiguredIpv4;
    }
    if (!has_ipv6_lo) {
      return DiagnosticFailureReason::kLoopbackMisconfiguredIpv6;
    }
    return DiagnosticFailureReason::kUnknown;
  }
};

}  // namespace

std::shared_ptr<DirectPathNetworkSystem> MakeDefaultDirectPathNetworkSystem() {
  return std::make_shared<DefaultDirectPathNetworkSystem>();
}

#else

namespace {

class DefaultDirectPathNetworkSystem : public DirectPathNetworkSystem {
 public:
  bool CanConnectTcp(std::string const&, std::uint16_t,
                     std::chrono::milliseconds) override {
    return false;
  }
  DiagnosticFailureReason CheckLoopbackConfiguration() override {
    return DiagnosticFailureReason::kUnknown;
  }
};

}  // namespace

std::shared_ptr<DirectPathNetworkSystem> MakeDefaultDirectPathNetworkSystem() {
  return std::make_shared<DefaultDirectPathNetworkSystem>();
}
#endif

DiagnosticFailureReason DirectPathDiagnostics::RunDiagnostics(
    Options const& options) {
  return RunDiagnostics(options, internal::MakeGcpDetector(),
                        MakeDefaultDirectPathNetworkSystem(), "169.254.169.254",
                        80);
}

DiagnosticFailureReason DirectPathDiagnostics::RunDiagnostics(
    Options const& options,
    std::shared_ptr<internal::GcpDetector> const& detector,
    std::shared_ptr<DirectPathNetworkSystem> const& network_system,
    std::string const& metadata_host, std::uint16_t metadata_port) {
  // Step 1: Check platform (GCP VM)
  auto effective_detector = detector;
  if (effective_detector == nullptr) {
    effective_detector = internal::MakeGcpDetector();
  }
  if (!effective_detector->IsGoogleCloudBios()) {
    return DiagnosticFailureReason::kNotInGcp;
  }

  auto effective_network_system = network_system;
  if (effective_network_system == nullptr) {
    effective_network_system = MakeDefaultDirectPathNetworkSystem();
  }

  // Step 2: Metadata server reachability
  std::chrono::milliseconds timeout =
      options.get<bigtable::experimental::DirectPathDiagnosticsTimeoutOption>();
  if (timeout <= std::chrono::milliseconds::zero()) {
    timeout = std::chrono::milliseconds(500);
  }

  if (!effective_network_system->CanConnectTcp(metadata_host, metadata_port,
                                               timeout)) {
    return DiagnosticFailureReason::kMetadataUnreachable;
  }

  // Step 3 & 4: Loopback configuration check
  DiagnosticFailureReason const lo_result =
      effective_network_system->CheckLoopbackConfiguration();
  if (lo_result != DiagnosticFailureReason::kUnknown) {
    return lo_result;
  }

  // Step 5: Route resolution or fallback
  return DiagnosticFailureReason::kUnknown;
}

#ifdef GOOGLE_CLOUD_CPP_BIGTABLE_WITH_OTEL_METRICS
void DirectPathDiagnostics::RunAsync(CompletionQueue cq,
                                     Options const& options) {
  RunAsync(std::move(cq), options, nullptr);
}

void DirectPathDiagnostics::RunAsync(
    CompletionQueue cq, Options const& options,
    std::shared_ptr<DirectAccessCompatibility> direct_access_compatibility) {
  RunAsync(std::move(cq), options, std::move(direct_access_compatibility),
           internal::MakeGcpDetector(), MakeDefaultDirectPathNetworkSystem(),
           "169.254.169.254", 80);
}

void DirectPathDiagnostics::RunAsync(
    CompletionQueue cq, Options const& options,
    std::shared_ptr<DirectAccessCompatibility> direct_access_compatibility,
    std::shared_ptr<internal::GcpDetector> detector,
    std::shared_ptr<DirectPathNetworkSystem> network_system,
    std::string const& metadata_host, std::uint16_t metadata_port) {
  auto run_options = options;
  if (run_options
          .get<bigtable::experimental::DirectPathDiagnosticsTimeoutOption>() <=
      std::chrono::milliseconds::zero()) {
    run_options.set<bigtable::experimental::DirectPathDiagnosticsTimeoutOption>(
        std::chrono::milliseconds(5000));
  }

  cq.RunAsync(
      [run_options,
       direct_access_compatibility = std::move(direct_access_compatibility),
       detector = std::move(detector),
       network_system = std::move(network_system), metadata_host,
       metadata_port]() {
        DiagnosticFailureReason const reason =
            DirectPathDiagnostics::RunDiagnostics(run_options, detector,
                                                  network_system, metadata_host,
                                                  metadata_port);
        if (direct_access_compatibility != nullptr) {
          direct_access_compatibility->Record(
              opentelemetry::context::RuntimeContext::GetCurrent(), 0,
              DirectAccessCompatibilityLabels{"", ToString(reason)});
        }
      });
}
#endif

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable_internal
}  // namespace cloud
}  // namespace google
