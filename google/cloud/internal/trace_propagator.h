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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_TRACE_PROPAGATOR_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_TRACE_PROPAGATOR_H

#include "google/cloud/options.h"
#include "google/cloud/version.h"
#include <opentelemetry/context/propagation/text_map_propagator.h>
#include <memory>

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {

/**
 * Returns a [propagator] to use for propagating context across process
 * boundaries.
 *
 * We use a composite propagator that includes the W3 Traceparent headers, as
 * well as the X-Cloud-Trace-Context header. These are the keys that Google
 * servers look for when they receive a request, and we almost exclusively send
 * requests to Google.
 *
 * @see https://opentelemetry.io/docs/instrumentation/cpp/manual/#context-propagation
 *
 * [propagator]:
 * https://opentelemetry.io/docs/reference/specification/context/api-propagators/#textmap-propagator
 */
std::unique_ptr<opentelemetry::context::propagation::TextMapPropagator>
MakePropagator();

}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_TRACE_PROPAGATOR_H

#endif  // GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY
