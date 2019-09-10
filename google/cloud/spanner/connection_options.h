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

namespace internal {
std::string BaseUserAgentPrefix();
}  // namespace internal

/**
 * The configuration parameters for spanner connections.
 */
class ConnectionOptions {
 public:
  /// The default options, using `grpc::GoogleDefaultCredentials()`.
  ConnectionOptions();

  /// Default parameters, using an explicit credentials object.
  explicit ConnectionOptions(std::shared_ptr<grpc::ChannelCredentials> c);

  /// Change the gRPC credentials from the default of
  /// `grpc::GoogleDefaultCredentials()`.
  ConnectionOptions& set_credentials(
      std::shared_ptr<grpc::ChannelCredentials> v) {
    credentials_ = std::move(v);
    return *this;
  }

  /// The gRPC credentials used by clients configured with this object.
  std::shared_ptr<grpc::ChannelCredentials> credentials() const {
    return credentials_;
  }

  /**
   * Change the gRPC endpoint used to contact the Cloud Spanner service.
   *
   * In almost all cases the default ("spanner.googleapis.com") is the correct
   * endpoint to use. It may need to be changed to (1) test against a fake or
   * simulator, or (2) to use a beta or EAP version of the service.
   */
  ConnectionOptions& set_endpoint(std::string v) {
    endpoint_ = std::move(v);
    return *this;
  }

  /// The endpoint used by clients configured with this object.
  std::string const& endpoint() const { return endpoint_; }

  /**
   * Return whether tracing is enabled for the given @p component.
   *
   * The Cloud Spanner C++ client can log interesting events to help library and
   * application developers troubleshoot problems with the library or their
   * configuration. This flag returns true if tracing should be enabled by
   * clients configured with this option.
   *
   * Currently only the `rpc` component is supported, which enables logging of
   * each RPC, including its parameters and any responses.
   */
  bool tracing_enabled(std::string const& component) const {
    return tracing_components_.find(component) != tracing_components_.end();
  }

  /// Enable tracing for @p component in clients configured with this object.
  ConnectionOptions& enable_tracing(std::string const& component) {
    tracing_components_.insert(component);
    return *this;
  }

  /// Disable tracing for @p component in clients configured with this object.
  ConnectionOptions& disable_tracing(std::string const& component) {
    tracing_components_.erase(component);
    return *this;
  }

  /**
   * Define the gRPC channel domain for clients configured with this object.
   *
   * In some cases applications may want to use a separate gRPC connections for
   * different clients. gRPC may share the connection used by separate channels
   * created with the same configuration. The client objects created with this
   * object will create gRPC channels configured with
   * `grpc.channel_pooling_domain` set to the value returned by
   * `channel_pool_domain()`. gRPC channels with different values for
   * `grpc.channel_pooling_domain` are guaranteed to use different connections.
   * Note that there is no guarantee that channels with the same value will have
   * the same connection though.
   *
   * This option might be useful for applications that want to segregate traffic
   * for whatever reason.
   */
  std::string channel_pool_domain() const { return channel_pool_domain_; }

  /// Set the value for `channel_pool_domain()`.
  ConnectionOptions& set_channel_pool_domain(std::string v) {
    channel_pool_domain_ = std::move(v);
    return *this;
  }

  /**
   * Prepend @p prefix to the user-agent string.
   *
   * Libraries or services that use the Cloud Spanner C++ client may want to
   * set their own user-agent prefix. This can help them develop telemetry
   * information about number of users running particular versions of their
   * system or library.
   */
  ConnectionOptions& add_user_agent_prefix(std::string prefix) {
    prefix += " ";
    prefix += user_agent_prefix_;
    user_agent_prefix_ = std::move(prefix);
    return *this;
  }

  /// Return the current value for the user agent string.
  std::string const& user_agent_prefix() const { return user_agent_prefix_; }

  /**
   * Create a new `grpc::ChannelArguments` configured with the options in this
   * object.
   *
   * The caller would typically pass this argument to
   * `grpc::CreateCustomChannel` and create one more more gRPC channels.
   */
  grpc::ChannelArguments CreateChannelArguments() const;

 private:
  std::shared_ptr<grpc::ChannelCredentials> credentials_;
  std::string endpoint_;
  std::set<std::string> tracing_components_;
  std::string channel_pool_domain_;

  std::string user_agent_prefix_;
};

}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_SPANNER_GOOGLE_CLOUD_SPANNER_CONNECTION_OPTIONS_H_
