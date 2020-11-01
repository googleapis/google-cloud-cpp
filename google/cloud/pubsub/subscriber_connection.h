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
#include "google/cloud/pubsub/application_callback.h"
#include "google/cloud/pubsub/backoff_policy.h"
#include "google/cloud/pubsub/connection_options.h"
#include "google/cloud/pubsub/internal/subscriber_stub.h"
#include "google/cloud/pubsub/message.h"
#include "google/cloud/pubsub/retry_policy.h"
#include "google/cloud/pubsub/subscriber_options.h"
#include "google/cloud/pubsub/subscription.h"
#include "google/cloud/pubsub/version.h"
#include "google/cloud/status_or.h"
#include <functional>

namespace google {
namespace cloud {
namespace pubsub {
inline namespace GOOGLE_CLOUD_CPP_PUBSUB_NS {

/**
 * A connection to the Cloud Pub/Sub service to receive events.
 *
 * This interface defines pure-virtual methods for each of the user-facing
 * overload sets in `Subscriber`. That is, all of `Subscriber`'s overloads will
 * forward to the one pure-virtual method declared in this interface. This
 * allows users to inject custom behavior (e.g., with a Google Mock object) in a
 * `Subscriber` object for use in their own tests.
 *
 * To create a concrete instance that connects you to the real Cloud Pub/Sub
 * service, see `MakeSubscriberConnection()`.
 *
 * @par The *Params nested classes
 * Applications may define classes derived from `SubscriberConnection`, for
 * example, because they want to mock the class. To avoid breaking all such
 * derived classes when we change the number or type of the arguments to the
 * member functions we define lightweight structures to pass the arguments.
 */
class SubscriberConnection {
 public:
  virtual ~SubscriberConnection() = 0;

  /// Wrap the arguments for `Subscribe()`
  struct SubscribeParams {
    ApplicationCallback callback;
  };

  /// Defines the interface for `Subscriber::Subscribe()`
  virtual future<Status> Subscribe(SubscribeParams p);
};

/**
 * Creates a new `SubscriberConnection` object to work with `Subscriber`.
 *
 * The `SubscriberConnection` class is not intended for direct use in
 * applications, it is provided for applications wanting to mock the
 * `SubscriberClient` behavior in their tests.
 *
 * @par Performance
 * Creating a new `SubscriberConnection` is relatively expensive. This typically
 * initiate connections to the service, and therefore these objects should be
 * shared and reused when possible. Note that gRPC reuses existing OS resources
 * (sockets) whenever possible, so applications may experience better
 * performance on the second (and subsequent) calls to this function with the
 * same `ConnectionOptions` parameters. However, this behavior is not guaranteed
 * and applications should not rely on it.
 *
 * @see `SubscriberConnection`
 *
 * @par Changing Retry Parameters Example
 * @snippet samples.cc subscriber-retry-settings
 *
 * @param subscription the Cloud Pub/Sub subscription used by the returned
 *     connection.
 * @param options configure the flow control and other parameters in the
 *     returned connection.
 * @param connection_options (optional) general configuration for this
 *    connection, this type is also used to configure `pubsub::Publisher`.
 * @param retry_policy control for how long (or how many times) are retryable
 *     RPCs attempted.
 * @param backoff_policy controls the backoff behavior between retry attempts,
 *     typically some form of exponential backoff with jitter.
 */
std::shared_ptr<SubscriberConnection> MakeSubscriberConnection(
    Subscription subscription, SubscriberOptions options = {},
    ConnectionOptions connection_options = {},
    std::unique_ptr<pubsub::RetryPolicy const> retry_policy = {},
    std::unique_ptr<pubsub::BackoffPolicy const> backoff_policy = {});

}  // namespace GOOGLE_CLOUD_CPP_PUBSUB_NS
}  // namespace pubsub

namespace pubsub_internal {
inline namespace GOOGLE_CLOUD_CPP_PUBSUB_NS {

std::shared_ptr<pubsub::SubscriberConnection> MakeSubscriberConnection(
    pubsub::Subscription subscription, pubsub::SubscriberOptions options,
    pubsub::ConnectionOptions connection_options,
    std::shared_ptr<SubscriberStub> stub,
    std::unique_ptr<pubsub::RetryPolicy const> retry_policy,
    std::unique_ptr<pubsub::BackoffPolicy const> backoff_policy);

}  // namespace GOOGLE_CLOUD_CPP_PUBSUB_NS
}  // namespace pubsub_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_SUBSCRIBER_CONNECTION_H
