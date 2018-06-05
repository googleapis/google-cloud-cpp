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

// Make the default pool size 4 because that is consistent with what Go does.
#ifndef BIGTABLE_CLIENT_DEFAULT_CONNECTION_POOL_SIZE
#define BIGTABLE_CLIENT_DEFAULT_CONNECTION_POOL_SIZE 4
#endif  // BIGTABLE_CLIENT_DEFAULT_CONNECTION_POOL_SIZE

namespace {
std::shared_ptr<grpc::ChannelCredentials> BigtableDefaultCredentials() {
  char const* emulator = std::getenv("BIGTABLE_EMULATOR_HOST");
  if (emulator != nullptr) {
    return grpc::InsecureChannelCredentials();
  }
  return grpc::GoogleDefaultCredentials();
}
}  // anonymous namespace

namespace google {
namespace cloud {
namespace bigtable {
inline namespace BIGTABLE_CLIENT_NS {
ClientOptions::ClientOptions(std::shared_ptr<grpc::ChannelCredentials> creds)
    : credentials_(std::move(creds)),
      connection_pool_size_(BIGTABLE_CLIENT_DEFAULT_CONNECTION_POOL_SIZE),
      data_endpoint_("bigtable.googleapis.com"),
      admin_endpoint_("bigtableadmin.googleapis.com") {
  static std::string const user_agent_prefix = "cbt-c++/" + version_string();
  channel_arguments_.SetUserAgentPrefix(user_agent_prefix);
}

ClientOptions::ClientOptions() : ClientOptions(BigtableDefaultCredentials()) {
  char const* emulator = std::getenv("BIGTABLE_EMULATOR_HOST");
  if (emulator != nullptr) {
    data_endpoint_ = emulator;
    admin_endpoint_ = emulator;
  }
}

}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable
}  // namespace cloud
}  // namespace google
