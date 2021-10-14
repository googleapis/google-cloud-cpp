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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_SUBSCRIBER_OPTIONS_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_SUBSCRIBER_OPTIONS_H

#include "google/cloud/pubsub/options.h"
#include "google/cloud/pubsub/version.h"
#include "google/cloud/options.h"
#include <chrono>
#include <thread>

namespace google {
namespace cloud {
namespace pubsub {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
class SubscriberOptions;
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace pubsub

namespace pubsub_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
Options MakeOptions(pubsub::SubscriberOptions);
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace pubsub_internal

namespace pubsub {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

/**
 * Configure how a `Subscriber` handles incoming messages.
 *
 * There are two main algorithms controlled by this function: the dispatching of
 * application callbacks, and requesting more data from the service.
 *
 * @par Callback Concurrency Control
 * @parblock
 * The subscription configuration determines the upper limit (set
 * `set_concurrency_watermarks()`) how many callbacks are *scheduled* at a time.
 * As long as this limit is not reached the library will continue to schedule
 * callbacks, once the limit is reached the library will wait until the number
 * of executing callbacks goes below the low watermark.
 *
 * A callback is "executing" until the `AckHandler::ack()` or
 * `AckHandler::nack()` function is called on the associated `AckHandler`.
 * Applications can use this to move long-running computations out of the
 * library internal thread pool.
 *
 * Note that callbacks are "scheduled", but they may not immediately execute.
 * For example, callbacks may be sequenced if the concurrency control parameters
 * are higher than the number of I/O threads configured in the
 * `SubscriberConnection`.
 *
 * The default value for the concurrency high watermarks is set to the value
 * returned by `std::thread::hardware_concurrency()` (or 4 if your standard
 * library returns `0` for this parameter).
 * @endparblock
 *
 * @par Message Flow Control
 * @parblock
 * The subscription will request more messages from the service as long as
 * both the outstanding message count (see `set_message_count_watermarks()`)
 * and the number of bytes in the outstanding messages (see
 * `set_message_size_watermarks()`) are below the high watermarks for these
 * values.
 *
 * Once either of the high watermarks are breached the library will wait until
 * **both** the values are below their low watermarks before requesting more
 * messages from the service.
 *
 * In this algorithm a message is outstanding until the `AckHandler::ack()` or
 * `AckHandler::nack()` function is called on the associated `AckHandler`. Note
 * that if the concurrency control algorithm has not scheduled a callback this
 * can also put back pressure on the flow control algorithm.
 * @endparblock
 *
 * @par Example: setting the concurrency control parameters
 * @snippet samples.cc subscriber-concurrency
 *
 * @par Example: setting the flow control parameters
 * @snippet samples.cc subscriber-flow-control
 */
class SubscriberOptions {
 public:
  SubscriberOptions() : SubscriberOptions(Options{}) {}

  /**
   * Initialize the subscriber options.
   *
   * Expected options are any of the types in the `SubscriberOptionList`
   *
   * @note Unrecognized options will be ignored. To debug issues with options
   *     set `GOOGLE_CLOUD_CPP_ENABLE_CLOG=yes` in the environment and
   *     unexpected options will be logged.
   *
   * @param opts configuration options
   */
  explicit SubscriberOptions(Options opts);

  /**
   * The maximum deadline for each incoming message.
   *
   * Configure how long does the application have to respond (ACK or NACK) an
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
  std::chrono::seconds max_deadline_time() const {
    return opts_.get<MaxDeadlineTimeOption>();
  }

  /// Set the maximum deadline for incoming messages.
  SubscriberOptions& set_max_deadline_time(std::chrono::seconds d) {
    opts_.set<MaxDeadlineTimeOption>(d);
    return *this;
  }

  /**
   * Set the maximum time by which the deadline for each incoming message is
   * extended.
   *
   * The Cloud Pub/Sub C++ client library will extend the deadline by at most
   * this amount, while waiting for an ack or nack. The default extension is 10
   * minutes. An application may wish to reduce this extension so that the
   * Pub/Sub service will resend a message sooner when it does not hear back
   * from a Subscriber.
   *
   * The value is clamped between 10 seconds and 10 minutes.
   *
   * @param extension the maximum time that the deadline can be extended by,
   * measured in seconds.
   */
  SubscriberOptions& set_max_deadline_extension(std::chrono::seconds extension);
  std::chrono::seconds max_deadline_extension() const {
    return opts_.get<MaxDeadlineExtensionOption>();
  }

