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
inline namespace GOOGLE_CLOUD_CPP_NS {

namespace internal {

grpc::ChannelArguments MakeChannelArguments(Options const& opts) {
  grpc::ChannelArguments channel_arguments;
  for (auto const& p : opts.get<GrpcChannelArgumentsOption>()) {
    channel_arguments.SetString(p.first, p.second);
  }
  auto const& user_agent_prefix = opts.get<UserAgentProductsOption>();
  if (!user_agent_prefix.empty()) {
    channel_arguments.SetUserAgentPrefix(absl::StrJoin(user_agent_prefix, " "));
  }
  return channel_arguments;
}

BackgroundThreadsFactory MakeBackgroundThreadsFactory(Options const& opts) {
  if (opts.has<GrpcBackgroundThreadsFactoryOption>())
    return opts.get<GrpcBackgroundThreadsFactoryOption>();
  auto const s = opts.get<GrpcBackgroundThreadPoolSizeOption>();
  return [s] {
    return absl::make_unique<
        ::google::cloud::internal::AutomaticallyCreatedBackgroundThreads>(s);
  };
}

}  // namespace internal

}  // namespace GOOGLE_CLOUD_CPP_NS
}  // namespace cloud
}  // namespace google
