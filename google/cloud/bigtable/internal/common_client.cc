// Copyright 2018 Google Inc.
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

#include "google/cloud/bigtable/internal/common_client.h"

namespace google {
namespace cloud {
namespace bigtable {
inline namespace BIGTABLE_CLIENT_NS {
namespace internal {

std::vector<std::shared_ptr<grpc::Channel>> CreateChannelPool(
    std::string const& endpoint, bigtable::ClientOptions const& options) {
  std::vector<std::shared_ptr<grpc::Channel>> result;
  for (std::size_t i = 0; i != options.connection_pool_size(); ++i) {
    auto args = options.channel_arguments();
    if (!options.connection_pool_name().empty()) {
      args.SetString("cbt-c++/connection-pool-name",
                     options.connection_pool_name());
    }
    args.SetInt("cbt-c++/connection-pool-id", static_cast<int>(i));
    result.push_back(
        grpc::CreateCustomChannel(endpoint, options.credentials(), args));
  }
  return result;
}

}  // namespace internal
}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable
}  // namespace cloud
}  // namespace google
