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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_SUBSCRIPTION_OPTIONS_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_SUBSCRIPTION_OPTIONS_H

#include "google/cloud/pubsub/version.h"
#include <chrono>

namespace google {
namespace cloud {
namespace pubsub {
inline namespace GOOGLE_CLOUD_CPP_PUBSUB_NS {

/**
 * Configure how a subscription handles incoming messages.
 */
class SubscriptionOptions {
 public:
  SubscriptionOptions() = default;

  /**
   * The maximum deadline for each incoming message.
   *
   * Configure how long does the application have to respond (ACK or NACK) an
   * incoming message. Note that this might be longer, or shorter, than the
   * deadline configured in the server-side subcription.
   *
   * The value `0` is reserved to leave the deadline unmodified and just use the
   * server-side configuration.
   *
   * @note The deadline applies to each message as it is delivered to the
   *     application, thus, if the library receives a batch of N messages their
   *     deadline for all the messages is extended repeatedly. Only once the
   *     message is delivered to a callback does the deadline become immutable.
   */
  std::chrono::seconds max_deadline_time() const { return max_deadline_time_; }

  /// Set the maximum deadline for incoming messages.
  SubscriptionOptions& set_max_deadline_time(std::chrono::seconds d) {
    max_deadline_time_ = d;
    return *this;
  }

  // TODO(#4645) - add options for flow control

 private:
  std::chrono::seconds max_deadline_time_ = std::chrono::seconds(0);
};

}  // namespace GOOGLE_CLOUD_CPP_PUBSUB_NS
}  // namespace pubsub
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_SUBSCRIPTION_OPTIONS_H
