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
#include <algorithm>
#include <chrono>
#include <cstddef>
#include <limits>

namespace google {
namespace cloud {
namespace pubsub {
inline namespace GOOGLE_CLOUD_CPP_PUBSUB_NS {

/**
 * Configuration options for a `Publisher`.
 *
 * These are largely used to control the batching configuration for a publisher.
 * By default a publishers flushes a batch as soon as any of these conditions
 * are met:
 * - 10ms after the first message is put in the batch,
 * - when the batch contains 100 messages or more,
 * - when the batch contains 1MiB of data or more.
 *
 * Applications developers should consult the Cloud Pub/Sub
 * [pricing page][pubsub-pricing-link] when selecting a batching configuration.
 *
 * @par Example
 * @snippet samples.cc publisher-options
 *
 * [pubsub-pricing-link]: https://cloud.google.com/pubsub/pricing
 */
class PublisherOptions {
 public:
  PublisherOptions() = default;

  //@{
  /**
   * @name Publisher batch control
   *
   * It is more efficient (in terms of client CPU and client network usage) to
   * send multiple messages in a single "batch" to the service. The following
   * configuration options can be used to improve throughput: sending larger
   * batches reduces CPU and network overhead. Note that batches are subject
   * to [quota limits].
   *
   * [quota limits]: https://cloud.google.com/pubsub/quotas#resource_limits
   */
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
  PublisherOptions& set_maximum_hold_time(
      std::chrono::duration<Rep, Period> v) {
    maximum_hold_time_ =
        std::chrono::duration_cast<std::chrono::microseconds>(v);
    return *this;
  }

  std::size_t maximum_batch_message_count() const {
    return maximum_batch_message_count_;
  }

  /**
   * Set the maximum number of messages in a batch.
   *
   * @note Application developers should keep in mind that Cloud Pub/Sub
   *    sets [limits][pubsub-quota-link] on the size of a batch (1,000 messages)
   *    The library makes no attempt to validate the value provided in this
   *    function.
   *
   * [pubsub-quota-link]: https://cloud.google.com/pubsub/quotas#resource_limits
   */
  PublisherOptions& set_maximum_batch_message_count(std::size_t v) {
    maximum_batch_message_count_ = v;
    return *this;
  }

  std::size_t maximum_batch_bytes() const { return maximum_batch_bytes_; }

  /**
   * Set the maximum size for the messages in a batch.
   *
   * @note Application developers should keep in mind that Cloud Pub/Sub
   *    sets [limits][pubsub-quota-link] on the size of a batch (10MB). The
   *    library makes no attempt to validate the value provided in this
   *    function.
   *
   * [pubsub-quota-link]: https://cloud.google.com/pubsub/quotas#resource_limits
   */
  PublisherOptions& set_maximum_batch_bytes(std::size_t v) {
    maximum_batch_bytes_ = v;
    return *this;
  }
  //@}

  //@{
  /**
   * @name Publisher message ordering.
   *
   * To guarantee messages are received by the service in the same order that
   * the application gives them to a publisher, the client library needs to wait
   * until a batch of messages is successfully delivered before sending the next
   * batch, otherwise batches may arrive out of order as there is no guarantee
   * the same channel or network path is used for each batch.
   *
   * For applications that do not care about message ordering, this can limit
   * the throughput. Therefore, the behavior is disabled by default.
   */

  /// Return `true` if message ordering is enabled.
  bool message_ordering() const { return message_ordering_; }

  /**
   * Enable message ordering.
   *
   * @see the documentation for the `Publisher` class for details.
   */
  PublisherOptions& enable_message_ordering() {
    message_ordering_ = true;
    return *this;
  }

  /**
   * Disable message ordering.
   *
   * @see the documentation for the `Publisher` class for details.
   */
  PublisherOptions& disable_message_ordering() {
    message_ordering_ = false;
    return *this;
  }
  //@}

  //@{
  /**
   * @name Publisher flow control.
   *
   * After a publisher flushes a batch of messages the batch is (obviously) not
   * received immediately by the service. While the batch remains pending it
   * potentially consumes memory resources in the client (and/or the service).
   *
   * Some applications may have constraints on the number of bytes and/or
   * messages they can tolerate in this pending state, and may prefer to block
   * or reject messages.
   */

  /// Flow control based on pending bytes.
  PublisherOptions& set_maximum_pending_bytes(std::size_t v) {
    maximum_pending_bytes_ = v;
    return *this;
  }

  /// Flow control based on pending messages.
  PublisherOptions& set_maximum_pending_messages(std::size_t v) {
    maximum_pending_messages_ = v;
    return *this;
  }

  std::size_t maximum_pending_bytes() const { return maximum_pending_bytes_; }
  std::size_t maximum_pending_messages() const {
    return maximum_pending_messages_;
  }

  /// The current action for a full publisher
  bool full_publisher_ignored() const {
    return full_publisher_action_ == FullPublisherAction::kIgnored;
  }
  bool full_publisher_rejects() const {
    return full_publisher_action_ == FullPublisherAction::kRejects;
  }
  bool full_publisher_blocks() const {
    return full_publisher_action_ == FullPublisherAction::kBlocks;
  }

  /// Ignore full publishers, continue as usual
  PublisherOptions& set_full_publisher_ignored() {
    full_publisher_action_ = FullPublisherAction::kIgnored;
    return *this;
  }

  /// Configure the publisher to reject new messages when full.
  PublisherOptions& set_full_publisher_rejects() {
    full_publisher_action_ = FullPublisherAction::kRejects;
    return *this;
  }

  /// Configure the publisher to block the caller when full.
  PublisherOptions& set_full_publisher_blocks() {
    full_publisher_action_ = FullPublisherAction::kBlocks;
    return *this;
  }
  //@}

 private:
  static auto constexpr kDefaultMaximumHoldTime = std::chrono::milliseconds(10);
  static std::size_t constexpr kDefaultMaximumMessageCount = 100;
  static std::size_t constexpr kDefaultMaximumMessageSize = 1024 * 1024L;
  static std::size_t constexpr kDefaultMaximumPendingBytes =
      (std::numeric_limits<std::size_t>::max)();
  static std::size_t constexpr kDefaultMaximumPendingMessages =
      (std::numeric_limits<std::size_t>::max)();

  enum class FullPublisherAction { kIgnored, kRejects, kBlocks };

  std::chrono::microseconds maximum_hold_time_ = kDefaultMaximumHoldTime;
  std::size_t maximum_batch_message_count_ = kDefaultMaximumMessageCount;
  std::size_t maximum_batch_bytes_ = kDefaultMaximumMessageSize;
  bool message_ordering_ = false;
  std::size_t maximum_pending_bytes_ = kDefaultMaximumPendingBytes;
  std::size_t maximum_pending_messages_ = kDefaultMaximumPendingMessages;
  FullPublisherAction full_publisher_action_ = FullPublisherAction::kBlocks;
};

}  // namespace GOOGLE_CLOUD_CPP_PUBSUB_NS
}  // namespace pubsub
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_PUBLISHER_OPTIONS_H
