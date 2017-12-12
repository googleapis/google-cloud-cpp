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

#include "bigtable/client/client_options.h"

namespace bigtable {
inline namespace BIGTABLE_CLIENT_NS {
// TODO() resolve issue #52
ClientOptions::ClientOptions() {
  char const *emulator = std::getenv("BIGTABLE_EMULATOR_HOST");
  if (emulator != nullptr) {
    endpoint_ = emulator;
    credentials_ = grpc::InsecureChannelCredentials();
  } else {
    endpoint_ = "bigtable.googleapis.com";
    credentials_ = grpc::GoogleDefaultCredentials();
  }
  channel_arguments_ = grpc::ChannelArguments();
}

void SetCompressionAlgorithm(grpc::ChannelArguments *channel_arguments,
                             const grpc_compression_algorithm algorithm) {
  channel_arguments->SetCompressionAlgorithm(algorithm);
}

void SetGrpclbFallbackTimeout(grpc::ChannelArguments *channel_arguments,
                              const int fallback_timeout) {
  channel_arguments->SetGrpclbFallbackTimeout(fallback_timeout);
}

void SetSocketMutator(grpc::ChannelArguments *channel_arguments,
                      grpc_socket_mutator *mutator) {
  channel_arguments->SetSocketMutator(mutator);
}

void SetUserAgentPrefix(grpc::ChannelArguments *channel_arguments,
                        const grpc::string &user_agent_prefix) {
  channel_arguments->SetUserAgentPrefix(user_agent_prefix);
}

void SetResourceQuota(grpc::ChannelArguments *channel_arguments,
                      const grpc::ResourceQuota &resource_quota) {
  channel_arguments->SetResourceQuota(resource_quota);
}

void SetMaxReceiveMessageSize(grpc::ChannelArguments *channel_arguments,
                              const int size) {
  channel_arguments->SetMaxReceiveMessageSize(size);
}

void SetMaxSendMessageSize(grpc::ChannelArguments *channel_arguments,
                           const int size) {
  channel_arguments->SetMaxSendMessageSize(size);
}

void SetLoadBalancingPolicyName(grpc::ChannelArguments *channel_arguments,
                                const grpc::string &lb_policy_name) {
  channel_arguments->SetLoadBalancingPolicyName(lb_policy_name);
}

void SetServiceConfigJSON(grpc::ChannelArguments *channel_arguments,
                          const grpc::string &service_config_json) {
  channel_arguments->SetServiceConfigJSON(service_config_json);
}

void SetSslTargetNameOverride(grpc::ChannelArguments *channel_arguments,
                              const grpc::string &name) {
  channel_arguments->SetSslTargetNameOverride(name);
}

}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable
