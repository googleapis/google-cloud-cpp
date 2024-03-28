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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_INTERNAL_MESSAGE_PROPAGATOR_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_INTERNAL_MESSAGE_PROPAGATOR_H

#ifdef GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY

#include "google/cloud/pubsub/internal/message_carrier.h"
#include "google/cloud/pubsub/message.h"
#include <opentelemetry/context/context.h>
#include <opentelemetry/context/propagation/text_map_propagator.h>

namespace google {
namespace cloud {
namespace pubsub_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

/**
 * Injects the current trace context into the @p message using the
 * `MessageCarrier`
 */
void InjectTraceContext(
    pubsub::Message& message,
    opentelemetry::context::propagation::TextMapPropagator& propagator);

/**
 * Extract the current trace context from the @p message using the
 * `MessageCarrier`
 */
opentelemetry::context::Context ExtractTraceContext(
    pubsub::Message& message,
    opentelemetry::context::propagation::TextMapPropagator& propagator);

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace pubsub_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_INTERNAL_MESSAGE_PROPAGATOR_H
