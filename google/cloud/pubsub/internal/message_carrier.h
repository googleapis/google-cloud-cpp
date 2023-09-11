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

#ifdef GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_INTERNAL_MESSAGE_CARRIER_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_INTERNAL_MESSAGE_CARRIER_H

#include "google/cloud/pubsub/message.h"
#include "google/cloud/options.h"
#include <opentelemetry/context/propagation/text_map_propagator.h>
#include <opentelemetry/nostd/shared_ptr.h>
#include <opentelemetry/trace/span.h>

namespace google {
namespace cloud {
namespace pubsub_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

/**
 * A [carrier] for a Pub/Sub Message. This injects the key/value into the
 * message attributes.
 *
 * [carrier]:
 * https://opentelemetry.io/docs/reference/specification/context/api-propagators/#carrier
 */
class MessageCarrier
    : public opentelemetry::context::propagation::TextMapCarrier {
 public:
  explicit MessageCarrier(pubsub::Message& message) : message_(message) {}

  opentelemetry::nostd::string_view Get(
      opentelemetry::nostd::string_view key) const noexcept override;

  void Set(opentelemetry::nostd::string_view key,
           opentelemetry::nostd::string_view value) noexcept override;

 private:
  pubsub::Message& message_;
};

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace pubsub_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_INTERNAL_MESSAGE_CARRIER_H

#endif  // GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY
