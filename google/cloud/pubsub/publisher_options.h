// Copyright 2020 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_PUBLISHER_OPTIONS_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_PUBLISHER_OPTIONS_H

#include "google/cloud/pubsub/options.h"
#include "google/cloud/pubsub/version.h"
#include "google/cloud/options.h"
#include <algorithm>
#include <chrono>
#include <cstddef>
#include <limits>

namespace google {
namespace cloud {
namespace pubsub {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
class PublisherOptions;
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace pubsub

namespace pubsub_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
Options MakeOptions(pubsub::PublisherOptions);
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace pubsub_internal

namespace pubsub {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

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
 * @deprecated We recommend you use `google::cloud::Options` and pass the
 *   options to `MakePublisherConnection()`.
 *
 * @par Example
 * @snippet samples.cc publisher-options
 *
 * [pubsub-pricing-link]: https://cloud.google.com/pubsub/pricing
 */
class PublisherOptions {
 public:
  /// @deprecated Use `google::cloud::Options{}` instead.
  GOOGLE_CLOUD_CPP_DEPRECATED("use `google::cloud::Options{}` instead")
  PublisherOptions();

  /**
   * Initialize the publisher options.
   *
   * Expected options are any of the types in the `PublisherOptionList`
   *
   * @note Unrecognized options will be ignored. To debug issues with options
   *     set `GOOGLE_CLOUD_CPP_ENABLE_CLOG=yes` in the environment and
   *     unexpected options will be logged.
   *
   * @param opts configuration options
   *
   * @deprecated Use `google::cloud::Options{}` instead.
   */
  GOOGLE_CLOUD_CPP_DEPRECATED("use `google::cloud::Options{}` instead")
  explicit PublisherOptions(Options opts);

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
  ///@{
  /// @deprecated Use `google::cloud::Options{}` and `MaxHoldTimeOption`
  ///     instead.
  GOOGLE_CLOUD_CPP_DEPRECATED(
      "use `google::cloud::Options{}` and `MaxHoldTimeOption` instead")
  std::chrono::microseconds maximum_hold_time() const {
    return opts_.get<MaxHoldTimeOption>();
  }

  /**
   * Sets the maximum hold time for the messages.
   *
   * @note While this function accepts durations in arbitrary precision, the
   *     implementation depends on the granularity of your OS timers. It is
   *     possible that messages are held for slightly longer times than the
   *     value set here.
   *
   * @note The first message in a batch starts the hold time counter. New
   *     messages do not extend the life of the batch. For example, if you have
   *     set the holding time to 10 milliseconds, start a batch with message 1,
   *     and publish a second message 5 milliseconds later, the second message
   *     will be flushed approximately 5 milliseconds after it is published.
   *
   * @deprecated Use `google::cloud::Options{}` and `MaxHoldTimeOption` instead.
   */
  template <typename Rep, typename Period>
  GOOGLE_CLOUD_CPP_DEPRECATED(
      "use `google::cloud::Options{}` and `MaxHoldTimeOption` instead")
  PublisherOptions& set_maximum_hold_time(
      std::chrono::duration<Rep, Period> v) {
    opts_.set<MaxHoldTimeOption>(
        std::chrono::duration_cast<std::chrono::microseconds>(v));
    return *this;
  }

