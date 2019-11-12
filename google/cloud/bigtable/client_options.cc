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
#include "google/cloud/internal/build_info.h"
#include "google/cloud/internal/getenv.h"
#include <thread>

namespace {
std::shared_ptr<grpc::ChannelCredentials> BigtableDefaultCredentials() {
  auto emulator = google::cloud::internal::GetEnv("BIGTABLE_EMULATOR_HOST");
  if (emulator.has_value()) {
    return grpc::InsecureChannelCredentials();
  }
  return grpc::GoogleDefaultCredentials();
}
}  // anonymous namespace

namespace google {
namespace cloud {
namespace bigtable {
inline namespace BIGTABLE_CLIENT_NS {
inline std::size_t CalculateDefaultConnectionPoolSize() {
  // For batter resource utilization and greater throughput, it is recommended
  // to calculate the default pool size based on cores(CPU) available. However,
  // as per C++11 documentation `std::thread::hardware_concurrency()` cannot be
  // fully rely upon, it is only a hint and the value can be 0 if it is not
  // well defined or not computable. Apart from CPU count, multiple channels can
  // be opened for each CPU to increase throughput.
  std::size_t cpu_count = std::thread::hardware_concurrency();
  return (cpu_count > 0) ? cpu_count * BIGTABLE_CLIENT_DEFAULT_CHANNELS_PER_CPU
                         : BIGTABLE_CLIENT_DEFAULT_CONNECTION_POOL_SIZE;
}

ClientOptions::ClientOptions(std::shared_ptr<grpc::ChannelCredentials> creds)
    : credentials_(std::move(creds)),
      connection_pool_size_(CalculateDefaultConnectionPoolSize()),
      data_endpoint_("bigtable.googleapis.com"),
      admin_endpoint_("bigtableadmin.googleapis.com"),
      instance_admin_endpoint_("bigtableadmin.googleapis.com") {
  static std::string const kUserAgentPrefix = UserAgentPrefix();
  channel_arguments_.SetUserAgentPrefix(kUserAgentPrefix);
  channel_arguments_.SetMaxSendMessageSize(
      BIGTABLE_CLIENT_DEFAULT_MAX_MESSAGE_LENGTH);
  channel_arguments_.SetMaxReceiveMessageSize(
      BIGTABLE_CLIENT_DEFAULT_MAX_MESSAGE_LENGTH);
}

ClientOptions::ClientOptions() : ClientOptions(BigtableDefaultCredentials()) {
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

}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable
}  // namespace cloud
}  // namespace google
