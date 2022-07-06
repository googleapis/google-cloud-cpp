// Copyright 2022 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_EXACTLY_ONCE_ACK_HANDLER_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_EXACTLY_ONCE_ACK_HANDLER_H

#include "google/cloud/pubsub/version.h"
#include "google/cloud/future.h"
#include "google/cloud/status.h"
#include <memory>

namespace google {
namespace cloud {
namespace pubsub {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

/**
 * Defines the interface to acknowledge and reject messages.
 *
 * When applications register a callback to receive Pub/Sub messages the
 * callback must be able to receive both a `pubsub::Message` and its associated
 * `pubsub::ExactlyOnceAckHandler`. Actions on a `pubsub::ExactlyOnceAckHandler`
 * always affect the same message received in the callback. Applications cannot
 * create standalone handlers (except in unit tests via mocks).
 *
 * This interface allows applications to acknowledge and reject messages that
 * are provided by the Cloud Pub/Sub C++ client library to the application. Note
 * that this class is move-able, to support applications that process messages
 * asynchronously. However, this class is *not* copy-able, because messages can
 * only be acknowledged or rejected exactly once.
 *
 * @par Thread Safety
 * This class is *thread compatible*, only one thread should call non-const
 * member functions of this class at a time. Note that because the non-const
 * member functions are `&&` overloads the application can only call `ack()` or
 * `nack()` exactly once, and only one of them.
 */
class ExactlyOnceAckHandler {
 public:
  ~ExactlyOnceAckHandler();

  ExactlyOnceAckHandler(ExactlyOnceAckHandler&&) = default;
  ExactlyOnceAckHandler& operator=(ExactlyOnceAckHandler&&) = default;

  /**
   * Acknowledges the message associated with this handler.
   *
   * @par Idempotency
   * If exactly-once is enabled in the subscription, the client library will
   * retry this operation in the background until it succeeds, fails with a
   * permanent error, or the ack id has become unusable (all ack ids are
   * unusable after 10 minutes). The returned future is satisfied when the retry
   * loop completes.
   *
   * If exactly-once is not enabled, the request is handled on a best-effort
   * basis.
   *
   * If the future is satisfied with an Okay `Status` **and** exactly-once
   * delivery is enabled in the subscription, then the message will not be
   * resent by Cloud Pub/Sub.  We remind the reader that Cloud Pub/Sub defaults
   * to "at least once" delivery, that is, without exactly-once delivery, the
   * message *may* be resent even after the future is satisfied with an Okay
   * `Status`.
   *
   * If the future is satisfied with an error, it is possible that Cloud Pub/Sub
   * never received the acknowledgement, and will resend the message.
   */
  future<Status> ack() && {
    auto impl = std::move(impl_);
    return impl->ack();
  }

  /**
   * Rejects the message associated with this handler.
   *
   * @par Idempotency
   * If exactly-once is enabled in the subscription, the client library will
   * retry this operation in the background until it succeeds, fails with a
   * permanent error, or the ack id has become unusable (all ack ids are
   * unusable after 10 minutes). The returned future is satisfied when the retry
   * loop completes.
   *
   * If exactly-once is not enabled, the request is handled on a best-effort
   * basis.
   *
   * In any case, Cloud Pub/Sub will eventually resend the message. It might do
   * so sooner if the operation succeeds.
   */
  future<Status> nack() && {
    auto impl = std::move(impl_);
    return impl->nack();
  }

  /// Returns the approximate number of times that Cloud Pub/Sub has attempted
  /// to deliver the associated message to a subscriber.
  std::int32_t delivery_attempt() const { return impl_->delivery_attempt(); }

  /// Allow applications to mock an `ExactlyOnceAckHandler`.
  class Impl {
   public:
    virtual ~Impl() = 0;
    /// The implementation for `ExactlyOnceAckHandler::ack()`
    virtual future<Status> ack() {
      return make_ready_future(
          Status(StatusCode::kUnimplemented, "base class"));
    }
    /// The implementation for `ExactlyOnceAckHandler::nack()`
    virtual future<Status> nack() {
      return make_ready_future(
          Status(StatusCode::kUnimplemented, "base class"));
    }
    /// The implementation for `ExactlyOnceAckHandler::delivery_attempt()`
    virtual std::int32_t delivery_attempt() const { return 0; }
  };

  /**
   * Applications may use this constructor in their mocks.
   */
  explicit ExactlyOnceAckHandler(std::unique_ptr<Impl> impl)
      : impl_(std::move(impl)) {}

 private:
  std::unique_ptr<Impl> impl_;
};

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace pubsub
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_EXACTLY_ONCE_ACK_HANDLER_H