  /// @deprecated Use `google::cloud::Options{}` and `MaxBatchMessagesOption`
  /// instead.
  GOOGLE_CLOUD_CPP_DEPRECATED(
      "use `google::cloud::Options{}` and `MaxBatchMessagesOption` instead")
  std::size_t maximum_batch_message_count() const {
    return opts_.get<MaxBatchMessagesOption>();
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
   *
   * @deprecated Use `google::cloud::Options{}` and `MaxBatchMessagesOption`
   *     instead.
   */
  GOOGLE_CLOUD_CPP_DEPRECATED(
      "use `google::cloud::Options{}` and `MaxBatchMessagesOption` instead")
  PublisherOptions& set_maximum_batch_message_count(std::size_t v) {
    opts_.set<MaxBatchMessagesOption>(v);
    return *this;
  }

  /// @deprecated Use `google::cloud::Options{}` and `MaxBatchBytesOption`
  ///     instead.
  GOOGLE_CLOUD_CPP_DEPRECATED(
      "use `google::cloud::Options{}` and `MaxBatchBytesOption` instead")
  std::size_t maximum_batch_bytes() const {
    return opts_.get<MaxBatchBytesOption>();
  }

  /**
   * Set the maximum size for the messages in a batch.
   *
   * @note Application developers should keep in mind that Cloud Pub/Sub
   *    sets [limits][pubsub-quota-link] on the size of a batch (10MB). The
   *    library makes no attempt to validate the value provided in this
   *    function.
   *
   * [pubsub-quota-link]: https://cloud.google.com/pubsub/quotas#resource_limits
   *
   * @deprecated Use `google::cloud::Options{}` and `MaxBatchBytesOption`
   *     instead.
   */
  GOOGLE_CLOUD_CPP_DEPRECATED(
      "use `google::cloud::Options{}` and `MaxBatchBytesOption` instead")
  PublisherOptions& set_maximum_batch_bytes(std::size_t v) {
    opts_.set<MaxBatchBytesOption>(v);
    return *this;
  }
  ///@}

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
  ///@{

  /**
   * Return `true` if message ordering is enabled.
   *
   * @deprecated Use `google::cloud::Options{}` and `MessageOrderingOption`
   *     instead.
   */
  GOOGLE_CLOUD_CPP_DEPRECATED(
      "use `google::cloud::Options{}` and `MessageOrderingOption` instead")
  bool message_ordering() const { return opts_.get<MessageOrderingOption>(); }

  /**
   * Enable message ordering.
   *
   * @deprecated Use `google::cloud::Options{}` and `MessageOrderingOption`
   *     instead.
   *
   * @see the documentation for the `Publisher` class for details.
   */
  GOOGLE_CLOUD_CPP_DEPRECATED(
      "use `google::cloud::Options{}` and `MessageOrderingOption` instead")
  PublisherOptions& enable_message_ordering() {
    opts_.set<MessageOrderingOption>(true);
    return *this;
  }

  /**
   * Disable message ordering.
   *
   * @deprecated Use `google::cloud::Options{}` and `MessageOrderingOption`
   *     instead.
   *
   * @see the documentation for the `Publisher` class for details.
   */
  GOOGLE_CLOUD_CPP_DEPRECATED(
      "use `google::cloud::Options{}` and `MessageOrderingOption` instead")
  PublisherOptions& disable_message_ordering() {
    opts_.set<MessageOrderingOption>(false);
    return *this;
  }
  ///@}

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
  ///@{

  /**
   * Flow control based on pending bytes.
   *
   * @deprecated Use `google::cloud::Options{}` and `MaxPendingBytesOption`
   *     instead.
   */
  GOOGLE_CLOUD_CPP_DEPRECATED(
      "use `google::cloud::Options{}` and `MaxPendingBytesOption` instead")
  PublisherOptions& set_maximum_pending_bytes(std::size_t v) {
    opts_.set<MaxPendingBytesOption>(v);
    return *this;
  }

  /**
   * Flow control based on pending messages.
   *
   * @deprecated Use `google::cloud::Options{}` and `MaxPendingMessagesOption`
   *     instead.
   */
  GOOGLE_CLOUD_CPP_DEPRECATED(
      "use `google::cloud::Options{}` and `MaxPendingMessagesOption` instead")
  PublisherOptions& set_maximum_pending_messages(std::size_t v) {
    opts_.set<MaxPendingMessagesOption>(v);
    return *this;
  }

  /// @deprecated Use `google::cloud::Options{}` and `MaxPendingBytesOption`
  /// instead.
  GOOGLE_CLOUD_CPP_DEPRECATED(
      "use `google::cloud::Options{}` and `MaxPendingBytesOption` instead")
  std::size_t maximum_pending_bytes() const {
    return opts_.get<MaxPendingBytesOption>();
  }

  /// @deprecated Use `google::cloud::Options{}` and `MaxPendingMessagesOption`
  /// instead.
  GOOGLE_CLOUD_CPP_DEPRECATED(
      "use `google::cloud::Options{}` and `MaxPendingMessagesOption` instead")
  std::size_t maximum_pending_messages() const {
    return opts_.get<MaxPendingMessagesOption>();
  }

  /**
   * The current action for a full publisher.
   *
   * @deprecated Use `google::cloud::Options{}` and `FullPublisherActionOption`
   *     instead.
   */
  GOOGLE_CLOUD_CPP_DEPRECATED(
      "use `google::cloud::Options{}` and `FullPublisherActionOption` instead")
  bool full_publisher_ignored() const {
    return opts_.get<FullPublisherActionOption>() ==
           FullPublisherAction::kIgnored;
  }

  /**
   * The current action for a full publisher.
   *
   * @deprecated Use `google::cloud::Options{}` and `FullPublisherActionOption`
   *     instead.
   */
  GOOGLE_CLOUD_CPP_DEPRECATED(
      "use `google::cloud::Options{}` and `FullPublisherActionOption` instead")
  bool full_publisher_rejects() const {
    return opts_.get<FullPublisherActionOption>() ==
           FullPublisherAction::kRejects;
  }

  /**
   * The current action for a full publisher.
   *
   * @deprecated Use `google::cloud::Options{}` and `FullPublisherActionOption`
   *     instead.
   */
  GOOGLE_CLOUD_CPP_DEPRECATED(
      "use `google::cloud::Options{}` and `FullPublisherActionOption` instead")
  bool full_publisher_blocks() const {
    return opts_.get<FullPublisherActionOption>() ==
           FullPublisherAction::kBlocks;
  }

  /**
   * Ignore full publishers, continue as usual
   *
   * @deprecated Use `google::cloud::Options{}` and `FullPublisherActionOption`
   *     instead.
   */
  GOOGLE_CLOUD_CPP_DEPRECATED(
      "use `google::cloud::Options{}` and `FullPublisherActionOption` instead")
  PublisherOptions& set_full_publisher_ignored() {
    opts_.set<FullPublisherActionOption>(FullPublisherAction::kIgnored);
    return *this;
  }

  /**
   * Configure the publisher to reject new messages when full.
   *
   * @deprecated Use `google::cloud::Options{}` and `FullPublisherActionOption`
   *     instead.
   */
  GOOGLE_CLOUD_CPP_DEPRECATED(
      "use `google::cloud::Options{}` and `FullPublisherActionOption` instead")
  PublisherOptions& set_full_publisher_rejects() {
    opts_.set<FullPublisherActionOption>(FullPublisherAction::kRejects);
    return *this;
  }

  /**
   * Configure the publisher to block the caller when full.
   *
   * @deprecated Use `google::cloud::Options{}` and `FullPublisherActionOption`
   *     instead.
   */
  GOOGLE_CLOUD_CPP_DEPRECATED(
      "use `google::cloud::Options{}` and `FullPublisherActionOption` instead")
  PublisherOptions& set_full_publisher_blocks() {
    opts_.set<FullPublisherActionOption>(FullPublisherAction::kBlocks);
    return *this;
  }
  ///@}

 private:
  friend Options pubsub_internal::MakeOptions(PublisherOptions);
  Options opts_;
};

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace pubsub

namespace pubsub_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

inline Options MakeOptions(pubsub::PublisherOptions o) {
  return std::move(o.opts_);
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace pubsub_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_PUBLISHER_OPTIONS_H
