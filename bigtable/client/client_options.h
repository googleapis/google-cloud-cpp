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

#ifndef BIGTABLE_CLIENT_CLIENTOPTIONS_H_
#define BIGTABLE_CLIENT_CLIENTOPTIONS_H_

#include "bigtable/client/version.h"

#include <grpc++/grpc++.h>

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
  const std::string& data_endpoint() const { return data_endpoint_; }
  ClientOptions& set_data_endpoint(std::string endpoint) {
    data_endpoint_ = std::move(endpoint);
    return *this;
  }

  /// Return the current endpoint for admin RPCs.
  const std::string& admin_endpoint() const { return admin_endpoint_; }
  ClientOptions& set_admin_endpoint(std::string endpoint) {
    admin_endpoint_ = std::move(endpoint);
    return *this;
  }

  /// Return the current credentials.
  std::shared_ptr<grpc::ChannelCredentials> credentials() const {
    return credentials_;
  }
  ClientOptions& SetCredentials(
      std::shared_ptr<grpc::ChannelCredentials> credentials) {
    credentials_ = credentials;
    return *this;
  }

  // TODO(#53) create setter/getter for each channel argument.
  const grpc::ChannelArguments channel_arguments() const {
    return channel_arguments_;
  }
  ClientOptions& set_channel_arguments(
      grpc::ChannelArguments channel_arguments) {
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
   * Set the grpclb fallback timeout (in ms) for the channel.
   *
   * Please see the docs for grpc::ChannelArguments::SetGrpclbFallbackTimeout()
   * on https://grpc.io/grpc/cpp/classgrpc_1_1_channel_arguments.html
   * for more details.
   *
   */
  void SetGrpclbFallbackTimeout(int fallback_timeout) {
    channel_arguments_.SetGrpclbFallbackTimeout(fallback_timeout);
  }

  /**
   * Set Socket Mutator for channel.
   *
   * Please see the docs for grpc::ChannelArguments::SetSocketMutator()
   * on https://grpc.io/grpc/cpp/classgrpc_1_1_channel_arguments.html
   * for more details.
   *
   */
  void SetSocketMutator(grpc_socket_mutator* mutator) {
    channel_arguments_.SetSocketMutator(mutator);
  }

  /**
   * Set the string to prepend to the user agent.
   *
   * Please see the docs for grpc::ChannelArguments::SetUserAgentPrefix()
   * on https://grpc.io/grpc/cpp/classgrpc_1_1_channel_arguments.html
   * for more details.
   *
   */
  void SetUserAgentPrefix(const grpc::string& user_agent_prefix) {
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
  void SetResourceQuota(const grpc::ResourceQuota& resource_quota) {
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
   * Please see the docs for grpc::ChannelArguments::SetLoadBalancingPolicyName()
   * on https://grpc.io/grpc/cpp/classgrpc_1_1_channel_arguments.html
   * for more details.
   *
   */
  void SetLoadBalancingPolicyName(const grpc::string& lb_policy_name) {
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
  void SetServiceConfigJSON(const grpc::string& service_config_json) {
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
  void SetSslTargetNameOverride(const grpc::string& name) {
    channel_arguments_.SetSslTargetNameOverride(name);
  }

 private:
  std::string data_endpoint_;
  std::string admin_endpoint_;
  std::shared_ptr<grpc::ChannelCredentials> credentials_;
  grpc::ChannelArguments channel_arguments_;
};
}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable

#endif  // BIGTABLE_CLIENT_CLIENTOPTIONS_H_
