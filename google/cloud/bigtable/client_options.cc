// Copyright 2017 Google Inc.
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

#include "google/cloud/bigtable/client_options.h"
#include "google/cloud/bigtable/internal/client_options_defaults.h"
#include "google/cloud/internal/background_threads_impl.h"
#include "google/cloud/internal/build_info.h"
#include "google/cloud/internal/getenv.h"
#include "absl/strings/str_split.h"
#include <limits>
#include <thread>

namespace {
std::shared_ptr<grpc::ChannelCredentials> BigtableDefaultCredentials() {
  auto emulator = google::cloud::internal::GetEnv("BIGTABLE_EMULATOR_HOST");
  if (emulator.has_value()) {
    return grpc::InsecureChannelCredentials();
  }
  return grpc::GoogleDefaultCredentials();
}

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
auto constexpr kDefaultKeepaliveTime =
    std::chrono::milliseconds(std::chrono::seconds(30));

// The default value for GRPC_KEEPALIVE_TIME_MS, how long the sender (in this
// case the Cloud Bigtable C++ client library) waits for an acknowledgement for
// a keepalive ping.
auto constexpr kDefaultKeepaliveTimeout =
    std::chrono::milliseconds(std::chrono::seconds(10));

}  // anonymous namespace

namespace google {
namespace cloud {
namespace bigtable {
inline namespace BIGTABLE_CLIENT_NS {
namespace internal {

std::string DefaultDataEndpoint() {
  auto emulator = google::cloud::internal::GetEnv("BIGTABLE_EMULATOR_HOST");
  if (emulator.has_value()) return *std::move(emulator);
  return "bigtable.googleapis.com";
}

std::string DefaultAdminEndpoint() {
  auto emulator = google::cloud::internal::GetEnv("BIGTABLE_EMULATOR_HOST");
  if (emulator.has_value()) return *std::move(emulator);
  return "bigtableadmin.googleapis.com";
}

std::string DefaultInstanceAdminEndpoint() {
  auto emulator =
      google::cloud::internal::GetEnv("BIGTABLE_INSTANCE_ADMIN_EMULATOR_HOST");
  if (emulator.has_value()) return *std::move(emulator);
  return DefaultAdminEndpoint();
}

std::set<std::string> DefaultTracingComponents() {
  auto tracing =
      google::cloud::internal::GetEnv("GOOGLE_CLOUD_CPP_ENABLE_TRACING");
  if (!tracing.has_value()) return {};
  return absl::StrSplit(*tracing, ',');
}

TracingOptions DefaultTracingOptions() {
  auto tracing_options =
      google::cloud::internal::GetEnv("GOOGLE_CLOUD_CPP_TRACING_OPTIONS");
  if (!tracing_options) return {};
  return TracingOptions{}.SetOptions(*tracing_options);
}

}  // namespace internal

inline std::size_t CalculateDefaultConnectionPoolSize() {
  // For batter resource utilization and greater throughput, it is recommended
  // to calculate the default pool size based on cores(CPU) available. However,
  // as per C++11 documentation `std::thread::hardware_concurrency()` cannot be
  // fully rely upon, it is only a hint and the value can be 0 if it is not
  // well defined or not computable. Apart from CPU count, multiple channels
  // can be opened for each CPU to increase throughput. The pool size is also
  // capped so that servers with many cores do not create too many channels.
  std::size_t cpu_count = std::thread::hardware_concurrency();
  if (cpu_count == 0) return BIGTABLE_CLIENT_DEFAULT_CONNECTION_POOL_SIZE;
  return (
      std::min<std::size_t>)(BIGTABLE_CLIENT_DEFAULT_CONNECTION_POOL_SIZE_MAX,
                             cpu_count *
                                 BIGTABLE_CLIENT_DEFAULT_CHANNELS_PER_CPU);
}

ClientOptions::ClientOptions(std::shared_ptr<grpc::ChannelCredentials> creds)
    : credentials_(std::move(creds)),
      connection_pool_size_(CalculateDefaultConnectionPoolSize()),
      data_endpoint_("bigtable.googleapis.com"),
      admin_endpoint_("bigtableadmin.googleapis.com"),
      instance_admin_endpoint_("bigtableadmin.googleapis.com"),
      tracing_components_(internal::DefaultTracingComponents()),
      tracing_options_(internal::DefaultTracingOptions()),
      // Refresh connections before the server has a chance to disconnect them
      // due to being idle.
      max_conn_refresh_period_(kDefaultMaxRefreshPeriod),
      min_conn_refresh_period_(kDefaultMinRefreshPeriod) {
  using std::chrono::duration_cast;
  using std::chrono::milliseconds;

  static std::string const kUserAgentPrefix = UserAgentPrefix();
  channel_arguments_.SetUserAgentPrefix(kUserAgentPrefix);
  channel_arguments_.SetMaxSendMessageSize(
      BIGTABLE_CLIENT_DEFAULT_MAX_MESSAGE_LENGTH);
  channel_arguments_.SetMaxReceiveMessageSize(
      BIGTABLE_CLIENT_DEFAULT_MAX_MESSAGE_LENGTH);
  auto to_arg = [](std::chrono::milliseconds ms) {
    auto const count = ms.count();
    if (count >= (std::numeric_limits<int>::max)()) {
      return std::numeric_limits<int>::max();
    }
    if (count <= (std::numeric_limits<int>::min)()) {
      return std::numeric_limits<int>::min();
    }
    return static_cast<int>(ms.count());
  };
  channel_arguments_.SetInt(
      GRPC_ARG_KEEPALIVE_TIME_MS,
      to_arg(duration_cast<milliseconds>(kDefaultKeepaliveTime)));
  channel_arguments_.SetInt(
      GRPC_ARG_KEEPALIVE_TIMEOUT_MS,
      to_arg(duration_cast<milliseconds>(kDefaultKeepaliveTimeout)));
}

ClientOptions::ClientOptions() : ClientOptions(BigtableDefaultCredentials()) {
  data_endpoint_ = internal::DefaultDataEndpoint();
  admin_endpoint_ = internal::DefaultAdminEndpoint();
  instance_admin_endpoint_ = internal::DefaultInstanceAdminEndpoint();
  auto emulator = google::cloud::internal::GetEnv("BIGTABLE_EMULATOR_HOST");
  if (emulator.has_value()) {
    data_endpoint_ = *emulator;
    admin_endpoint_ = *emulator;
    instance_admin_endpoint_ = *emulator;
  }
  auto instance_admin_emulator =
      google::cloud::internal::GetEnv("BIGTABLE_INSTANCE_ADMIN_EMULATOR_HOST");
  if (instance_admin_emulator.has_value()) {
    instance_admin_endpoint_ = *instance_admin_emulator;
  }
}

// NOLINTNEXTLINE(readability-identifier-naming)
ClientOptions& ClientOptions::set_connection_pool_size(std::size_t size) {
  if (size == 0) {
    connection_pool_size_ = CalculateDefaultConnectionPoolSize();
    return *this;
  }
  connection_pool_size_ = size;
  return *this;
}

std::string ClientOptions::UserAgentPrefix() {
  std::string agent = "gcloud-cpp/" + version_string() + " " +
                      google::cloud::internal::compiler();
  return agent;
}

ClientOptions& ClientOptions::DisableBackgroundThreads(
    google::cloud::CompletionQueue const& cq) {
  background_threads_factory_ = [cq] {
    return absl::make_unique<
        ::google::cloud::internal::CustomerSuppliedBackgroundThreads>(cq);
  };
  return *this;
}

ClientOptions::BackgroundThreadsFactory
ClientOptions::background_threads_factory() const {
  if (background_threads_factory_) return background_threads_factory_;
  auto const s = background_thread_pool_size_;
  return [s] {
    return absl::make_unique<
        ::google::cloud::internal::AutomaticallyCreatedBackgroundThreads>(s);
  };
}

}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable
}  // namespace cloud
}  // namespace google
