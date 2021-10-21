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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_ACK_HANDLER_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_ACK_HANDLER_H

#include "google/cloud/pubsub/version.h"
#include "google/cloud/status.h"
#include <memory>

namespace google {
namespace cloud {
namespace pubsub {
inline namespace GOOGLE_CLOUD_CPP_PUBSUB_NS {

/**
 * Defines the interface to acknowledge and reject messages.
 *
 * When applications register a callback to receive Pub/Sub messages the
 * callback must be able to receive both a `pubsub::Message` and its associated
 * `pubsub::AckHandler`. Actions on a `pubsub::AckHandler` always affect the
 * same message received in the callback. Applications cannot create standalone
 * handlers (except in unit tests via mocks).
 *
 * This interface allows applications to acknowledge and reject messages in a
 * provided to the Cloud Pub/Sub C++ client library. Note that this class is
 * move-able, to support applications that process messages asynchronously.
 * However, this class is *not* copy-able, because messages can only be
 * acknowledged or rejected exactly once.
 *
 * @par Thread Safety
 * This class is *thread compatible*, only one thread should call non-const
 * member functions of this class at a time. Note that because the non-const
 * member functions are `&&` overloads the application can only call `ack()` or
 * `nack()` exactly once, and only one of them.
 */
class AckHandler {
 public:
  ~AckHandler();

  AckHandler(AckHandler&&) noexcept = default;
  AckHandler& operator=(AckHandler&&) noexcept = default;

  /**
   * Acknowledges the message associated with this handler.
   *
   * @par Idempotency
   * Note that this is not an idempotent operation, and therefore it is never
   * retried. Furthermore, the service may still resend a message after a
   * successful `ack()`. Applications developers are reminded that Cloud Pub/Sub
   * offers "at least once" semantics so they should be prepared to handle
   * duplicate messages.
   */
  void ack() && {
    auto impl = std::move(impl_);
    impl->ack();
  }

  /**
   * Rejects the message associated with this handler.
   *
   * @par Idempotency
   * Note that this is not an idempotent operation, and therefore it is never
   * retried. Furthermore, the service may still resend a message after a
   * successful `nack()`. Applications developers are reminded that Cloud
   * Pub/Sub offers "at least once" semantics so they should be prepared to
   * handle duplicate messages.
   */
  void nack() && {
    auto impl = std::move(impl_);
    impl->nack();
  }

  /// Returns the approximate number of times that Cloud Pub/Sub has attempted
  /// to deliver the associated message to a subscriber.
  std::int32_t delivery_attempt() const { return impl_->delivery_attempt(); }

  /// Allow applications to mock an `AckHandler`.
  class Impl {
   public:
    virtual ~Impl() = 0;
    /// The implementation for `AckHandler::ack()`
    virtual void ack() {}
    /// The implementation for `AckHandler::nack()`
    virtual void nack() {}
    /// The implementation for `AckHandler::delivery_attempt()`
    virtual std::int32_t delivery_attempt() const { return 0; }
  };

  /**
   * Applications may use this constructor in their mocks.
   */
  explicit AckHandler(std::unique_ptr<Impl> impl) : impl_(std::move(impl)) {}

 private:
  std::unique_ptr<Impl> impl_;
};

}  // namespace GOOGLE_CLOUD_CPP_PUBSUB_NS
}  // namespace pubsub
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_ACK_HANDLER_H
