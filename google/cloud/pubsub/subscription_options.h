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

  /**
   * Set the high watermark and low watermarks for callback concurrency.
   *
   * The Cloud Pub/Sub C++ client library will schedule parallel callbacks as
   * long as the number of outstanding callbacks is less than the high
   * watermark. Once the watermark is reached the client will not resume
   * scheduling callbacks until the number of outstanding callbacks is at or
   * below the low watermark. Using hysteresis prevents instability.
   *
   * Note that this controls the number of callbacks *scheduled*, not the number
   * of callbacks actually executing at a time. The application needs to create
   * (or configure) the background threads pool with enough parallelism to
   * execute more than one callback at a time.
   *
   * Some applications many want to share a thread pool across many
   * subscriptions, the additional level of control (scheduled vs. running
   * callbacks) allows applications, for example, to ensure that at most `K`
   * threads in the pool are used by any given subscription.
   *
   * @par Example
   * @snippet samples.cc subscriber-concurrency
   *
   * @note applications that want to have a single outstanding callback can set
   *     these parameters to `lwm==0` and `hwm==1`.
   *
   * @param hwm the high watermark, if this parameter is `0` the high watermark
   *     is set to `1`, to avoid starvation for the callbacks
   * @param lwm the low watermark, if this parameter greater than @p hwm then
   *    the low watermark is set to the same value as the high watermark
   */
  SubscriptionOptions& set_concurrency_watermarks(std::size_t lwm,
                                                  std::size_t hwm);
  std::size_t concurrency_lwm() const { return concurrency_lwm_; }
  std::size_t concurrency_hwm() const { return concurrency_hwm_; }

 private:
  std::chrono::seconds max_deadline_time_ = std::chrono::seconds(0);
  std::size_t concurrency_lwm_ = 0;
  std::size_t concurrency_hwm_ = 1;
};

}  // namespace GOOGLE_CLOUD_CPP_PUBSUB_NS
}  // namespace pubsub
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_SUBSCRIPTION_OPTIONS_H
