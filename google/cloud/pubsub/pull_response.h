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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_PULL_RESPONSE_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_PULL_RESPONSE_H

#include "google/cloud/pubsub/message.h"
#include "google/cloud/pubsub/pull_ack_handler.h"
#include "google/cloud/pubsub/version.h"

namespace google {
namespace cloud {
namespace pubsub {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

/**
 * The response for a blocking pull.
 *
 * If the application invokes `handler.nack()` or allows `handler` to go out
 * of scope, then the service will redeliver the message.
 *
 * With exactly-once delivery subscriptions, the service will stop
 * redelivering the message once the application invokes `handler.ack()` and
 * the invocation succeeds. With best-efforts subscriptions, the service *may*
 * redeliver the message, even after a successful `handler.ack()` invocation.
 *
 * If `handler` is not an rvalue, you may need to use `std::move(ack).ack()`
 * or `std::move(handler).nack()`.
 *
 * @see https://cloud.google.com/pubsub/docs/exactly-once-delivery
 */
struct PullResponse {
  /// The ack/nack handler associated with this message.
  pubsub::PullAckHandler handler;
  /// The message attributes and payload.
  pubsub::Message message;
};

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace pubsub
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_PULL_RESPONSE_H
