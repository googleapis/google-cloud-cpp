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
#include "google/cloud/storage/client_options.h"
#include "google/cloud/storage/grpc_plugin.h"
#include "google/cloud/storage/options.h"
#include "google/cloud/common_options.h"
#include "google/cloud/grpc_options.h"
#include "google/cloud/internal/detect_gcp.h"
#include "google/cloud/internal/getenv.h"
#include "google/cloud/internal/populate_common_options.h"
#include "google/cloud/internal/service_endpoint.h"
#include "google/cloud/universe_domain_options.h"
#include "absl/strings/match.h"
#include <grpcpp/grpcpp.h>
#include <algorithm>
#include <thread>

namespace google {
namespace cloud {
namespace storage_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

auto constexpr kMinMetricsPeriod = std::chrono::seconds(5);
auto constexpr kDefaultMetricsPeriod = std::chrono::seconds(60);

int DefaultGrpcNumChannels(std::string const& endpoint) {
  // When using Direct Connectivity the gRPC library already does load balancing
  // across multiple sockets, it makes little sense to perform additional load
  // balancing in the client library.
  if (absl::StartsWith(endpoint, "google-c2p:///") ||
      absl::StartsWith(endpoint, "google-c2p-experimental:///")) {
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

}  // namespace

Options DefaultOptionsGrpc(
    Options options,
    std::shared_ptr<internal::GcpDetector> const& gcp_detector) {
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

  auto const testbench =
      GetEnv("CLOUD_STORAGE_EXPERIMENTAL_GRPC_TESTBENCH_ENDPOINT");
  if (testbench.has_value() && !testbench->empty()) {
    options.set<EndpointOption>(*testbench);
    // The emulator does not support HTTPS or authentication, use insecure
    // (sometimes called "anonymous") credentials, which disable SSL.
    options.set<UnifiedCredentialsOption>(MakeInsecureCredentials());
  }

  // gRPC <= 1.64 may crash when metrics are enabled, so we don't enable them by
  // default.
  //     https://github.com/grpc/grpc/pull/36664
  auto const enable_grpc_metrics =
      !testbench.has_value() && GrpcEnableMetricsIsSafe();

  auto const ep = google::cloud::internal::UniverseDomainEndpoint(
      "storage.googleapis.com", options);

  // Set default to direct connectivity if we can detect we are running in GCP
  // and there is not already a set endpoint or unviverse domain endpoint.
  if ((!options.has<EndpointOption>() &&
       !options.has<internal::UniverseDomainOption>()) &&
      (gcp_detector->IsGoogleCloudBios() ||
       gcp_detector->IsGoogleCloudServerless())) {
    options.set<EndpointOption>("google-c2p:///storage.googleapis.com");
  }

  options = google::cloud::internal::MergeOptions(
      std::move(options),
      Options{}
          .set<EndpointOption>(ep)
          .set<AuthorityOption>(ep)
          .set<storage_experimental::EnableGrpcMetricsOption>(
              enable_grpc_metrics)
          .set<storage_experimental::GrpcMetricsPeriodOption>(
              kDefaultMetricsPeriod));
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

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_internal
}  // namespace cloud
}  // namespace google
