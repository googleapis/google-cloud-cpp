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
// limitations under the License.

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_CLIENT_OPTIONS_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_CLIENT_OPTIONS_H

#include "google/cloud/bigtable/version.h"
#include "google/cloud/status.h"
#include "google/cloud/tracing_options.h"
#include <grpcpp/grpcpp.h>
#include <grpcpp/resource_quota.h>
#include <set>
#include <string>

namespace google {
namespace cloud {
namespace bigtable {
inline namespace BIGTABLE_CLIENT_NS {
namespace internal {
struct InstanceAdminTraits;
std::string DefaultDataEndpoint();
std::string DefaultAdminEndpoint();
std::string DefaultInstanceAdminEndpoint();
TracingOptions DefaultTracingOptions();
}  // namespace internal

/**
 * Configuration options for the Bigtable Client.
 *
 * Applications typically configure the client class using:
 * @code
 * auto client =
 *   bigtable::Client(bigtable::ClientOptions().SetCredentials(...));
 * @endcode
 */
class ClientOptions {
 public:
  /**
   * Initialize the client options with the default credentials.
   *
   * Configure the client to connect to the Cloud Bigtable service, using the
   * default Google credentials for authentication.
   *
   * @par Environment Variables
   * If the `BIGTABLE_EMULATOR_HOST` environment variable is set, the default
   * configuration changes in important ways:
   *
   * - The credentials are initialized to `grpc::InsecureCredentials()`.
   * - Any client created with these objects will connect to the endpoint
   *   (typically just a `host:port` string) set in the environment variable.
   *
   * This makes it easy to test applications using the Cloud Bigtable Emulator.
   *
   * @see The Google Cloud Platform introduction to
   * [application default
   * credentials](https://cloud.google.com/docs/authentication/production)
   * @see `grpc::GoogleDefaultCredentials` in the [grpc
   * documentation](https://grpc.io/grpc/cpp/namespacegrpc.html)
   * @see The [documentation](https://cloud.google.com/bigtable/docs/emulator)
   * for the Cloud Bigtable Emulator.
   */
  ClientOptions();

  /**
   * Connect to the production instance of Cloud Bigtable using @p creds.
   *
   * This constructor always connects to the production instance of Cloud
   * Bigtable, and can be used when the application default credentials are
   * not configured in the environment where the application is running.
   */
  explicit ClientOptions(std::shared_ptr<grpc::ChannelCredentials> creds);

  /// Return the current endpoint for data RPCs.
  std::string const& data_endpoint() const { return data_endpoint_; }
  ClientOptions& set_data_endpoint(std::string endpoint) {
    data_endpoint_ = std::move(endpoint);
    return *this;
  }

  /// Return the current endpoint for admin RPCs.
  std::string const& admin_endpoint() const { return admin_endpoint_; }
  ClientOptions& set_admin_endpoint(std::string endpoint) {
    admin_endpoint_ = std::move(endpoint);
    // These two endpoints are generally equivalent, but they may differ in
    // some tests.
    instance_admin_endpoint_ = admin_endpoint_;
    return *this;
  }

  /**
   * Set the name of the connection pool.
   *
   * gRPC typically opens a single connection for each destination.  To improve
   * performance, the Cloud Bigtable C++ client can open multiple connections
   * to a given destination, but these connections are shared by all threads
   * in the application.  Sometimes the application may want even more
   * segregation, for example, the application may want to use a different pool
   * for high-priority requests vs. lower priority ones.  Using different names
   * creates segregated pools.
   */
  ClientOptions& set_connection_pool_name(std::string name) {
    connection_pool_name_ = std::move(name);
    return *this;
  }
  /// Return the name of the connection pool.
  std::string const& connection_pool_name() const {
    return connection_pool_name_;
  }

  /* Set the size of the connection pool.
   *
   * Specifying 0 for @p size will set the size of the connection pool to
   * default.
   */
  ClientOptions& set_connection_pool_size(std::size_t size);

  std::size_t connection_pool_size() const { return connection_pool_size_; }

  /// Return the current credentials.
  std::shared_ptr<grpc::ChannelCredentials> credentials() const {
    return credentials_;
  }
  ClientOptions& SetCredentials(
      std::shared_ptr<grpc::ChannelCredentials> credentials) {
    credentials_ = std::move(credentials);
    return *this;
  }

  /// Access all the channel arguments.
  grpc::ChannelArguments channel_arguments() const {
    return channel_arguments_;
  }

  /// Set all the channel arguments.
  ClientOptions& set_channel_arguments(
      grpc::ChannelArguments const& channel_arguments) {
    channel_arguments_ = channel_arguments;
    return *this;
  }

  /**
   * Set compression algorithm for channel.
   *
   * Please see the docs for grpc::ChannelArguments::SetCompressionAlgorithm()
   * on https://grpc.io/grpc/cpp/classgrpc_1_1_channel_arguments.html
   * for more details.
   *
   */
  void SetCompressionAlgorithm(grpc_compression_algorithm algorithm) {
    channel_arguments_.SetCompressionAlgorithm(algorithm);
  }

  /**
   * Set the `grpclb` fallback timeout with the timestamp @p fallback_timeout
   * for the channel.
   *
   * For example:
   *
   * @code
   * bigtable::ClientOptions::SetGrpclbFallbackTimeout(
   *     std::chrono::milliseconds(5000))
   * bigtable::ClientOptions::SetGrpclbFallbackTimeout(
   *     std::chrono::seconds(5))
   * @endcode
   *
   * @tparam Rep a placeholder to match the Rep tparam for @p fallback_timeout,
   *     the semantics of this template parameter are documented in
   *     `std::chrono::duration<>` (in brief, the underlying arithmetic type
   *     used to store the number of ticks), for our purposes it is simply a
   *     formal parameter.
   * @tparam Period a placeholder to match the Period tparam for @p
   *     fallback_timeout, the semantics of this template parameter are
   *     documented in `std::chrono::duration<>` (in brief, the length of the
   *     tick in seconds, expressed as a `std::ratio<>`), for our purposes it
   *     is simply a formal parameter.
   *
   * @see
   * [std::chrono::duration<>](http://en.cppreference.com/w/cpp/chrono/duration)
   *     for more details.
   *
   * Please see the docs for
   * `grpc::ChannelArguments::SetGrpclbFallbackTimeout()` on
   * https://grpc.io/grpc/cpp/classgrpc_1_1_channel_arguments.html for more
   * details.
   *
   */
  template <typename Rep, typename Period>
  google::cloud::Status SetGrpclbFallbackTimeout(
      std::chrono::duration<Rep, Period> fallback_timeout) {
    std::chrono::milliseconds ft_ms =
        std::chrono::duration_cast<std::chrono::milliseconds>(fallback_timeout);

    if (ft_ms.count() > std::numeric_limits<int>::max()) {
      return google::cloud::Status(google::cloud::StatusCode::kOutOfRange,
                                   "The supplied duration is larger than the "
                                   "maximum value allowed by gRPC (INT_MAX)");
    }
    auto fallback_timeout_ms = static_cast<int>(ft_ms.count());
    channel_arguments_.SetGrpclbFallbackTimeout(fallback_timeout_ms);
    return google::cloud::Status();
  }

  /**
   * Set the string to prepend to the user agent.
   *
   * Please see the docs for `grpc::ChannelArguments::SetUserAgentPrefix()`
   * on https://grpc.io/grpc/cpp/classgrpc_1_1_channel_arguments.html
   * for more details.
   *
   */
  void SetUserAgentPrefix(grpc::string const& user_agent_prefix) {
    channel_arguments_.SetUserAgentPrefix(user_agent_prefix);
  }

  /**
   * Set the buffer pool to be attached to the constructed channel.
   *
   * Please see the docs for `grpc::ChannelArguments::SetResourceQuota()`
   * on https://grpc.io/grpc/cpp/classgrpc_1_1_channel_arguments.html
   * for more details.
   *
   */
  void SetResourceQuota(grpc::ResourceQuota const& resource_quota) {
    channel_arguments_.SetResourceQuota(resource_quota);
  }

  /**
   * Set the max receive message size in bytes. -1 means unlimited.
   *
   * Please see the docs for
   * `grpc::ChannelArguments::SetMaxReceiveMessageSize()` on
   * https://grpc.io/grpc/cpp/classgrpc_1_1_channel_arguments.html for more
   * details.
   *
   */
  void SetMaxReceiveMessageSize(int size) {
    channel_arguments_.SetMaxReceiveMessageSize(size);
  }

  /**
   * Set the max send message size in bytes. -1 means unlimited.
   *
   * Please see the docs for grpc::ChannelArguments::SetMaxSendMessageSize()
   * on https://grpc.io/grpc/cpp/classgrpc_1_1_channel_arguments.html
   * for more details.
   *
   */
  void SetMaxSendMessageSize(int size) {
    channel_arguments_.SetMaxSendMessageSize(size);
  }

  /**
   * Set LB policy name.
   *
   * Please see the docs for
   * grpc::ChannelArguments::SetLoadBalancingPolicyName()
   * on https://grpc.io/grpc/cpp/classgrpc_1_1_channel_arguments.html
   * for more details.
   *
   */
  void SetLoadBalancingPolicyName(grpc::string const& lb_policy_name) {
    channel_arguments_.SetLoadBalancingPolicyName(lb_policy_name);
  }

  /**
   * Set service config in JSON form.
   *
   * Please see the docs for grpc::ChannelArguments::SetServiceConfigJSON()
   * on https://grpc.io/grpc/cpp/classgrpc_1_1_channel_arguments.html
   * for more details.
   *
   */
  void SetServiceConfigJSON(grpc::string const& service_config_json) {
    channel_arguments_.SetServiceConfigJSON(service_config_json);
  }

  /**
   * Set target name override for SSL host name checking.
   *
   * Please see the docs for grpc::ChannelArguments::SetSslTargetNameOverride()
   * on https://grpc.io/grpc/cpp/classgrpc_1_1_channel_arguments.html
   * for more details.
   *
   */
  void SetSslTargetNameOverride(grpc::string const& name) {
    channel_arguments_.SetSslTargetNameOverride(name);
  }

  /// Return the user agent prefix used by the library.
  static std::string UserAgentPrefix();

  /**
   * Return whether tracing is enabled for the given @p component.
   *
   * The C++ clients can log interesting events to help library and application
   * developers troubleshoot problems. This flag returns true if tracing should
   * be enabled by clients configured with this option.
   */
  bool tracing_enabled(std::string const& component) const {
    return tracing_components_.find(component) != tracing_components_.end();
  }

  /// Enable tracing for @p component in clients configured with this object.
  ClientOptions& enable_tracing(std::string const& component) {
    tracing_components_.insert(component);
    return *this;
  }

  /// Disable tracing for @p component in clients configured with this object.
  ClientOptions& disable_tracing(std::string const& component) {
    tracing_components_.erase(component);
    return *this;
  }

  /// Return the options for use when tracing RPCs.
  TracingOptions const& tracing_options() const { return tracing_options_; }

 private:
  friend struct internal::InstanceAdminTraits;
  friend struct ClientOptionsTestTraits;

  /// Return the current endpoint for instance admin RPCs.
  std::string const& instance_admin_endpoint() const {
    return instance_admin_endpoint_;
  }

  std::shared_ptr<grpc::ChannelCredentials> credentials_;
  grpc::ChannelArguments channel_arguments_;
  std::string connection_pool_name_;
  std::size_t connection_pool_size_;
  std::string data_endpoint_;
  std::string admin_endpoint_;
  // The endpoint for instance admin operations, in most scenarios this should
  // have the same value as `admin_endpoint_`. The most common exception is
  // testing, where the emulator for instance admin operations may be different
  // than the emulator for admin and data operations.
  std::string instance_admin_endpoint_;
  std::set<std::string> tracing_components_;
  TracingOptions tracing_options_;
};
}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_CLIENT_OPTIONS_H
