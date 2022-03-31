// Copyright 2020 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "google/cloud/pubsub/internal/create_channel.h"

namespace google {
namespace cloud {
namespace pubsub_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

std::shared_ptr<grpc::Channel> CreateChannel(Options const& opts,
                                             int channel_id) {
  return grpc::CreateCustomChannel(opts.get<EndpointOption>(),
                                   opts.get<GrpcCredentialOption>(),
                                   MakeChannelArguments(opts, channel_id));
}

grpc::ChannelArguments MakeChannelArguments(Options const& opts,
                                            int channel_id) {
  auto args = internal::MakeChannelArguments(opts);
  args.SetInt(GRPC_ARG_CHANNEL_ID, channel_id);
  // Pub/Sub messages are often larger than the default gRPC limit (4MiB). How
  // much bigger is a bit of a guess. The application-level payload cannot be
  // larger than 10MiB, but there is the overhead in the protos, and the gRPC
  // overhead (auth tokens, headers, etc) to consider. We set the limits to
  // 16MiB because (a) it is a round number, (b) it generously exceeds any
  // reasonable overhead, and (c) while applications open many channels, their
  // total is rarely more than 100, so even if too generous it is unlikely to be
  // material. i.e. 6 MiB * 100 is not enough to worry about setting a more
  // strict limit.
  args.SetMaxSendMessageSize(16 * 1024 * 1024);
  args.SetMaxReceiveMessageSize(16 * 1024 * 1024);
  // Pub/Sub messages with EOS (Exactly Once Semantics) may have a larger
  // metadata size than is allowed by default.  Increase to 4 MiB.
  args.SetInt("grpc.max_metadata_size", 4 * 1024 * 1024);
  return args;
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace pubsub_internal
}  // namespace cloud
}  // namespace google
