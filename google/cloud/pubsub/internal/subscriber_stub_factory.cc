// Copyright 2022 Google LLC
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

#include "google/cloud/pubsub/internal/subscriber_stub_factory.h"
#include "google/cloud/pubsub/internal/create_channel.h"

namespace google {
namespace cloud {
namespace pubsub_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

std::shared_ptr<SubscriberStub> CreateDefaultSubscriberStub(
    std::shared_ptr<grpc::Channel> channel) {
  return std::make_shared<DefaultSubscriberStub>(
      google::pubsub::v1::Subscriber::NewStub(std::move(channel)));
}

std::shared_ptr<SubscriberStub> CreateDefaultSubscriberStub(Options const& opts,
                                                            int channel_id) {
  return CreateDefaultSubscriberStub(CreateChannel(opts, channel_id));
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace pubsub_internal
}  // namespace cloud
}  // namespace google
