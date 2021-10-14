// Copyright 2021 Google LLC
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

#include "google/cloud/grpc_options.h"
#include "google/cloud/common_options.h"
#include "google/cloud/internal/absl_str_join_quiet.h"
#include "google/cloud/internal/background_threads_impl.h"

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {

grpc::ChannelArguments MakeChannelArguments(Options const& opts) {
  auto channel_arguments = opts.get<GrpcChannelArgumentsNativeOption>();
  for (auto const& p : opts.get<GrpcChannelArgumentsOption>()) {
    channel_arguments.SetString(p.first, p.second);
  }
  auto const& user_agent_prefix = opts.get<UserAgentProductsOption>();
  if (!user_agent_prefix.empty()) {
    channel_arguments.SetUserAgentPrefix(absl::StrJoin(user_agent_prefix, " "));
  }
  return channel_arguments;
}

absl::optional<int> GetIntChannelArgument(grpc::ChannelArguments const& args,
                                          std::string const& key) {
  auto c_args = args.c_channel_args();
  // Just do a linear search for the key; the data structure is not organized
  // in any other useful way.
  for (auto const* a = c_args.args; a != c_args.args + c_args.num_args; ++a) {
    if (key != a->key) continue;
    if (a->type != GRPC_ARG_INTEGER) return absl::nullopt;
    return a->value.integer;
  }
  return absl::nullopt;
}

absl::optional<std::string> GetStringChannelArgument(
    grpc::ChannelArguments const& args, std::string const& key) {
  auto c_args = args.c_channel_args();
  // Just do a linear search for the key; the data structure is not organized
  // in any other useful way.
  for (auto const* a = c_args.args; a != c_args.args + c_args.num_args; ++a) {
    if (key != a->key) continue;
    if (a->type != GRPC_ARG_STRING) return absl::nullopt;
    return a->value.string;
  }
  return absl::nullopt;
}

BackgroundThreadsFactory MakeBackgroundThreadsFactory(Options const& opts) {
  if (opts.has<GrpcCompletionQueueOption>()) {
    auto const& cq = opts.get<GrpcCompletionQueueOption>();
    return [cq] {
      return absl::make_unique<CustomerSuppliedBackgroundThreads>(cq);
    };
  }
  if (opts.has<GrpcBackgroundThreadsFactoryOption>()) {
    return opts.get<GrpcBackgroundThreadsFactoryOption>();
  }
  auto const s = opts.get<GrpcBackgroundThreadPoolSizeOption>();
  return [s] {
    return absl::make_unique<AutomaticallyCreatedBackgroundThreads>(s);
  };
}

}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google
