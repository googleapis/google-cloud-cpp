// Copyright 2020 Google LLC
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

#include "google/cloud/pubsub/internal/create_channel.h"
#include "google/cloud/pubsub/internal/emulator_overrides.h"

namespace google {
namespace cloud {
namespace pubsub_internal {
inline namespace GOOGLE_CLOUD_CPP_PUBSUB_NS {

/// Create a gRPC channel with the right configuration.
std::shared_ptr<grpc::Channel> CreateChannel(pubsub::ConnectionOptions options,
                                             int channel_id) {
  options = EmulatorOverrides(std::move(options));
  auto channel_arguments = options.CreateChannelArguments();
  // Newer versions of gRPC include a macro (`GRPC_ARG_CHANNEL_ID`) but use
  // its value here to allow compiling against older versions.
  channel_arguments.SetInt("grpc.channel_id", channel_id);
  // Pub/Sub messages are often larger than the default gRPC limit (4MiB). How
  // much bigger is a bit of a guess. The application-level payload cannot be
  // larger than 10MiB, but there is the overhead in the protos, and the gRPC
  // overhead (auth tokens, headers, etc) to consider. We set the limits to
  // 16MiB because (a) it is a round number, (b) it generously exceeds any
  // reasonable overhead, and (c) typically applications have dozens of channels
  // and rarely more than 100, so even if too generous it is unlikely to be
  // material.
  channel_arguments.SetMaxSendMessageSize(16 * 1024 * 1024);
  channel_arguments.SetMaxReceiveMessageSize(16 * 1024 * 1024);

  return grpc::CreateCustomChannel(options.endpoint(), options.credentials(),
                                   std::move(channel_arguments));
}

}  // namespace GOOGLE_CLOUD_CPP_PUBSUB_NS
}  // namespace pubsub_internal
}  // namespace cloud
}  // namespace google
