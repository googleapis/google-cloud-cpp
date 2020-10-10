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
 * This interface allows applications to acknowledge and reject messages in a
 * provided to the Cloud Pub/Sub C++ client library. Note that this class is
 * move-able, to support applications that process messages asynchronously.
 * However, this class is *not* copy-able, because messages can only be
 * acknowledged or rejected exactly once.
 */
class AckHandler {
 public:
  ~AckHandler();

  AckHandler(AckHandler&&) noexcept = default;
  AckHandler& operator=(AckHandler&&) noexcept = default;

  /// Acknowledge the message and return any (unrecoverable) RPC errors
  void ack() && {
    auto impl = std::move(impl_);
    impl->ack();
  }

  /// Reject the message and return any (unrecoverable) RPC errors
  void nack() && {
    auto impl = std::move(impl_);
    impl->nack();
  }

  /// The Cloud Pub/Sub acknowledge ID, useful for debugging and logging.
  std::string ack_id() const { return impl_->ack_id(); }

  /// The approximate number of times that Cloud Pub/Sub has attempted to
  /// deliver the associated message to a subscriber.
  std::int32_t delivery_attempt() const { return impl_->delivery_attempt(); }

  /// Allow applications to mock an `AckHandler`.
  class Impl {
   public:
    virtual ~Impl() = 0;
    /// The implementation for `AckHandler::ack()`
    virtual void ack() = 0;
    /// The implementation for `AckHandler::nack()`
    virtual void nack() = 0;
    /// The implementation for `AckHandler::ack_id()`
    virtual std::string ack_id() const = 0;
    /// The implementation for `AckHandler::delivery_attempt()`
    virtual std::int32_t delivery_attempt() const = 0;
  };

  /**
   * Application may use this constructor in their mocks.
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
