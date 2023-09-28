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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_INTERNAL_MESSAGE_CARRIER_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_INTERNAL_MESSAGE_CARRIER_H

#ifdef GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY

#include "google/cloud/pubsub/message.h"
#include "google/cloud/version.h"
#include <opentelemetry/context/propagation/text_map_propagator.h>
#include <opentelemetry/nostd/string_view.h>

namespace google {
namespace cloud {
namespace pubsub_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

/**
 * A [carrier] for a Pub/Sub Message.
 *
 * This class sets and accesses key value pairs stored in the Message attributes
 * with the key prefix "googclient_".
 *
 * [carrier]:
 * https://opentelemetry.io/docs/reference/specification/context/api-propagators/#carrier
 */
class MessageCarrier
    : public opentelemetry::context::propagation::TextMapCarrier {
 public:
  explicit MessageCarrier(pubsub::Message& message) : message_(message) {}

  // Returns a string_view to the value for a given key if it exists or a null
  // string_view. Note: the returned string_view is only valid as long message
  // as the lifetime of the message
  opentelemetry::nostd::string_view Get(
      opentelemetry::nostd::string_view key) const noexcept override;

  // Injects the key/value into the message attributes.
  void Set(opentelemetry::nostd::string_view key,
           opentelemetry::nostd::string_view value) noexcept override;

 private:
  pubsub::Message& message_;
};

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace pubsub_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_INTERNAL_MESSAGE_CARRIER_H
