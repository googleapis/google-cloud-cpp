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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_INTERNAL_SUBSCRIPTION_MESSAGE_SOURCE_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_INTERNAL_SUBSCRIPTION_MESSAGE_SOURCE_H

#include "google/cloud/pubsub/version.h"
#include "google/cloud/future.h"
#include "google/cloud/status.h"
#include <google/pubsub/v1/pubsub.pb.h>
#include <functional>
#include <string>

namespace google {
namespace cloud {
namespace pubsub_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

using MessageCallback =
    std::function<void(google::pubsub::v1::ReceivedMessage)>;

/**
 * Defines the interface for one-message-at-a-time sources.
 *
 * A message source generates messages via `MessageCallback` callbacks.
 * Typically the source is some sort of queue that receives
 * `AsyncStreamingPull()` responses and breaks them into smaller messages.
 */
class SubscriptionMessageSource {
 public:
  virtual ~SubscriptionMessageSource() = default;

  /// Start the source, set up the callback. Calling multiple times should have
  /// no effect, only the first callback is used.
  virtual void Start(MessageCallback) = 0;

  /// Shutdown the source, cancel any outstanding requests and or timers. No
  /// callbacks should be generated after this call.
  virtual void Shutdown() = 0;

  /// Request more messages from the source.
  virtual void Read(std::size_t max_callbacks) = 0;

  /**
   * Positive acknowledgment the message associated with @p ack_id.
   *
   * The application has successfully handled this message and no new deliveries
   * are necessary.
   */
  virtual void AckMessage(std::string const& ack_id) = 0;

  /**
   * Reject the message associated with @p ack_id.
   *
   * The application was not able to handle this message. Rejecting a message
   * allows the service to re-deliver it, subject to the topic and subscription
   * configuration.
   */
  virtual void NackMessage(std::string const& ack_id) = 0;
};

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace pubsub_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_INTERNAL_SUBSCRIPTION_MESSAGE_SOURCE_H
