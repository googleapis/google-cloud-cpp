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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_INTERNAL_TRACING_HELPERS_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_INTERNAL_TRACING_HELPERS_H

#ifdef GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY

#include "google/cloud/pubsub/pull_ack_handler.h"
#include "google/cloud/pubsub/version.h"
#include <opentelemetry/trace/context.h>
#include <opentelemetry/trace/span.h>
#include <opentelemetry/trace/span_startoptions.h>
#include <string>

namespace google {
namespace cloud {
namespace pubsub_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

// Create start span options for a root span.
opentelemetry::trace::StartSpanOptions RootStartSpanOptions();

using TracingAttributes =
    std::vector<std::pair<opentelemetry::nostd::string_view,
                          opentelemetry::common::AttributeValue>>;

/// Create a list of links with @p span context if compiled with open telemetery
/// ABI 2.0.
std::vector<std::pair<opentelemetry::trace::SpanContext, TracingAttributes>>
CreateLinks(opentelemetry::trace::SpanContext const& span_context);

/// Adds two link attributes to the @p current span for the trace id and span id
/// from @p span.
void MaybeAddLinkAttributes(
    opentelemetry::trace::Span& current_span,
    opentelemetry::trace::SpanContext const& span_context,
    std::string const& span_name);

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace pubsub_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_INTERNAL_TRACING_HELPERS_H
