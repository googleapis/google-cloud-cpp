// Copyright 2021 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_OPTIONS_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_OPTIONS_H

/**
 * @file
 *
 * This file defines options to be used with instances of
 * `google::cloud::Options`. By convention options are named with an "Option"
 * suffix. As the name would imply, all options are optional, and leaving them
 * unset will result in a reasonable default being chosen.
 *
 * Not all options are meaningful to all functions that accept a
 * `google::cloud::Options` instance. Each function that accepts a
 * `google::cloud::Options` should document which options it expects. This is
 * typically done by indicating lists of options using "OptionList" aliases.
 * For example, a function may indicate that users may set any option in
 * `PublisherOptionList`.
 *
 * @note Unrecognized options are allowed and will be ignored. To debug issues
 *     with options set `GOOGLE_CLOUD_CPP_ENABLE_CLOG=yes` in the environment
 *     and unexpected options will be logged.
 *
 * @see `google::cloud::CommonOptionList`
 * @see `google::cloud::GrpcOptionList`
 */

#include "google/cloud/pubsub/version.h"
#include "google/cloud/options.h"
#include <chrono>

namespace google {
namespace cloud {
namespace pubsub {
inline namespace GOOGLE_CLOUD_CPP_PUBSUB_NS {

/**
 * The maximum hold time for the messages.
 *
 * @note The implementation depends on the granularity of your OS timers. It is
 *     possible that messages are held for slightly longer times than the value
 *     set here.
 *
 * @note The first message in a batch starts the hold time counter. New
 *     messages do not extend the life of the batch. For example, if you have
 *     set the holding time to 10 milliseconds, start a batch with message 1,
 *     and publish a second message 5 milliseconds later, the second message
 *     will be flushed approximately 5 milliseconds after it is published.
 */
struct MaximumHoldTimeOption {
  using Type = std::chrono::microseconds;
};

/**
 * The maximum number of messages in a batch.
 *
 * @note Application developers should keep in mind that Cloud Pub/Sub
 *    sets [limits][pubsub-quota-link] on the size of a batch (1,000 messages)
 *    The library makes no attempt to validate the value provided in this
 *    option.
 *
 * [pubsub-quota-link]: https://cloud.google.com/pubsub/quotas#resource_limits
 */
struct MaximumBatchMessagesOption {
  using Type = std::size_t;
};

/**
 * The maximum size for the messages in a batch.
 *
 * @note Application developers should keep in mind that Cloud Pub/Sub
 *    sets [limits][pubsub-quota-link] on the size of a batch (10MB). The
 *    library makes no attempt to validate the value provided in this
 *    option.
 *
 * [pubsub-quota-link]: https://cloud.google.com/pubsub/quotas#resource_limits
 */
struct MaximumBatchBytesOption {
  using Type = std::size_t;
};

/**
 * The maximum number of pending messages.
 *
 * After a publisher flushes a batch of messages the batch is (obviously) not
 * received immediately by the service. While the batch remains pending it
 * potentially consumes memory resources in the client (and/or the service).
 *
 * Some applications may have constraints on the number of bytes and/or
 * messages they can tolerate in this pending state, and may prefer to block
 * or reject messages.
 */
struct MaximumPendingMessagesOption {
  using Type = std::size_t;
};

/**
 * The maximum size for pending messages.
 *
 * After a publisher flushes a batch of messages the batch is (obviously) not
 * received immediately by the service. While the batch remains pending it
 * potentially consumes memory resources in the client (and/or the service).
 *
 * Some applications may have constraints on the number of bytes and/or
 * messages they can tolerate in this pending state, and may prefer to block
 * or reject messages.
 */
struct MaximumPendingBytesOption {
  using Type = std::size_t;
};

/**
 * Publisher message ordering.
 *
 * To guarantee messages are received by the service in the same order that
 * the application gives them to a publisher, the client library needs to wait
 * until a batch of messages is successfully delivered before sending the next
 * batch, otherwise batches may arrive out of order as there is no guarantee
 * the same channel or network path is used for each batch.
 *
 * For applications that do not care about message ordering, this can limit
 * the throughput. Therefore, the behavior is disabled by default.
 *
 * @see the documentation for the `Publisher` class for details.
 */
struct MessageOrderingOption {
  using Type = bool;
};

/// Actions taken by a full publisher.
enum class FullPublisherAction {
  /// Ignore full publishers, continue as usual.
  kIgnored,
  /// Configure the publisher to reject new messages when full.
  kRejects,
  /// Configure the publisher to block the caller when full.
  kBlocks
};
/// The action taken by a full publisher.
struct FullPublisherActionOption {
  using Type = FullPublisherAction;
};

/// The list of options specific to publishers
using PublisherOptionList =
    OptionList<MaximumHoldTimeOption, MaximumBatchMessagesOption,
               MaximumBatchBytesOption, MaximumPendingMessagesOption,
               MaximumPendingBytesOption, MessageOrderingOption,
               FullPublisherActionOption>;

}  // namespace GOOGLE_CLOUD_CPP_PUBSUB_NS
}  // namespace pubsub
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_OPTIONS_H
