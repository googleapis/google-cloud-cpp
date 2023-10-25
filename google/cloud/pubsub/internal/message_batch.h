// Copyright 2023 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_INTERNAL_MESSAGE_BATCH_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_INTERNAL_MESSAGE_BATCH_H

#include "google/cloud/pubsub/message.h"
#include "google/cloud/future.h"
#include "google/cloud/options.h"
#include <functional>

namespace google {
namespace cloud {
namespace pubsub_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

/**
 * An interface for storing and acting on a batch messages across calls and
 * callbacks in the `BatchingPublisherConnection`.
 *
 * The `BatchingPublisherConnection` receives messages to publish and batches
 * them in a single rpc. This interface offers a way to trace the life of the
 * message from the initial Publish call to when the stub returns.
 */
class MessageBatch {
 public:
  virtual ~MessageBatch() = default;

  // Saves the message from the Publish call. Invoked in
  // `BatchingPublisherConnection::Publish(...)`.
  virtual void SaveMessage(pubsub::Message m) = 0;

  // Captures information about a batch of messages before it's flushed. Invoked
  // in `BatchingPublisherConnection::FlushImpl(...)`. Returns a task to invoke
  // in another callback.
  virtual std::function<void(future<void>)> Flush() = 0;
};

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace pubsub_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_INTERNAL_MESSAGE_BATCH_H
