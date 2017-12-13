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
  const std::string& endpoint() const { return endpoint_; }
  ClientOptions& SetEndpoint(const std::string& endpoint) {
    endpoint_ = endpoint;
    return *this;
  }
  std::shared_ptr<grpc::ChannelCredentials> credentials() const {
    return credentials_;
  }
  ClientOptions& SetCredentials(
      std::shared_ptr<grpc::ChannelCredentials> credentials) {
    credentials_ = credentials;
    return *this;
  }
  // TODO() create setter/getter for each channel argument. Issue #53
  const grpc::ChannelArguments channel_arguments() const {
    return channel_arguments_;
  }
  ClientOptions& SetChannelArguments(grpc::ChannelArguments channel_arguments) {
    channel_arguments_ = channel_arguments;
    return *this;
  }

  /*
   * Set Compression Algorithm for channel.
   *
   * Please look at
   * https://grpc.io/grpc/cpp/classgrpc_1_1_channel_arguments.html#aaf332071bbdff5e4a7f3352f4ad564d5
   * for more details.
   *
   */
  inline void SetCompressionAlgorithm(grpc_compression_algorithm algorithm) {
    channel_arguments_.SetCompressionAlgorithm(algorithm);
  }

  /*
   * Set fallback timeout.
   *
   */
  inline void SetGrpclbFallbackTimeout(int fallback_timeout) {
    channel_arguments_.SetGrpclbFallbackTimeout(fallback_timeout);
  }

  /*
   * Set Socket Mutator for channel.
   *
   * Please look at
   * https://grpc.io/grpc/cpp/classgrpc_1_1_channel_arguments.html#a520690f499be85159a20200ddb986a96
   * for more details.
   *
   */
  inline void SetSocketMutator(grpc_socket_mutator* mutator) {
    channel_arguments_.SetSocketMutator(mutator);
  }

  /*
   * Set the string to prepend to the user agent.
   *
   * Please look at
   * https://grpc.io/grpc/cpp/classgrpc_1_1_channel_arguments.html#a0d6080324cabd7ba0833b95c77c0c907
   * for more details.
   *
   */
  inline void SetUserAgentPrefix(const grpc::string& user_agent_prefix) {
    channel_arguments_.SetUserAgentPrefix(user_agent_prefix);
  }

  /*
   * Set the buffer pool to be attached to the constructed channel.
   *
   * Please look at
   * https://grpc.io/grpc/cpp/classgrpc_1_1_channel_arguments.html#af569e816b047a77f08f0e8e0f585a4a5
   * for more details.
   *
   */
  inline void SetResourceQuota(const grpc::ResourceQuota& resource_quota) {
    channel_arguments_.SetResourceQuota(resource_quota);
  }

  /*
   * Set the buffer pool to be attached to the constructed channel.
   *
   * Please look at
   * https://grpc.io/grpc/cpp/classgrpc_1_1_channel_arguments.html#a2629f85664dd2822fe54059c4730baf8
   * for more details.
   *
   */
  inline void SetMaxReceiveMessageSize(int size) {
    channel_arguments_.SetMaxReceiveMessageSize(size);
  }

  /*
   * Set the buffer pool to be attached to the constructed channel.
   *
   * Please look at
   * https://grpc.io/grpc/cpp/classgrpc_1_1_channel_arguments.html#ac1fa513191e8104ec57dfd6598297ce5
   * for more details.
   *
   */
  inline void SetMaxSendMessageSize(int size) {
    channel_arguments_.SetMaxSendMessageSize(size);
  }

  /*
   * Set the buffer pool to be attached to the constructed channel.
   *
   * Please look at
   * https://grpc.io/grpc/cpp/classgrpc_1_1_channel_arguments.html#a860058f6b9fa340bb2075ae113e423a6
   * for more details.
   *
   */
  inline void SetLoadBalancingPolicyName(const grpc::string& lb_policy_name) {
    channel_arguments_.SetLoadBalancingPolicyName(lb_policy_name);
  }

  /*
   * Set service config in JSON form.
   *
   * Please visit
   * https://grpc.io/grpc/cpp/classgrpc_1_1_channel_arguments.html#a1537a3ed35f0f19d7aa8193fd1a1a513
   * for more details.
   *
   */
  inline void SetServiceConfigJSON(const grpc::string& service_config_json) {
    channel_arguments_.SetServiceConfigJSON(service_config_json);
  }

  /*
   * Set target name override for SSL host name checking.
   *
   * Please visit
   * https://grpc.io/grpc/cpp/classgrpc_1_1_channel_arguments.html#af4182e659448184f9618f079a1570328
   * for more details.
   *
   */
  inline void SetSslTargetNameOverride(const grpc::string& name) {
    channel_arguments_.SetSslTargetNameOverride(name);
  }

 private:
  // Endpoint here stands for data endpoint for fetching data.
  std::string endpoint_;
  std::shared_ptr<grpc::ChannelCredentials> credentials_;
  grpc::ChannelArguments channel_arguments_;
};
}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable

#endif  // BIGTABLE_CLIENT_CLIENTOPTIONS_H_
