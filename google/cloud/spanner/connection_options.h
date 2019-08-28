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

#ifndef GOOGLE_CLOUD_CPP_SPANNER_GOOGLE_CLOUD_SPANNER_CONNECTION_OPTIONS_H_
#define GOOGLE_CLOUD_CPP_SPANNER_GOOGLE_CLOUD_SPANNER_CONNECTION_OPTIONS_H_

#include "google/cloud/spanner/version.h"
#include "google/cloud/status_or.h"
#include <google/spanner/admin/database/v1/spanner_database_admin.grpc.pb.h>
#include <google/spanner/v1/spanner.pb.h>
#include <grpcpp/grpcpp.h>
#include <cstdint>
#include <set>
#include <string>

namespace google {
namespace cloud {
namespace spanner {
inline namespace SPANNER_CLIENT_NS {
/**
 * The configuration parameters for spanner connections.
 */
class ConnectionOptions {
 public:
  /// The default options, using `grpc::GoogleDefaultCredentials()`.
  ConnectionOptions();

  /// Default parameters, using an explicit credentials object.
  explicit ConnectionOptions(std::shared_ptr<grpc::ChannelCredentials> c);

  ConnectionOptions& set_credentials(
      std::shared_ptr<grpc::ChannelCredentials> v) {
    credentials_ = std::move(v);
    return *this;
  }
  std::shared_ptr<grpc::ChannelCredentials> credentials() const {
    return credentials_;
  }

  ConnectionOptions& set_endpoint(std::string v) {
    endpoint_ = std::move(v);
    return *this;
  }
  std::string const& endpoint() const { return endpoint_; }

  bool clog_enabled() const { return clog_enabled_; }
  ConnectionOptions& enable_clog() {
    clog_enabled_ = true;
    return *this;
  }
  ConnectionOptions& disable_clog() {
    clog_enabled_ = false;
    return *this;
  }

  bool tracing_enabled(std::string const& component) const {
    return tracing_components_.find(component) != tracing_components_.end();
  }
  ConnectionOptions& enable_tracing(std::string const& component) {
    tracing_components_.insert(component);
    return *this;
  }
  ConnectionOptions& disable_tracing(std::string const& component) {
    tracing_components_.erase(component);
    return *this;
  }

  std::string channel_pool_domain() const { return channel_pool_domain_; }
  ConnectionOptions& set_channel_pool_domain(std::string v) {
    channel_pool_domain_ = std::move(v);
    return *this;
  }

 private:
  std::shared_ptr<grpc::ChannelCredentials> credentials_;
  std::string endpoint_;
  bool clog_enabled_ = false;
  std::set<std::string> tracing_components_;
  std::string channel_pool_domain_;
};

}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_SPANNER_GOOGLE_CLOUD_SPANNER_CONNECTION_OPTIONS_H_
