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

#ifndef GOOGLE_CLOUD_CPP_SPANNER_GOOGLE_CLOUD_SPANNER_CLIENT_OPTIONS_H_
#define GOOGLE_CLOUD_CPP_SPANNER_GOOGLE_CLOUD_SPANNER_CLIENT_OPTIONS_H_

#include "google/cloud/spanner/version.h"
#include "google/cloud/status_or.h"
#include <google/spanner/admin/database/v1/spanner_database_admin.grpc.pb.h>
#include <grpcpp/grpcpp.h>

namespace google {
namespace cloud {
namespace spanner {
inline namespace SPANNER_CLIENT_NS {
/**
 * The configuration parameters for spanner clients.
 */
class ClientOptions {
 public:
  /// The default options, using `grpc::GoogleDefaultCredentials()`.
  ClientOptions();

  /// Default parameters, using an explicit credentials object.
  explicit ClientOptions(std::shared_ptr<grpc::ChannelCredentials> c);

  ClientOptions& set_credentials(std::shared_ptr<grpc::ChannelCredentials> v) {
    credentials_ = std::move(v);
    return *this;
  }
  std::shared_ptr<grpc::ChannelCredentials> credentials() const {
    return credentials_;
  }

  ClientOptions& set_admin_endpoint(std::string v) {
    admin_endpoint_ = std::move(v);
    return *this;
  }
  std::string const& admin_endpoint() const { return admin_endpoint_; }

 private:
  std::shared_ptr<grpc::ChannelCredentials> credentials_;
  std::string admin_endpoint_;
};

}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_SPANNER_GOOGLE_CLOUD_SPANNER_CLIENT_OPTIONS_H_
