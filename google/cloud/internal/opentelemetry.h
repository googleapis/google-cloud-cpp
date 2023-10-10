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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_OPENTELEMETRY_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_OPENTELEMETRY_H

#include "google/cloud/future.h"
#include "google/cloud/options.h"
#include "google/cloud/status.h"
#include "google/cloud/status_or.h"
#include "google/cloud/version.h"
#ifdef GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY
#include <opentelemetry/context/propagation/text_map_propagator.h>
#include <opentelemetry/nostd/shared_ptr.h>
#include <opentelemetry/nostd/string_view.h>
#include <opentelemetry/trace/span.h>
#include <opentelemetry/trace/tracer.h>
#endif  // GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY
#include <chrono>
#include <functional>

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {

#ifdef GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY

/**
 * Returns a [tracer] to use for creating [spans].
 *
 * This function exists for the sake of testing. Library maintainers should call
 * `MakeSpan(...)` directly to create a span.
 *
 * @see https://opentelemetry.io/docs/instrumentation/cpp/manual/#initializing-tracing
 *
 * [spans]:
 * https://opentelemetry.io/docs/concepts/signals/traces/#spans-in-opentelemetry
 * [tracer]: https://opentelemetry.io/docs/concepts/signals/traces/#tracer
 */
opentelemetry::nostd::shared_ptr<opentelemetry::trace::Tracer> GetTracer(
    Options const& options);

/**
 * Start a [span] using the current [tracer].
 *
 * The current tracer is determined by the prevailing `CurrentOptions()`.
 *
 * @see https://opentelemetry.io/docs/instrumentation/cpp/manual/#start-a-span
 *
 * [span]:
 * https://opentelemetry.io/docs/concepts/signals/traces/#spans-in-opentelemetry
 * [tracer]: https://opentelemetry.io/docs/concepts/signals/traces/#tracer
 */
opentelemetry::nostd::shared_ptr<opentelemetry::trace::Span> MakeSpan(
    opentelemetry::nostd::string_view name);

/**
 * Start a [span] using the current [tracer] with the specified attributes.
 *
 * The current tracer is determined by the prevailing `CurrentOptions()`.
 *
 * @see https://opentelemetry.io/docs/instrumentation/cpp/manual/#start-a-span
 *
 * [span]:
 * https://opentelemetry.io/docs/concepts/signals/traces/#spans-in-opentelemetry
 * [tracer]: https://opentelemetry.io/docs/concepts/signals/traces/#tracer
 */
opentelemetry::nostd::shared_ptr<opentelemetry::trace::Span> MakeSpan(
    opentelemetry::nostd::string_view name,
    std::initializer_list<std::pair<opentelemetry::nostd::string_view,
                                    opentelemetry::common::AttributeValue>>
        attributes);

/**
 * Start a [span] using the current [tracer] with the specified attributes and
 * links.
 *
 * The current tracer is determined by the prevailing `CurrentOptions()`.
 *
 * @see https://opentelemetry.io/docs/instrumentation/cpp/manual/#start-a-span
 *
 * [span]:
 * https://opentelemetry.io/docs/concepts/signals/traces/#spans-in-opentelemetry
 * [tracer]: https://opentelemetry.io/docs/concepts/signals/traces/#tracer
 */
template <class T>
opentelemetry::nostd::shared_ptr<opentelemetry::trace::Span> MakeSpan(
    opentelemetry::nostd::string_view name,
    std::initializer_list<std::pair<opentelemetry::nostd::string_view,
                                    opentelemetry::common::AttributeValue>>
        attributes,
    T const& links) {
  opentelemetry::trace::StartSpanOptions options;
  options.kind = opentelemetry::trace::SpanKind::kClient;
  return GetTracer(CurrentOptions())
      ->StartSpan(name,
                  opentelemetry::nostd::span<
                      std::pair<opentelemetry::nostd::string_view,
                                opentelemetry::common::AttributeValue> const>{
                      attributes.begin(), attributes.end()},
                  links, options);
}

/**
 * Extracts information from a `Status` and adds it to a span.
 *
 * This method will end the span, and set its [span status], accordingly. Other
 * details, such as error information, will be set as [attributes] on the span.
 *
 * @see https://opentelemetry.io/docs/concepts/signals/traces/#spans-in-opentelemetry
 *
 * [attributes]:
 * https://opentelemetry.io/docs/concepts/signals/traces/#attributes
 * [span status]:
 * https://opentelemetry.io/docs/concepts/signals/traces/#span-status
 */
void EndSpanImpl(opentelemetry::trace::Span& span, Status const& status);

/**
 * Extracts information from a `Status` and adds it to a span.
 *
 * The span is ended. The original value is returned, for the sake of
 * composition.
 */
Status EndSpan(opentelemetry::trace::Span& span, Status const& status);

/**
 * Extracts information from a `StatusOr<>` and adds it to a span.
 *
 * The span is ended. The original value is returned, for the sake of
 * composition.
 */
template <typename T>
StatusOr<T> EndSpan(opentelemetry::trace::Span& span, StatusOr<T> value) {
  EndSpanImpl(span, value.status());
  return value;
}

/**
 * Extracts information from a `future<>` and adds it to a span.
 *
 * The span is ended. The original value is returned, for the sake of
 * composition.
 */
template <typename T>
future<T> EndSpan(
    opentelemetry::nostd::shared_ptr<opentelemetry::trace::Span> span,
    future<T> fut) {
  return fut.then(
      [s = std::move(span)](auto f) { return EndSpan(*s, f.get()); });
}

/**
 * Ends a span with an ok status.
 *
 * This is used to end spans that wrap void functions.
 */
void EndSpan(opentelemetry::trace::Span& span);

#endif  // GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY

bool TracingEnabled(Options const& options);

/// Wraps the sleeper in a span, if tracing is enabled.
std::function<void(std::chrono::milliseconds)> MakeTracedSleeper(
    Options const& options,
    std::function<void(std::chrono::milliseconds)> const& sleeper);

/// Adds an attribute to the active span, if tracing is enabled.
void AddSpanAttribute(std::string const& key, std::string const& value);

}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_OPENTELEMETRY_H
