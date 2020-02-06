// Copyright 2020 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_PUBSUB_GOOGLE_CLOUD_PUBSUB_CONNECTION_OPTIONS_H
#define GOOGLE_CLOUD_CPP_PUBSUB_GOOGLE_CLOUD_PUBSUB_CONNECTION_OPTIONS_H

#include "google/cloud/pubsub/version.h"
#include <grpcpp/grpcpp.h>

namespace google {
namespace cloud {
namespace pubsub {
inline namespace GOOGLE_CLOUD_CPP_PUBSUB_NS {

/// TODO(#23) - placeholder, replace with real class later.
class ConnectionOptions {
 public:
  ConnectionOptions() : ConnectionOptions(grpc::GoogleDefaultCredentials()) {}
  explicit ConnectionOptions(
      std::shared_ptr<grpc::ChannelCredentials> credentials);

  ConnectionOptions& set_endpoint(std::string v) {
    endpoint_ = std::move(v);
    return *this;
  }
  std::string const& endpoint() const { return endpoint_; }

  std::shared_ptr<grpc::ChannelCredentials> credentials() const {
    return credentials_;
  }

 private:
  std::string endpoint_;
  std::shared_ptr<grpc::ChannelCredentials> credentials_;
};

}  // namespace GOOGLE_CLOUD_CPP_PUBSUB_NS
}  // namespace pubsub
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_PUBSUB_GOOGLE_CLOUD_PUBSUB_CONNECTION_OPTIONS_H
