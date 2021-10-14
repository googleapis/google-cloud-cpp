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

#include "google/cloud/pubsub/backoff_policy.h"
#include "google/cloud/pubsub/retry_policy.h"
#include "google/cloud/pubsub/version.h"
#include "google/cloud/options.h"
#include <chrono>

namespace google {
namespace cloud {
namespace pubsub {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

/// The retry policy
struct RetryPolicyOption {
  using Type = std::shared_ptr<pubsub::RetryPolicy>;
};

/// The backoff policy
struct BackoffPolicyOption {
  using Type = std::shared_ptr<pubsub::BackoffPolicy>;
};

/// The list of all "policy" options.
using PolicyOptionList = OptionList<RetryPolicyOption, BackoffPolicyOption>;

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
struct MaxHoldTimeOption {
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
struct MaxBatchMessagesOption {
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
struct MaxBatchBytesOption {
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
struct MaxPendingMessagesOption {
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
struct MaxPendingBytesOption {
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

/// The list of options specific to publishers.
using PublisherOptionList =
    OptionList<MaxHoldTimeOption, MaxBatchMessagesOption, MaxBatchBytesOption,
               MaxPendingMessagesOption, MaxPendingBytesOption,
               MessageOrderingOption, FullPublisherActionOption>;

/**
 * The maximum deadline for each incoming message.
 *
 * Configure how long the application has to respond (ACK or NACK) to an
 * incoming message. Note that this might be longer, or shorter, than the
 * deadline configured in the server-side subscription.
 *
 * The value `0` is reserved to leave the deadline unmodified and just use the
 * server-side configuration.
 *
 * @note The deadline applies to each message as it is delivered to the
 *     application, thus, if the library receives a batch of N messages their
 *     deadline for all the messages is extended repeatedly. Only once the
 *     message is delivered to a callback does the deadline become immutable.
 */
struct MaxDeadlineTimeOption {
  using Type = std::chrono::seconds;
};

/**
 * The maximum time by which the deadline for each incoming message is extended.
 *
 * The Cloud Pub/Sub C++ client library will extend the deadline by at most this
 * amount, while waiting for an ack or nack. The default extension is 10
 * minutes. An application may wish to reduce this extension so that the Pub/Sub
 * service will resend a message sooner when it does not hear back from a
 * Subscriber.
 *
 * The value is clamped between 10 seconds and 10 minutes.
 */
struct MaxDeadlineExtensionOption {
  using Type = std::chrono::seconds;
};

/**
 * The maximum number of outstanding messages per streaming pull.
 *
 * The Cloud Pub/Sub C++ client library uses streaming pull requests to receive
 * messages from the service. The service will stop delivering messages if this
 * many messages or more have neither been acknowledged nor rejected.
 *
 * If a negative or 0 value is supplied, the number of messages will be
 * unlimited.
 *
 * @par Example
 * @snippet samples.cc subscriber-flow-control
 */
struct MaxOutstandingMessagesOption {
  using Type = std::int64_t;
};

/**
 * The maximum number of outstanding bytes per streaming pull.
 *
 * The Cloud Pub/Sub C++ client library uses streaming pull requests to receive
 * messages from the service. The service will stop delivering messages if this
 * many bytes or more worth of messages have not been acknowledged nor rejected.
 *
 * If a negative or 0 value is supplied, the number of bytes will be unlimited.
 *
 * @par Example
 * @snippet samples.cc subscriber-flow-control
 */
struct MaxOutstandingBytesOption {
  using Type = std::int64_t;
};

/**
 * The maximum callback concurrency.
 *
 * The Cloud Pub/Sub C++ client library will schedule parallel callbacks as long
 * as the number of outstanding callbacks is less than this maximum.
 *
 * Note that this controls the number of callbacks *scheduled*, not the number
 * of callbacks actually executing at a time. The application needs to create
 * (or configure) the background threads pool with enough parallelism to execute
 * more than one callback at a time.
 *
 * Some applications may want to share a thread pool across many subscriptions.
 * The additional level of control (scheduled vs. running callbacks) allows
 * applications, for example, to ensure that at most `K` threads in the pool are
 * used by any given subscription.
 *
 * @par Example
 * @snippet samples.cc subscriber-concurrency
 */
struct MaxConcurrencyOption {
  using Type = std::size_t;
};

/**
 * How often the session polls for automatic shutdowns.
 *
 * Applications can shutdown a session by calling `.cancel()` on the returned
 * `future<Status>`.  In addition, applications can fire & forget a session,
 * which is only shutdown once the completion queue servicing the session shuts
 * down. In this latter case the session polls periodically to detect if the CQ
 * has shutdown. This controls how often this polling happens.
 */
struct ShutdownPollingPeriodOption {
  using Type = std::chrono::milliseconds;
};

/// The list of options specific to subscribers.
using SubscriberOptionList =
    OptionList<MaxDeadlineTimeOption, MaxDeadlineExtensionOption,
               MaxOutstandingMessagesOption, MaxOutstandingBytesOption,
               MaxConcurrencyOption, ShutdownPollingPeriodOption>;

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace pubsub
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_OPTIONS_H
