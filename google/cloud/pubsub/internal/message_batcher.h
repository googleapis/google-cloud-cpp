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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_INTERNAL_MESSAGE_BATCHER_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_INTERNAL_MESSAGE_BATCHER_H

#include "google/cloud/pubsub/message.h"
#include "google/cloud/pubsub/version.h"
#include "google/cloud/future.h"
#include "google/cloud/status_or.h"

namespace google {
namespace cloud {
namespace pubsub_internal {
inline namespace GOOGLE_CLOUD_CPP_PUBSUB_NS {

/**
 * Defines the interface to batch published messages.
 */
class MessageBatcher {
 public:
  virtual ~MessageBatcher() = default;

  /// Receive a new message to batch
  virtual future<StatusOr<std::string>> Publish(pubsub::Message m) = 0;

  /// Flush any pending messages
  virtual void Flush() = 0;
};

}  // namespace GOOGLE_CLOUD_CPP_PUBSUB_NS
}  // namespace pubsub_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_INTERNAL_MESSAGE_BATCHER_H
