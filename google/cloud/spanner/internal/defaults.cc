// Copyright 2021 Google LLC
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

#include "google/cloud/spanner/internal/defaults.h"
#include "google/cloud/spanner/internal/session_pool.h"
#include "google/cloud/spanner/options.h"
#include "google/cloud/grpc_options.h"
#include "google/cloud/internal/getenv.h"
#include "google/cloud/internal/populate_common_options.h"
#include "google/cloud/internal/populate_grpc_options.h"
#include "google/cloud/options.h"
#include <chrono>
#include <string>

namespace google {
namespace cloud {
namespace spanner_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

namespace {

// Sets basic defaults that apply to normal and admin connections.
void SetBasicDefaults(Options& opts) {
  opts = internal::PopulateCommonOptions(
      std::move(opts), "GOOGLE_CLOUD_CPP_SPANNER_DEFAULT_ENDPOINT",
      "SPANNER_EMULATOR_HOST", "GOOGLE_CLOUD_CPP_SPANNER_DEFAULT_AUTHORITY",
      "spanner.googleapis.com");
  opts = internal::PopulateGrpcOptions(std::move(opts));
  if (!opts.has<GrpcNumChannelsOption>()) {
    opts.set<GrpcNumChannelsOption>(4);
  }
}

}  // namespace

Options DefaultOptions(Options opts) {
  SetBasicDefaults(opts);

  if (!opts.has<spanner::SpannerRetryPolicyOption>()) {
    opts.set<spanner::SpannerRetryPolicyOption>(
        std::make_shared<google::cloud::spanner::LimitedTimeRetryPolicy>(
            std::chrono::minutes(10)));
  }
  if (!opts.has<spanner::SpannerBackoffPolicyOption>()) {
    auto constexpr kBackoffScaling = 2.0;
    opts.set<spanner::SpannerBackoffPolicyOption>(
        std::make_shared<google::cloud::spanner::ExponentialBackoffPolicy>(
            std::chrono::milliseconds(100), std::chrono::minutes(1),
            kBackoffScaling));
  }
  if (!opts.has<spanner::QueryOptimizerVersionOption>()) {
    auto e = internal::GetEnv("SPANNER_OPTIMIZER_VERSION");
    if (e && !e->empty()) {
      opts.set<spanner::QueryOptimizerVersionOption>(*std::move(e));
    }
  }
  if (!opts.has<spanner::QueryOptimizerStatisticsPackageOption>()) {
    auto e = internal::GetEnv("SPANNER_OPTIMIZER_STATISTICS_PACKAGE");
    if (e && !e->empty()) {
      opts.set<spanner::QueryOptimizerStatisticsPackageOption>(*std::move(e));
    }
  }

  // Sets Spanner-specific session-pool options.
  auto& num_channels = opts.lookup<GrpcNumChannelsOption>();
  num_channels = (std::max)(num_channels, 1);
  if (!opts.has<spanner::SessionPoolMinSessionsOption>()) {
    opts.set<spanner::SessionPoolMinSessionsOption>(25 * num_channels);
  }
  if (!opts.has<spanner::SessionPoolMaxSessionsPerChannelOption>()) {
    opts.set<spanner::SessionPoolMaxSessionsPerChannelOption>(100);
  }
  if (!opts.has<spanner::SessionPoolActionOnExhaustionOption>()) {
    opts.set<spanner::SessionPoolActionOnExhaustionOption>(
        spanner::ActionOnExhaustion::kBlock);
  }
  if (!opts.has<spanner::SessionPoolKeepAliveIntervalOption>()) {
    opts.set<spanner::SessionPoolKeepAliveIntervalOption>(
        std::chrono::minutes(55));
  }
  if (!opts.has<SessionPoolClockOption>()) {
    opts.set<SessionPoolClockOption>(std::make_shared<Session::Clock>());
  }
  // Enforces some SessionPool constraints.
  auto& max_idle = opts.lookup<spanner::SessionPoolMaxIdleSessionsOption>();
  max_idle = (std::max)(max_idle, 0);
  auto& max_sessions_per_channel =
      opts.lookup<spanner::SessionPoolMaxSessionsPerChannelOption>();
  max_sessions_per_channel = (std::max)(max_sessions_per_channel, 1);
  auto& min_sessions = opts.lookup<spanner::SessionPoolMinSessionsOption>();
  min_sessions = (std::max)(min_sessions, 0);
  min_sessions =
      (std::min)(min_sessions, max_sessions_per_channel * num_channels);

  if (!opts.has<spanner::RouteToLeaderOption>()) {
    // The option defaults to on (unset), but the default can be changed with a
    // suitably-negative value in `${GOOGLE_CLOUD_CPP_SPANNER_ROUTE_TO_LEADER}`.
    if (auto e = internal::GetEnv("GOOGLE_CLOUD_CPP_SPANNER_ROUTE_TO_LEADER")) {
      for (auto const* disable : {"N", "n", "F", "f", "0", "off"}) {
        if (*e == disable) {
          // Change the default from "for RW/PartitionedDml transactions"
          // to "never".
          opts.set<spanner::RouteToLeaderOption>(false);
          break;
        }
      }
    }
  }

  return opts;
}

// Sets the options that have different defaults for admin connections, then
// uses `DefaultOptions()` to set all the remaining defaults.
Options DefaultAdminOptions(Options opts) {
  SetBasicDefaults(opts);

  // Manually default `GrpcCredentialOption`, because the legacy admin stubs do
  // not support GUAC (aka `UnifiedCredentialsOption`).
  auto e = internal::GetEnv("SPANNER_EMULATOR_HOST");
  if (e && !e->empty()) {
    opts.set<GrpcCredentialOption>(grpc::InsecureChannelCredentials());
  } else if (!opts.has<GrpcCredentialOption>()) {
    opts.set<GrpcCredentialOption>(grpc::GoogleDefaultCredentials());
  }

  if (!opts.has<spanner::SpannerRetryPolicyOption>()) {
    opts.set<spanner::SpannerRetryPolicyOption>(
        std::make_shared<google::cloud::spanner::LimitedTimeRetryPolicy>(
            std::chrono::minutes(30)));
  }
  if (!opts.has<spanner::SpannerBackoffPolicyOption>()) {
    auto constexpr kBackoffScaling = 2.0;
    opts.set<spanner::SpannerBackoffPolicyOption>(
        std::make_shared<google::cloud::spanner::ExponentialBackoffPolicy>(
            std::chrono::seconds(1), std::chrono::minutes(5), kBackoffScaling));
  }
  if (!opts.has<spanner::SpannerPollingPolicyOption>()) {
    auto constexpr kBackoffScaling = 2.0;
    opts.set<spanner::SpannerPollingPolicyOption>(
        std::make_shared<google::cloud::spanner::GenericPollingPolicy<>>(
            google::cloud::spanner::LimitedTimeRetryPolicy(
                std::chrono::minutes(30)),
            google::cloud::spanner::ExponentialBackoffPolicy(
                std::chrono::seconds(10), std::chrono::minutes(5),
                kBackoffScaling)));
  }

  return opts;
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace spanner_internal
}  // namespace cloud
}  // namespace google
