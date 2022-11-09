// Copyright 2022 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_INTERNAL_SUBSCRIBER_CONNECTION_IMPL_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_INTERNAL_SUBSCRIBER_CONNECTION_IMPL_H

#include "google/cloud/pubsub/ack_handler.h"
#include "google/cloud/pubsub/message.h"
#include "google/cloud/pubsub/pull_ack_handler.h"
#include "google/cloud/pubsub/subscriber_connection.h"
#include "google/cloud/status_or.h"
#include "google/cloud/version.h"

namespace google {
namespace cloud {
namespace pubsub_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

class SubscriberConnectionImpl : public pubsub::SubscriberConnection {
 public:
  explicit SubscriberConnectionImpl(
      pubsub::Subscription subscription, Options opts,
      std::shared_ptr<pubsub_internal::SubscriberStub> stub);

  ~SubscriberConnectionImpl() override;

  future<Status> Subscribe(SubscribeParams p) override;

  future<Status> ExactlyOnceSubscribe(ExactlyOnceSubscribeParams p) override;

  // TODO(#7187) - move to pubsub::SubscriberConnection
  struct PullResponse {
    pubsub::PullAckHandler handler;
    pubsub::Message message;
  };

  StatusOr<PullResponse> Pull();

  Options options() override;

 private:
  std::string MakeClientId();

  pubsub::Subscription const subscription_;
  Options const opts_;
  std::shared_ptr<pubsub_internal::SubscriberStub> stub_;
  std::shared_ptr<BackgroundThreads> background_;
  std::mutex mu_;
  internal::DefaultPRNG generator_;
};

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace pubsub_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_INTERNAL_SUBSCRIBER_CONNECTION_IMPL_H
