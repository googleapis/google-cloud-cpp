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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_PUBLISHER_OPTIONS_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_PUBLISHER_OPTIONS_H

#include "google/cloud/pubsub/version.h"
#include <chrono>
#include <cstddef>

namespace google {
namespace cloud {
namespace pubsub {
inline namespace GOOGLE_CLOUD_CPP_PUBSUB_NS {

/**
 * Configure batching for a `Publisher` object.
 *
 * By default, the client library does not automatically batch messages, all
 * messages have a maximum holding time of 0.
 *
 * @warning TODO(#4808) - we are planning to change this default.
 *
 * @note Applications developers may want to consider batching messages,
 *     specially if their messages have small payloads. Consult the Cloud
 *     Pub/Sub [pricing page][pubsub-pricing-link] for details.
 *
 * @par Example
 * @snippet samples.cc publisher-options
 *
 * [pubsub-pricing-link]: https://cloud.google.com/pubsub/pricing
 */
class BatchingConfig {
 public:
  BatchingConfig();

  /// The maximum hold time.
  std::chrono::microseconds maximum_hold_time() const {
    return maximum_hold_time_;
  }

  /**
   * Sets the maximum hold time for the messages.
   *
   * @note while this function accepts durations in arbitrary precision, the
   *     implementation depends on the granularity of your OS timers. It is
   *     possible that messages are held for slightly longer times than the
   *     value set here.
   *
   * @note the first message in a batch starts the hold time counter. New
   *     messages do not extend the life of the batch. For example, if you have
   *     set the holding time to 10 milliseconds, start a batch with message 1,
   *     and publish a second message 5 milliseconds later, the second message
   *     will be flushed approximately 5 milliseconds after it is published.
   */
  template <typename Rep, typename Period>
  BatchingConfig& set_maximum_hold_time(std::chrono::duration<Rep, Period> v) {
    maximum_hold_time_ =
        std::chrono::duration_cast<std::chrono::microseconds>(v);
    return *this;
  }

  std::size_t maximum_message_count() const { return maximum_message_count_; }
  BatchingConfig& set_maximum_message_count(std::size_t v) {
    maximum_message_count_ = v;
    return *this;
  }

  std::size_t maximum_batch_bytes() const { return maximum_batch_bytes_; }
  BatchingConfig& set_maximum_batch_bytes(std::size_t v) {
    maximum_batch_bytes_ = v;
    return *this;
  }

 private:
  std::chrono::microseconds maximum_hold_time_;
  std::size_t maximum_message_count_;
  std::size_t maximum_batch_bytes_;
};

/**
 * Configuration options for a `Publisher`
 *
 * @par Example
 * @snippet samples.cc publisher-options
 */
class PublisherOptions {
 public:
  PublisherOptions() = default;

  BatchingConfig const& batching_config() const { return batching_config_; }
  PublisherOptions& set_batching_config(BatchingConfig v) {
    batching_config_ = std::move(v);
    return *this;
  }

  bool message_ordering() const { return message_ordering_; }
  PublisherOptions& enable_message_ordering() {
    message_ordering_ = true;
    return *this;
  }
  PublisherOptions& disable_message_ordering() {
    message_ordering_ = false;
    return *this;
  }

 private:
  BatchingConfig batching_config_;
  bool message_ordering_ = false;
};

}  // namespace GOOGLE_CLOUD_CPP_PUBSUB_NS
}  // namespace pubsub
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_PUBLISHER_OPTIONS_H
