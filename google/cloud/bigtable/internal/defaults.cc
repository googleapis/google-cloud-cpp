// Copyright 2021 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "google/cloud/bigtable/internal/defaults.h"
#include "google/cloud/bigtable/internal/client_options_defaults.h"
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
inline namespace BIGTABLE_CLIENT_NS {
namespace internal {

// As learned from experiments, idle gRPC connections enter IDLE state after 4m.
auto constexpr kDefaultMaxRefreshPeriod =
    std::chrono::milliseconds(std::chrono::minutes(3));

// Applications with hundreds of clients seem to work better with a longer
// delay for the initial refresh. As there is no particular rush, start with 1m.
auto constexpr kDefaultMinRefreshPeriod =
    std::chrono::milliseconds(std::chrono::minutes(1));

static_assert(kDefaultMinRefreshPeriod <= kDefaultMaxRefreshPeriod,
              "The default period range must be valid");

int DefaultConnectionPoolSize() {
  // For better resource utilization and greater throughput, it is recommended
  // to calculate the default pool size based on cores(CPU) available. However,
  // as per C++11 documentation `std::thread::hardware_concurrency()` cannot be
  // fully relied upon. It is only a hint and the value can be 0 if it is not
  // well defined or not computable. Apart from CPU count, multiple channels
  // can be opened for each CPU to increase throughput. The pool size is also
  // capped so that servers with many cores do not create too many channels.
  int cpu_count = std::thread::hardware_concurrency();
  if (cpu_count == 0) return BIGTABLE_CLIENT_DEFAULT_CONNECTION_POOL_SIZE;
  return (std::min)(BIGTABLE_CLIENT_DEFAULT_CONNECTION_POOL_SIZE_MAX,
                    cpu_count * BIGTABLE_CLIENT_DEFAULT_CHANNELS_PER_CPU);
}

Options DefaultOptions(Options opts) {
  auto emulator = google::cloud::internal::GetEnv("BIGTABLE_EMULATOR_HOST");
  if (emulator) {
    opts.set<DataEndpointOption>(*emulator);
    opts.set<AdminEndpointOption>(*emulator);
    opts.set<InstanceAdminEndpointOption>(*emulator);
  }

  auto instance_admin_emulator =
      google::cloud::internal::GetEnv("BIGTABLE_INSTANCE_ADMIN_EMULATOR_HOST");
  if (instance_admin_emulator) {
    opts.set<InstanceAdminEndpointOption>(*std::move(instance_admin_emulator));
  }

  if (!opts.has<DataEndpointOption>()) {
    opts.set<DataEndpointOption>("bigtable.googleapis.com");
  }
  if (!opts.has<AdminEndpointOption>()) {
    opts.set<AdminEndpointOption>("bigtableadmin.googleapis.com");
  }
  if (!opts.has<InstanceAdminEndpointOption>()) {
    opts.set<InstanceAdminEndpointOption>("bigtableadmin.googleapis.com");
  }
  if (!opts.has<GrpcCredentialOption>()) {
    opts.set<GrpcCredentialOption>(emulator ? grpc::InsecureChannelCredentials()
                                            : grpc::GoogleDefaultCredentials());
  }
  if (!opts.has<TracingComponentsOption>()) {
    opts.set<TracingComponentsOption>(
        ::google::cloud::internal::DefaultTracingComponents());
  }
  if (!opts.has<GrpcTracingOptionsOption>()) {
    opts.set<GrpcTracingOptionsOption>(
        ::google::cloud::internal::DefaultTracingOptions());
  }
  if (!opts.has<GrpcNumChannelsOption>()) {
    opts.set<GrpcNumChannelsOption>(DefaultConnectionPoolSize());
  }
  if (!opts.has<MinConnectionRefreshOption>()) {
    opts.set<MinConnectionRefreshOption>(kDefaultMinRefreshPeriod);
  }
  if (!opts.has<MaxConnectionRefreshOption>()) {
    opts.set<MaxConnectionRefreshOption>(kDefaultMaxRefreshPeriod);
  }

  return opts;
}

}  // namespace internal
}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable
}  // namespace cloud
}  // namespace google
