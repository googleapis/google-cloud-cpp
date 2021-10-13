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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_INTERNAL_CREATE_CHANNEL_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_INTERNAL_CREATE_CHANNEL_H

#include "google/cloud/pubsub/connection_options.h"
#include "google/cloud/pubsub/version.h"
#include <grpcpp/grpcpp.h>
#include <memory>

namespace google {
namespace cloud {
namespace pubsub_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

/// Create a gRPC channel with the right configuration.
std::shared_ptr<grpc::Channel> CreateChannel(Options const& opts,
                                             int channel_id);

/// Initialize Channel Arguments configured by @p opts and @p channel_id
grpc::ChannelArguments MakeChannelArguments(Options const& opts,
                                            int channel_id);

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace pubsub_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_INTERNAL_CREATE_CHANNEL_H
