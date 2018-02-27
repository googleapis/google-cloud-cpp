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

#include "bigtable/client/client_options.h"

// Make the default pool size 4 because that is consistent with what Go does.
#ifndef BIGTABLE_CLIENT_DEFAULT_CONNECTION_POOL_SIZE
#define BIGTABLE_CLIENT_DEFAULT_CONNECTION_POOL_SIZE 4
#endif  // BIGTABLE_CLIENT_DEFAULT_CONNECTION_POOL_SIZE

namespace bigtable {
inline namespace BIGTABLE_CLIENT_NS {
ClientOptions::ClientOptions()
    : connection_pool_size_(BIGTABLE_CLIENT_DEFAULT_CONNECTION_POOL_SIZE) {
  char const* emulator = std::getenv("BIGTABLE_EMULATOR_HOST");
  if (emulator != nullptr) {
    data_endpoint_ = emulator;
    admin_endpoint_ = emulator;
    credentials_ = grpc::InsecureChannelCredentials();
  } else {
    data_endpoint_ = "bigtable.googleapis.com";
    admin_endpoint_ = "bigtableadmin.googleapis.com";
    credentials_ = grpc::GoogleDefaultCredentials();
  }
  channel_arguments_ = grpc::ChannelArguments();

  static std::string const prefix = "cbt-c++/" + version_string();
  channel_arguments_.SetUserAgentPrefix(prefix);
}

}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable
