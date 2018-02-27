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

#ifndef GOOGLE_CLOUD_CPP_BIGTABLE_CLIENT_CLIENT_OPTIONS_H_
#define GOOGLE_CLOUD_CPP_BIGTABLE_CLIENT_CLIENT_OPTIONS_H_

#include "bigtable/client/version.h"

#include <grpc++/grpc++.h>

#include "bigtable/client/internal/throw_delegate.h"

namespace bigtable {
inline namespace BIGTABLE_CLIENT_NS {
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
  ClientOptions();

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

  /// Set the size of the connection pool.
  ClientOptions& set_connection_pool_size(std::size_t size) {
    if (size == 0) {
      internal::RaiseRangeError(
          "ClientOptions::set_connection_pool_size requires size > 0");
    }
    connection_pool_size_ = size;
    return *this;
  }
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
   * Set the grpclb fallback timeout with the timestamp @p fallback_timeout
   * for the channel.
   *
   * @throws std::range_error if the @p fallback_timeout parameter is too large.
   *     Currently gRPC uses `int` to represent the timeout, and it is expressed
   *     in milliseconds. Therefore, the maximum timeout is about 50 days on
   *     platforms where `int` is a 32-bit number.
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
   * Please see the docs for grpc::ChannelArguments::SetGrpclbFallbackTimeout()
   * on https://grpc.io/grpc/cpp/classgrpc_1_1_channel_arguments.html
   * for more details.
   *
   */
  template <typename Rep, typename Period>
  void SetGrpclbFallbackTimeout(
      std::chrono::duration<Rep, Period> fallback_timeout) {
    std::chrono::milliseconds ft_ms =
        std::chrono::duration_cast<std::chrono::milliseconds>(fallback_timeout);

    if (ft_ms.count() > std::numeric_limits<int>::max()) {
      internal::RaiseRangeError("Duration Exceeds Range for int");
    }
    auto fallback_timeout_ms = static_cast<int>(ft_ms.count());
    channel_arguments_.SetGrpclbFallbackTimeout(fallback_timeout_ms);
  }

  /**
   * Set the string to prepend to the user agent.
   *
   * Please see the docs for grpc::ChannelArguments::SetUserAgentPrefix()
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
   * Please see the docs for grpc::ChannelArguments::SetResourceQuota()
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
   * Please see the docs for grpc::ChannelArguments::SetMaxReceiveMessageSize()
   * on https://grpc.io/grpc/cpp/classgrpc_1_1_channel_arguments.html
   * for more details.
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

 private:
  std::string data_endpoint_;
  std::string admin_endpoint_;
  std::shared_ptr<grpc::ChannelCredentials> credentials_;
  grpc::ChannelArguments channel_arguments_;
  std::string connection_pool_name_;
  std::size_t connection_pool_size_;
};
}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable

#endif  // GOOGLE_CLOUD_CPP_BIGTABLE_CLIENT_CLIENT_OPTIONS_H_
