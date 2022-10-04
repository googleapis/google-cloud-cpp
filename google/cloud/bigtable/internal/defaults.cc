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

#include "google/cloud/bigtable/internal/defaults.h"
#include "google/cloud/bigtable/internal/client_options_defaults.h"
#include "google/cloud/bigtable/internal/rpc_policy_parameters.h"
#include "google/cloud/bigtable/options.h"
#include "google/cloud/common_options.h"
#include "google/cloud/connection_options.h"
#include "google/cloud/grpc_options.h"
#include "google/cloud/internal/getenv.h"
#include "google/cloud/internal/user_agent_prefix.h"
#include "google/cloud/options.h"
#include <chrono>
#include <string>

namespace google {
namespace cloud {
namespace bigtable {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {

namespace {

auto constexpr kBackoffScaling = 2.0;

// As learned from experiments, idle gRPC connections enter IDLE state after 4m.
auto constexpr kDefaultMaxRefreshPeriod =
    std::chrono::milliseconds(std::chrono::minutes(3));

// Applications with hundreds of clients seem to work better with a longer
// delay for the initial refresh. As there is no particular rush, start with 1m.
auto constexpr kDefaultMinRefreshPeriod =
    std::chrono::milliseconds(std::chrono::minutes(1));

static_assert(kDefaultMinRefreshPeriod <= kDefaultMaxRefreshPeriod,
              "The default period range must be valid");

// For background information on gRPC keepalive pings, see
//     https://github.com/grpc/grpc/blob/master/doc/keepalive.md

// The default value for GRPC_KEEPALIVE_TIME_MS, how long before a keepalive
// ping is sent. A better name may have been "period", but consistency with the
// gRPC naming seems valuable.
auto constexpr kDefaultKeepaliveTime = std::chrono::seconds(30);

// The default value for GRPC_KEEPALIVE_TIMEOUT_MS, how long the sender (in
// this case the Cloud Bigtable C++ client library) waits for an acknowledgement
// for a keepalive ping.
auto constexpr kDefaultKeepaliveTimeout = std::chrono::seconds(10);

Options DefaultConnectionRefreshOptions(Options opts) {
  auto const has_min = opts.has<MinConnectionRefreshOption>();
  auto const has_max = opts.has<MaxConnectionRefreshOption>();
  if (!has_min && !has_max) {
    opts.set<MinConnectionRefreshOption>(kDefaultMinRefreshPeriod);
    opts.set<MaxConnectionRefreshOption>(kDefaultMaxRefreshPeriod);
  } else if (has_min && !has_max) {
    opts.set<MaxConnectionRefreshOption>((std::max)(
        opts.get<MinConnectionRefreshOption>(), kDefaultMaxRefreshPeriod));
  } else if (!has_min && has_max) {
    opts.set<MinConnectionRefreshOption>((std::min)(
        opts.get<MaxConnectionRefreshOption>(), kDefaultMinRefreshPeriod));
  } else {
    // If the range is invalid, use the greater value as both the min and max
    auto const p = opts.get<MinConnectionRefreshOption>();
    if (p > opts.get<MaxConnectionRefreshOption>()) {
      opts.set<MaxConnectionRefreshOption>(std::move(p));
    }
  }
  return opts;
}

Options DefaultChannelArgumentOptions(Options opts) {
  using ::google::cloud::internal::GetIntChannelArgument;
  auto c_arg = [](std::chrono::milliseconds ms) {
    return static_cast<int>(ms.count());
  };
  auto& args = opts.lookup<GrpcChannelArgumentsNativeOption>();
  if (!GetIntChannelArgument(args, GRPC_ARG_MAX_SEND_MESSAGE_LENGTH)) {
    args.SetMaxSendMessageSize(BIGTABLE_CLIENT_DEFAULT_MAX_MESSAGE_LENGTH);
  }
  if (!GetIntChannelArgument(args, GRPC_ARG_MAX_RECEIVE_MESSAGE_LENGTH)) {
    args.SetMaxReceiveMessageSize(BIGTABLE_CLIENT_DEFAULT_MAX_MESSAGE_LENGTH);
  }
  if (!GetIntChannelArgument(args, GRPC_ARG_KEEPALIVE_TIME_MS)) {
    args.SetInt(GRPC_ARG_KEEPALIVE_TIME_MS, c_arg(kDefaultKeepaliveTime));
  }
  if (!GetIntChannelArgument(args, GRPC_ARG_KEEPALIVE_TIMEOUT_MS)) {
    args.SetInt(GRPC_ARG_KEEPALIVE_TIMEOUT_MS, c_arg(kDefaultKeepaliveTimeout));
  }
  return opts;
}

}  // namespace

int DefaultConnectionPoolSize() {
  // For better resource utilization and greater throughput, it is recommended
  // to calculate the default pool size based on cores(CPU) available. However,
  // as per C++11 documentation `std::thread::hardware_concurrency()` cannot be
  // fully relied upon. It is only a hint and the value can be 0 if it is not
  // well-defined or not computable. Apart from CPU count, multiple channels
  // can be opened for each CPU to increase throughput. The pool size is also
  // capped so that servers with many cores do not create too many channels.
  int cpu_count = std::thread::hardware_concurrency();
  if (cpu_count == 0) return BIGTABLE_CLIENT_DEFAULT_CONNECTION_POOL_SIZE;
  return (std::min)(BIGTABLE_CLIENT_DEFAULT_CONNECTION_POOL_SIZE_MAX,
                    cpu_count * BIGTABLE_CLIENT_DEFAULT_CHANNELS_PER_CPU);
}

Options DefaultOptions(Options opts) {
  using ::google::cloud::internal::GetEnv;

  if (opts.has<EndpointOption>()) {
    auto const& ep = opts.get<EndpointOption>();
    if (!opts.has<DataEndpointOption>()) {
      opts.set<DataEndpointOption>(ep);
    }
    if (!opts.has<AdminEndpointOption>()) {
      opts.set<AdminEndpointOption>(ep);
    }
    if (!opts.has<InstanceAdminEndpointOption>()) {
      opts.set<InstanceAdminEndpointOption>(ep);
    }
  }

  auto emulator = GetEnv("BIGTABLE_EMULATOR_HOST");
  if (emulator) {
    opts.set<DataEndpointOption>(*emulator);
    opts.set<AdminEndpointOption>(*emulator);
    opts.set<InstanceAdminEndpointOption>(*emulator);
  }

  auto instance_admin_emulator =
      GetEnv("BIGTABLE_INSTANCE_ADMIN_EMULATOR_HOST");
  if (instance_admin_emulator) {
    opts.set<InstanceAdminEndpointOption>(*std::move(instance_admin_emulator));
  }

  // Fill any missing default values.
  auto defaults =
      Options{}
          .set<DataEndpointOption>("bigtable.googleapis.com")
          .set<AdminEndpointOption>("bigtableadmin.googleapis.com")
          .set<InstanceAdminEndpointOption>("bigtableadmin.googleapis.com")
          .set<GrpcCredentialOption>(emulator
                                         ? grpc::InsecureChannelCredentials()
                                         : grpc::GoogleDefaultCredentials())
          .set<TracingComponentsOption>(
              ::google::cloud::internal::DefaultTracingComponents())
          .set<GrpcTracingOptionsOption>(
              ::google::cloud::internal::DefaultTracingOptions())
          .set<GrpcNumChannelsOption>(DefaultConnectionPoolSize());

  opts = google::cloud::internal::MergeOptions(std::move(opts),
                                               std::move(defaults));

  opts = DefaultConnectionRefreshOptions(std::move(opts));
  opts = DefaultChannelArgumentOptions(std::move(opts));

  // Inserts our user-agent string at the front.
  auto& products = opts.lookup<UserAgentProductsOption>();
  products.insert(products.begin(),
                  ::google::cloud::internal::UserAgentPrefix());

  return opts;
}

Options DefaultDataOptions(Options opts) {
  using ::google::cloud::internal::GetEnv;
  auto user_project = GetEnv("GOOGLE_CLOUD_CPP_USER_PROJECT");
  if (user_project && !user_project->empty()) {
    opts.set<UserProjectOption>(*std::move(user_project));
  }
  if (!opts.has<AuthorityOption>()) {
    opts.set<AuthorityOption>("bigtable.googleapis.com");
  }
  if (!opts.has<bigtable::DataRetryPolicyOption>()) {
    opts.set<bigtable::DataRetryPolicyOption>(
        bigtable::DataLimitedTimeRetryPolicy(
            kBigtableLimits.maximum_retry_period)
            .clone());
  }
  if (!opts.has<bigtable::DataBackoffPolicyOption>()) {
    opts.set<bigtable::DataBackoffPolicyOption>(
        ExponentialBackoffPolicy(kBigtableLimits.initial_delay / 2,
                                 kBigtableLimits.maximum_delay, kBackoffScaling)
            .clone());
  }
  if (!opts.has<bigtable::IdempotentMutationPolicyOption>()) {
    opts.set<bigtable::IdempotentMutationPolicyOption>(
        bigtable::DefaultIdempotentMutationPolicy());
  }
  opts = DefaultOptions(std::move(opts));
  return opts.set<EndpointOption>(opts.get<DataEndpointOption>());
}

Options DefaultInstanceAdminOptions(Options opts) {
  opts = DefaultOptions(std::move(opts));
  return opts.set<EndpointOption>(opts.get<InstanceAdminEndpointOption>());
}

Options DefaultTableAdminOptions(Options opts) {
  opts = DefaultOptions(std::move(opts));
  return opts.set<EndpointOption>(opts.get<AdminEndpointOption>());
}

}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable
}  // namespace cloud
}  // namespace google
