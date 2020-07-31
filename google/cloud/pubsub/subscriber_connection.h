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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_SUBSCRIBER_CONNECTION_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_SUBSCRIBER_CONNECTION_H

#include "google/cloud/pubsub/ack_handler.h"
#include "google/cloud/pubsub/connection_options.h"
#include "google/cloud/pubsub/internal/subscriber_stub.h"
#include "google/cloud/pubsub/message.h"
#include "google/cloud/pubsub/subscription.h"
#include "google/cloud/pubsub/subscription_options.h"
#include "google/cloud/pubsub/version.h"
#include "google/cloud/status_or.h"
#include <functional>

namespace google {
namespace cloud {
namespace pubsub {
inline namespace GOOGLE_CLOUD_CPP_PUBSUB_NS {

/**
 * A connection to the Cloud Pub/Sub service to publish events.
 */
class SubscriberConnection {
 public:
  virtual ~SubscriberConnection() = 0;

  using CallbackType = std::function<void(Message, AckHandler)>;

  struct SubscribeParams {
    std::string full_subscription_name;
    CallbackType callback;
    SubscriptionOptions options;
  };
  virtual future<Status> Subscribe(SubscribeParams p) = 0;
};

/**
 * Returns a `SubscriberConnection` object to work with Cloud Pub/Sub subscriber
 * APIs.
 *
 * The `SubscriberConnection` class is not intended for direct use in
 * applications, it is provided for applications wanting to mock the
 * `SubscriberClient` behavior in their tests.
 *
 * @see `SubscriberConnection`
 *
 * @param options (optional) configure the `SubscriberConnection` created by
 *     this function.
 */
std::shared_ptr<SubscriberConnection> MakeSubscriberConnection(
    ConnectionOptions const& options = ConnectionOptions());

}  // namespace GOOGLE_CLOUD_CPP_PUBSUB_NS
}  // namespace pubsub

namespace pubsub_internal {
inline namespace GOOGLE_CLOUD_CPP_PUBSUB_NS {

std::shared_ptr<pubsub::SubscriberConnection> MakeSubscriberConnection(
    std::shared_ptr<SubscriberStub> stub,
    pubsub::ConnectionOptions const& options);

}  // namespace GOOGLE_CLOUD_CPP_PUBSUB_NS
}  // namespace pubsub_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_SUBSCRIBER_CONNECTION_H
