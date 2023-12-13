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

#include "google/cloud/pubsub/internal/message_propagator.h"
#include "google/cloud/pubsub/internal/message_carrier.h"
#include "google/cloud/pubsub/message.h"
#include "opentelemetry/context/propagation/text_map_propagator.h"
#include "opentelemetry/context/runtime_context.h"

namespace google {
namespace cloud {
namespace pubsub_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

void InjectTraceContext(
    pubsub::Message& message,
    opentelemetry::context::propagation::TextMapPropagator& propagator) {
  auto current = opentelemetry::context::RuntimeContext::GetCurrent();
  MessageCarrier carrier(message);
  propagator.Inject(carrier, current);
}

opentelemetry::context::Context ExtractTraceContext(
    pubsub::Message& message,
    opentelemetry::context::propagation::TextMapPropagator& propagator) {
  auto current = opentelemetry::context::RuntimeContext::GetCurrent();
  MessageCarrier carrier(message);
  return propagator.Extract(carrier, current);
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace pubsub_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY
