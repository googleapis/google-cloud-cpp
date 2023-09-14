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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_REST_OPENTELEMETRY_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_REST_OPENTELEMETRY_H

#ifdef GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY
#include "google/cloud/internal/rest_request.h"
#include "google/cloud/internal/trace_propagator.h"
#include "google/cloud/options.h"
#include <opentelemetry/nostd/shared_ptr.h>
#include <opentelemetry/trace/span.h>

namespace google {
namespace cloud {
namespace rest_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

/**
 * Propagate trace context for an outbound HTTP request.
 *
 * The trace context is added as metadata in the `RestContext`. By
 * injecting the trace context, we can potentially pick up a client side span
 * from within Google's servers.
 *
 * The format of the metadata is determined by the `TextMapPropagator` used for
 * the given call. Circa 2023-04, Google expects an `traceparent`
 * [header].
 *
 * @see https://opentelemetry.io/docs/concepts/instrumenting-library/#injecting-context
 *
 * [header]: https://www.w3.org/TR/trace-context/#traceparent-header
 */
void InjectTraceContext(
    RestContext& context,
    opentelemetry::context::propagation::TextMapPropagator& propagator);

/**
 * Make a span, setting attributes related to HTTP.
 *
 * @see
 * https://opentelemetry.io/docs/reference/specification/trace/semantic_conventions/http/
 * for the semantic conventions used for span names and attributes.
 */
opentelemetry::nostd::shared_ptr<opentelemetry::trace::Span> MakeSpanHttp(
    RestRequest const& request, opentelemetry::nostd::string_view method);

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace rest_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_REST_OPENTELEMETRY_H