  /**
   * Set the maximum number of outstanding messages per streaming pull.
   *
   * The Cloud Pub/Sub C++ client library uses streaming pull requests to
   * receive messages from the service. The service will stop delivering
   * messages if @p message_count or more messages have not been acknowledged
   * nor rejected.
   *
   * @par Example
   * @snippet samples.cc subscriber-flow-control
   *
   * @param message_count the maximum number of messages outstanding, use 0 or
   *     negative numbers to make the message count unlimited.
   */
  SubscriberOptions& set_max_outstanding_messages(std::int64_t message_count);
  std::int64_t max_outstanding_messages() const {
    return opts_.get<MaxOutstandingMessagesOption>();
  }

  /**
   * Set the maximum number of outstanding bytes per streaming pull.
   *
   * The Cloud Pub/Sub C++ client library uses streaming pull requests to
   * receive messages from the service. The service will stop delivering
   * messages if @p bytes or more worth of messages have not been
   * acknowledged nor rejected.
   *
   * @par Example
   * @snippet samples.cc subscriber-flow-control
   *
   * @param bytes the maximum number of bytes outstanding, use 0 or negative
   *     numbers to make the number of bytes unlimited.
   */
  SubscriberOptions& set_max_outstanding_bytes(std::int64_t bytes);
  std::int64_t max_outstanding_bytes() const {
    return opts_.get<MaxOutstandingBytesOption>();
  }

  /**
   * Set the maximum callback concurrency.
   *
   * The Cloud Pub/Sub C++ client library will schedule parallel callbacks as
   * long as the number of outstanding callbacks is less than this maximum.
   *
   * Note that this controls the number of callbacks *scheduled*, not the number
   * of callbacks actually executing at a time. The application needs to create
   * (or configure) the background threads pool with enough parallelism to
   * execute more than one callback at a time.
   *
   * Some applications may want to share a thread pool across many
   * subscriptions, the additional level of control (scheduled vs. running
   * callbacks) allows applications, for example, to ensure that at most `K`
   * threads in the pool are used by any given subscription.
   *
   * @par Example
   * @snippet samples.cc subscriber-concurrency
   *
   * @param v the new value, 0 resets to the default
   */
  SubscriberOptions& set_max_concurrency(std::size_t v);

  /// Maximum number of callbacks scheduled by the library at a time.
  std::size_t max_concurrency() const {
    return opts_.get<MaxConcurrencyOption>();
  }

  /**
   * Control how often the session polls for automatic shutdowns.
   *
   * Applications can shutdown a session by calling `.cancel()` on the returned
   * `future<Status>`.  In addition, applications can fire & forget a session,
   * which is only shutdown once the completion queue servicing the session
   * shuts down. In this latter case the session polls periodically to detect
   * if the CQ has shutdown. This controls how often this polling happens.
   */
  SubscriberOptions& set_shutdown_polling_period(std::chrono::milliseconds v) {
    opts_.set<ShutdownPollingPeriodOption>(v);
    return *this;
  }
  std::chrono::milliseconds shutdown_polling_period() const {
    return opts_.get<ShutdownPollingPeriodOption>();
  }

 private:
  friend Options pubsub_internal::MakeOptions(SubscriberOptions);
  Options opts_;
};

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace pubsub

namespace pubsub_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

inline Options MakeOptions(pubsub::SubscriberOptions o) {
  return std::move(o.opts_);
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace pubsub_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_SUBSCRIBER_OPTIONS_H
