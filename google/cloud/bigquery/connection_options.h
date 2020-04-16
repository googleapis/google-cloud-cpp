// Copyright 2019 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGQUERY_CONNECTION_OPTIONS_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGQUERY_CONNECTION_OPTIONS_H

#include "google/cloud/bigquery/version.h"
#include <grpcpp/grpcpp.h>
#include <memory>
#include <string>

namespace google {
namespace cloud {
namespace bigquery {
inline namespace BIGQUERY_CLIENT_NS {
class ConnectionOptions {
 public:
  ConnectionOptions();

  explicit ConnectionOptions(
      std::shared_ptr<grpc::ChannelCredentials> credentials);

  ConnectionOptions& set_credentials(
      std::shared_ptr<grpc::ChannelCredentials> credentials) {
    credentials_ = std::move(credentials);
    return *this;
  }

  std::shared_ptr<grpc::ChannelCredentials> credentials() const {
    return credentials_;
  }

  ConnectionOptions& set_bigquerystorage_endpoint(std::string endpoint) {
    bigquerystorage_endpoint_ = std::move(endpoint);
    return *this;
  }

  std::string const& bigquerystorage_endpoint() const {
    return bigquerystorage_endpoint_;
  }

  ConnectionOptions& add_user_agent_prefix(std::string prefix) {
    prefix += " ";
    prefix += user_agent_prefix_;
    user_agent_prefix_ = std::move(prefix);
    return *this;
  }

  std::string const& user_agent_prefix() const { return user_agent_prefix_; }

  grpc::ChannelArguments CreateChannelArguments() const;

 private:
  std::shared_ptr<grpc::ChannelCredentials> credentials_;
  std::string bigquerystorage_endpoint_;
  std::string user_agent_prefix_;
};

}  // namespace BIGQUERY_CLIENT_NS
}  // namespace bigquery
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGQUERY_CONNECTION_OPTIONS_H
