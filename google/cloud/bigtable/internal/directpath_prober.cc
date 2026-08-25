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

#include "google/cloud/bigtable/internal/directpath_prober.h"
#include "google/cloud/bigtable/instance_resource.h"
#include "google/cloud/bigtable/internal/bigtable_stub.h"
#include "google/cloud/bigtable/internal/bigtable_stub_factory.h"
#include "google/cloud/bigtable/internal/data_connection_impl.h"
#include "google/cloud/bigtable/internal/defaults.h"
#include "google/cloud/bigtable/internal/endpoint_options.h"
#include "google/cloud/bigtable/internal/operation_context.h"
#include "google/cloud/bigtable/options.h"
#include "google/cloud/common_options.h"
#include "google/cloud/grpc_options.h"
#include "google/cloud/internal/make_status.h"
#include "absl/strings/match.h"
#include "absl/strings/numbers.h"
#include "absl/strings/str_split.h"
#include <google/bigtable/v2/bigtable.grpc.pb.h>
#include <grpcpp/grpcpp.h>
#include <algorithm>
#include <chrono>
#include <string_view>
#include <vector>

namespace google {
namespace cloud {
namespace bigtable_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

IpPreference ExtractIpPreference(std::string_view peer_address) {
  if (absl::StartsWith(peer_address, "ipv4:")) {
    return IpPreference::kIpv4;
  }
  if (absl::StartsWith(peer_address, "ipv6:")) {
    return IpPreference::kIpv6;
  }
  return IpPreference::kNone;
}

bool IsAltsNegotiated(grpc::AuthContext const& auth_ctx) {
  if (!auth_ctx.IsPeerAuthenticated()) return false;
  std::vector<grpc::string_ref> const sec_types =
      auth_ctx.FindPropertyValues("transport_security_type");
  return std::any_of(sec_types.begin(), sec_types.end(),
                     [](grpc::string_ref const& st) { return st == "alts"; });
}

bool IsAltsNegotiated(grpc::ClientContext const& context) {
  std::shared_ptr<grpc::AuthContext const> const auth_ctx =
      context.auth_context();
  if (auth_ctx == nullptr) return false;
  return IsAltsNegotiated(*auth_ctx);
}

bool IsPeerAuthenticated(grpc::ClientContext const& context) {
  std::shared_ptr<grpc::AuthContext const> const auth_ctx =
      context.auth_context();
  if (auth_ctx == nullptr) return false;
  return auth_ctx->IsPeerAuthenticated();
}

// Returns true if the peer IP address belongs to a DirectPath subnet.
// IPv6 connections over DirectPath use DirectPath IPv6 subnets, while IPv4
// connections route through the DirectPath 34.126.0.0/18 CIDR block.
bool IsDirectPathIp(std::string_view peer_address) {
  if (absl::StartsWith(peer_address, "ipv6:")) return true;
  if (absl::StartsWith(peer_address, "ipv4:")) {
    std::string_view const ip_port = peer_address.substr(5);
    std::vector<std::string_view> const parts = absl::StrSplit(ip_port, ':');
    if (!parts.empty()) {
      std::vector<std::string_view> const octets =
          absl::StrSplit(parts[0], '.');
      // DirectPath IPv4 addresses fall within the 34.126.0.0/18 subnet range
      // (34.126.0.0 to 34.126.63.255), where the first two octets are 34.126
      // and the third octet is between 0 and 63 inclusive.
      if (octets.size() == 4 && octets[0] == "34" && octets[1] == "126") {
        int third = 0;
        if (absl::SimpleAtoi(octets[2], &third) && third >= 0 && third <= 63) {
          return true;
        }
      }
    }
  }
  return false;
}

}  // namespace

std::string ToString(IpPreference preference) {
  switch (preference) {
    case IpPreference::kIpv4:
      return "ipv4";
    case IpPreference::kIpv6:
      return "ipv6";
    case IpPreference::kNone:
      return "";
  }
  return "";
}

StatusOr<DirectPathProbeResult> DirectPathProber::Probe(
    std::shared_ptr<internal::GrpcAuthenticationStrategy> const& auth,
    bigtable::InstanceResource const& instance_resource, Options const& options,
    CompletionQueue const& cq) {
  return Probe(auth, instance_resource, options, cq, nullptr, "", nullptr);
}

StatusOr<DirectPathProbeResult> DirectPathProber::Probe(
    std::shared_ptr<internal::GrpcAuthenticationStrategy> const& auth,
    bigtable::InstanceResource const& instance_resource, Options const& options,
    CompletionQueue const& cq, std::shared_ptr<BigtableStub> const& stub,
    std::string const& peer_address,
    std::shared_ptr<grpc::AuthContext const> const& auth_context) {
  std::shared_ptr<BigtableStub> effective_stub = stub;
  if (!effective_stub) {
    if (!auth) {
      return internal::InternalError("Auth strategy cannot be null",
                                     GCP_ERROR_INFO());
    }

    Options probe_options = options;
    probe_options.set<::google::cloud::bigtable_internal::DataEndpointOption>(
        bigtable::internal::DefaultDirectPathDataEndpoint());
    probe_options.set<EndpointOption>(
        bigtable::internal::DefaultDirectPathDataEndpoint());
    probe_options.set<AuthorityOption>(
        bigtable::internal::DefaultDirectPathAuthority());
    probe_options.set<bigtable::experimental::DirectPathModeOption>(
        bigtable::experimental::DirectPathMode::kEnabled);
    probe_options.set<GrpcNumChannelsOption>(1);
    probe_options.set<bigtable::MinConnectionRefreshOption>(
        std::chrono::milliseconds::zero());
    probe_options.set<bigtable::MaxConnectionRefreshOption>(
        std::chrono::milliseconds::zero());

    auto stub_factory = [](std::shared_ptr<grpc::Channel> channel) {
      return std::make_shared<DefaultBigtableStub>(
          google::bigtable::v2::Bigtable::NewStub(std::move(channel)));
    };
    effective_stub =
        CreateDecoratedStubs(auth, cq, probe_options, stub_factory);
  }

  std::chrono::milliseconds timeout =
      options.get<bigtable::experimental::DirectPathProbeTimeoutOption>();
  if (timeout <= std::chrono::milliseconds::zero()) {
    timeout = bigtable::internal::DefaultDirectPathProbeTimeout();
  }

  grpc::ClientContext client_context;
  client_context.set_deadline(std::chrono::system_clock::now() + timeout);

  google::bigtable::v2::PingAndWarmRequest request;
  request.set_name(instance_resource.FullName());
  if (options.has<bigtable::AppProfileIdOption>()) {
    request.set_app_profile_id(options.get<bigtable::AppProfileIdOption>());
  }
  OperationContext op_ctx;
  StatusOr<google::bigtable::v2::PingAndWarmResponse> response =
      effective_stub->PingAndWarm(client_context, options, request, op_ctx);
  if (!response.ok()) return response.status();

  DirectPathProbeResult result;
  result.peer_address =
      peer_address.empty() ? client_context.peer() : peer_address;
  bool const is_alts = auth_context != nullptr
                           ? IsAltsNegotiated(*auth_context)
                           : IsAltsNegotiated(client_context);
  bool const is_auth = auth_context != nullptr
                           ? auth_context->IsPeerAuthenticated()
                           : IsPeerAuthenticated(client_context);
  bool const is_dp_ip = IsDirectPathIp(result.peer_address);
  result.success = is_alts || (is_auth && is_dp_ip);
  result.ip_preference = result.success
                             ? ExtractIpPreference(result.peer_address)
                             : IpPreference::kNone;

  return result;
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable_internal
}  // namespace cloud
}  // namespace google
