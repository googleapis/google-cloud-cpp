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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_APPLICATION_CALLBACK_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_APPLICATION_CALLBACK_H

#include "google/cloud/pubsub/version.h"
#include <functional>

namespace google {
namespace cloud {
namespace pubsub {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
class Message;
class AckHandler;
class ExactlyOnceAckHandler;

/**
 * Defines the interface for application-level callbacks.
 *
 * Applications provide a callable compatible with this type to receive
 * messages.  They acknowledge (or reject) messages using `AckHandler`. This is
 * a move-only type to support asynchronously acknowledgments.
 */
using ApplicationCallback = std::function<void(Message, AckHandler)>;

/**
 * Defines the interface for application-level callbacks with exactly-once
 * delivery.
 *
 * Applications provide a callable compatible with this type to receive
 * messages.  They acknowledge (or reject) messages using
 * `ExactlyOnceAckHandler`.  This is a move-only type to support asynchronous
 * acknowledgments.
 */
using ExactlyOnceApplicationCallback =
    std::function<void(pubsub::Message, ExactlyOnceAckHandler)>;

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace pubsub
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_APPLICATION_CALLBACK_H
