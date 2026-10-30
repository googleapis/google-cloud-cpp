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

#include "google/cloud/storage/internal/grpc/default_options.h"
#include "google/cloud/storage/grpc_plugin.h"
#include "google/cloud/storage/internal/grpc/channel_telemetry.h"
#include "google/cloud/storage/options.h"
#include "google/cloud/common_options.h"
#include "google/cloud/grpc_options.h"
#include "google/cloud/internal/detect_gcp.h"
#include "google/cloud/internal/getenv.h"
#include "google/cloud/internal/populate_common_options.h"
#include "google/cloud/internal/service_endpoint.h"
#include "google/cloud/internal/unified_grpc_credentials.h"
#include "google/cloud/universe_domain_options.h"
#include "absl/strings/match.h"
#include "absl/strings/str_cat.h"
#include <grpcpp/grpcpp.h>
#include <algorithm>
#include <string>
#include <string_view>
#include <thread>
#include <tuple>
#include <utility>

namespace google {
namespace cloud {
namespace storage_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

auto constexpr kMinMetricsPeriod = std::chrono::seconds(5);
auto constexpr kDefaultMetricsPeriod = std::chrono::seconds(60);
auto constexpr kDefaultMetricsExportTimeout = std::chrono::seconds(30);
bool constexpr kDefaultDirectPathXdsOverInterconnect = false;

/// The host of the DirectPath over Interconnect target. The service identifies
/// Interconnect traffic by this value in the `:authority` of each RPC, so it is
/// also the default authority for that target.
constexpr std::string_view kDirectPathInterconnectHost =
    "storage-direct.googleapis.com";

int DefaultGrpcNumChannels(std::string const& endpoint) {
  // When using Direct Connectivity the gRPC library already does load balancing
  // across multiple sockets, it makes little sense to perform additional load
  // balancing in the client library.
  if (absl::StartsWith(endpoint, kC2pPrefix) ||
      absl::StartsWith(endpoint, kC2pExperimentalPrefix)) {
    return 1;
  }
  // When not using Direct Connectivity, there are limits to the bandwidth per
  // channel, we want to create more channels to avoid hitting said limits.  The
  // value here is mostly a guess: we know 1 channel is too little for most
  // applications, but the ideal number depends on the workload.  The
  // application can always override this default, so it is not important to
  // have it exactly right.
  auto constexpr kMinimumChannels = 4;
  auto const count = std::thread::hardware_concurrency();
  return (std::max)(kMinimumChannels, static_cast<int>(count));
}

/// Returns true if @p endpoint targets `kDirectPathInterconnectHost` through
/// the `google-c2p` resolver, whatever its query parameters.
///
/// This is deliberately narrower than `DetectTransportType()`, which classifies
/// a `force-xds` target for any host as DirectPath over Interconnect. That
/// label describes the network path the client requested, and is what the
/// telemetry reports. The service decides Interconnect routing by the
/// `:authority` of each RPC instead, and recognizes only this host there, so a
/// `force-xds` target for any other host keeps the standard default authority.
bool TargetsDirectPathInterconnectHost(std::string_view endpoint) {
  for (std::string_view prefix : {kC2pPrefix, kC2pExperimentalPrefix}) {
    if (!absl::StartsWith(endpoint, prefix)) continue;
    std::string_view const host_and_query = endpoint.substr(prefix.size());
    std::string_view const host =
        host_and_query.substr(0, host_and_query.find_first_of("/?#:"));
    return host == kDirectPathInterconnectHost;
  }
  return false;
}

}  // namespace

Options DefaultOptionsGrpc(
    Options options,
    std::shared_ptr<internal::GcpDetector> const& gcp_detector) {
  return DefaultOptionsGrpc(std::move(options), gcp_detector,
                            GrpcForceXdsIsSupported());
}

Options DefaultOptionsGrpc(
    Options options, std::shared_ptr<internal::GcpDetector> const& gcp_detector,
    bool grpc_supports_force_xds) {
  using ::google::cloud::internal::GetEnv;
  // Experiments show that gRPC gets better upload throughput when the upload
  // buffer is at least 32MiB.
  auto constexpr kDefaultGrpcUploadBufferSize = 32 * 1024 * 1024L;
  options = google::cloud::internal::MergeOptions(
      std::move(options), Options{}.set<storage::UploadBufferSizeOption>(
                              kDefaultGrpcUploadBufferSize));
  options =
      storage::internal::DefaultOptionsWithCredentials(std::move(options));
  if (!options.has<UnifiedCredentialsOption>() &&
      !options.has<GrpcCredentialOption>()) {
    options.set<UnifiedCredentialsOption>(
        google::cloud::MakeGoogleDefaultCredentials(
            google::cloud::internal::MakeAuthOptions(options)));
  }

  auto const preserve_creds =
      GetEnv("GOOGLE_CLOUD_CPP_STORAGE_TESTING_PRESERVE_CREDENTIALS");
  auto const testbench =
      GetEnv("CLOUD_STORAGE_EXPERIMENTAL_GRPC_TESTBENCH_ENDPOINT");
  if (testbench.has_value() && !testbench->empty()) {
    options.set<EndpointOption>(*testbench);

    if (!preserve_creds.has_value()) {
      // The emulator does not support HTTPS or authentication, use insecure
      // (sometimes called "anonymous") credentials, which disable SSL.
      options.set<UnifiedCredentialsOption>(MakeInsecureCredentials());
    }
  }

  // gRPC <= 1.64 may crash when metrics are enabled, so we don't enable them by
  // default.
  //     https://github.com/grpc/grpc/pull/36664
  auto const enable_grpc_metrics =
      !testbench.has_value() && GrpcEnableMetricsIsSafe();

  auto const ep = google::cloud::internal::UniverseDomainEndpoint(
      "storage.googleapis.com", options);

  if (!options
           .has<storage_experimental::DirectPathXdsOverInterconnectOption>()) {
    options.set<storage_experimental::DirectPathXdsOverInterconnectOption>(
        kDefaultDirectPathXdsOverInterconnect);
  }

  // The environment variable takes precedence over the option.
  // An explicit "false" disables the feature even when the option is set, so
  // deployments can opt out without rebuilding the application. Any other value
  // is treated as if the variable were not set.
  auto const direct_path_interconnect_env =
      GetEnv("GOOGLE_CLOUD_ENABLE_DIRECT_PATH_XDS_OVER_INTERCONNECT");
  if (direct_path_interconnect_env.has_value()) {
    if (*direct_path_interconnect_env == "true") {
      options.set<storage_experimental::DirectPathXdsOverInterconnectOption>(
          true);
    } else if (*direct_path_interconnect_env == "false") {
      options.set<storage_experimental::DirectPathXdsOverInterconnectOption>(
          false);
    }
  }
  // gRPC < 1.85 ignores the `force-xds` query parameter, which would silently
  // connect over regular DirectPath or CloudPath while telemetry reports
  // `DirectPathInterconnect`. Leave the option set so
  // `LogChannelConfiguration()` can warn that the request was ignored.
  //     https://github.com/grpc/grpc/pull/42980
  bool const direct_path_interconnect =
      grpc_supports_force_xds &&
      options.get<storage_experimental::DirectPathXdsOverInterconnectOption>();

  // Unless the application configured an endpoint or universe domain, default
  // to direct connectivity: the Interconnect target when that feature is
  // enabled and supported, otherwise the standard target when running in GCP.
  if (!options.has<EndpointOption>() &&
      !options.has<internal::UniverseDomainOption>()) {
    if (direct_path_interconnect) {
      options.set<EndpointOption>(absl::StrCat(
          "google-c2p:///", kDirectPathInterconnectHost, "?force-xds"));
    } else if (gcp_detector->IsGoogleCloudBios() ||
               gcp_detector->IsGoogleCloudServerless()) {
      options.set<EndpointOption>("google-c2p:///storage.googleapis.com");
    }
  }

  // The service identifies DirectPath over Interconnect traffic by the
  // `:authority` of each RPC, which must be `storage-direct.googleapis.com`
  // rather than `storage.googleapis.com` (otherwise requests are routed as
  // regular DirectPath and the feature is silently inactive). Key this default
  // on the effective endpoint so an application-supplied Interconnect endpoint
  // gets the same authority, and skip it when gRPC ignores `force-xds` because
  // the channel does not use Interconnect then. The `*.googleapis.com`
  // certificate covers this hostname, so TLS verification needs no override.
  // Set `AuthorityOption` explicitly here because `MergeOptions()` below would
  // otherwise default it to `storage.googleapis.com`.
  std::string const& endpoint = options.get<EndpointOption>();
  bool const interconnect_endpoint =
      grpc_supports_force_xds &&
      DetectTransportType(endpoint) == TransportType::kDirectPathInterconnect;
  if (!options.has<AuthorityOption>() && interconnect_endpoint &&
      TargetsDirectPathInterconnectHost(endpoint)) {
    options.set<AuthorityOption>(std::string(kDirectPathInterconnectHost));
  }

  // Interconnect clients run outside Google Cloud, where ALTS is unavailable,
  // yet `grpc::GoogleDefaultCredentials()` selects ALTS for the (non-CFE) xDS
  // cluster behind a `force-xds` target. Have default credentials use TLS plus
  // the Application Default Credentials there instead. Unlike the authority
  // above this applies to any `force-xds` host: the choice of transport
  // security does not depend on which host is dialed.
  options.set<google::cloud::internal::GrpcGoogleDefaultTlsOnlyOption>(
      interconnect_endpoint);

  options = google::cloud::internal::MergeOptions(
      std::move(options),
      Options{}
          .set<EndpointOption>(ep)
          .set<AuthorityOption>(ep)
          .set<storage_experimental::EnableGrpcMetricsOption>(
              enable_grpc_metrics)
          .set<storage_experimental::GrpcMetricsPeriodOption>(
              kDefaultMetricsPeriod)
          .set<storage_experimental::GrpcMetricsExportTimeoutOption>(
              kDefaultMetricsExportTimeout));
  if (options.get<storage_experimental::GrpcMetricsPeriodOption>() <
      kMinMetricsPeriod) {
    options.set<storage_experimental::GrpcMetricsPeriodOption>(
        kMinMetricsPeriod);
  }
  // We can only compute this once the endpoint is known, so take an additional
  // step.
  auto const num_channels =
      DefaultGrpcNumChannels(options.get<EndpointOption>());
  return google::cloud::internal::MergeOptions(
      std::move(options), Options{}.set<GrpcNumChannelsOption>(num_channels));
}

bool GrpcEnableMetricsIsSafe(int major, int minor, int patch) {
  // Never happens. No 0.x version is supported or implements the version
  // macros, but it makes the rest more readable.
  if (major < 1) return false;
  if (major > 1) return true;
  if (minor <= 62) return false;
  if (minor == 63) return patch >= 1;
  if (minor == 64) return patch >= 1;
  return true;
}

bool GrpcEnableMetricsIsSafe() {
#ifndef GRPC_CPP_VERSION_MAJOR
  return false;
#else
  return GrpcEnableMetricsIsSafe(GRPC_CPP_VERSION_MAJOR, GRPC_CPP_VERSION_MINOR,
                                 GRPC_CPP_VERSION_PATCH);
#endif  // GRPC_CPP_VERSION_MAJOR
}

bool GrpcForceXdsIsSupported(int major, int minor, int patch) {
  // The `google-c2p` resolver started honoring `force-xds` in gRPC 1.85.0:
  //     https://github.com/grpc/grpc/pull/42980
  return std::make_tuple(major, minor, patch) >= std::make_tuple(1, 85, 0);
}

bool GrpcForceXdsIsSupported() {
#ifndef GRPC_CPP_VERSION_MAJOR
  return false;
#else
  return GrpcForceXdsIsSupported(GRPC_CPP_VERSION_MAJOR, GRPC_CPP_VERSION_MINOR,
                                 GRPC_CPP_VERSION_PATCH);
#endif  // GRPC_CPP_VERSION_MAJOR
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_internal
}  // namespace cloud
}  // namespace google
