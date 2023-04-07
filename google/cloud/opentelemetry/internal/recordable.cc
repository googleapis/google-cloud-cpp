// Copyright 2023 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "google/cloud/opentelemetry/internal/recordable.h"

namespace google {
namespace cloud {
namespace otel_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

void Recordable::SetIdentity(
    opentelemetry::trace::SpanContext const& /*span_context*/,
    opentelemetry::trace::SpanId /*parent_span_id*/) noexcept {}

void Recordable::SetAttribute(
    opentelemetry::nostd::string_view /*key*/,
    opentelemetry::common::AttributeValue const& /*value*/) noexcept {}

void Recordable::AddEvent(
    opentelemetry::nostd::string_view /*name*/,
    opentelemetry::common::SystemTimestamp /*timestamp*/,
    opentelemetry::common::KeyValueIterable const& /*attributes*/) noexcept {}

void Recordable::AddLink(
    opentelemetry::trace::SpanContext const& /*span_context*/,
    opentelemetry::common::KeyValueIterable const& /*attributes*/) noexcept {}

void Recordable::SetStatus(
    opentelemetry::trace::StatusCode /*code*/,
    opentelemetry::nostd::string_view /*description*/) noexcept {}

void Recordable::SetName(opentelemetry::nostd::string_view /*name*/) noexcept {}

void Recordable::SetSpanKind(
    opentelemetry::trace::SpanKind /*span_kind*/) noexcept {}

void Recordable::SetResource(
    opentelemetry::sdk::resource::Resource const& /*resource*/) noexcept {}

void Recordable::SetStartTime(
    opentelemetry::common::SystemTimestamp /*start_time*/) noexcept {}

void Recordable::SetDuration(std::chrono::nanoseconds /*duration*/) noexcept {}

void Recordable::SetInstrumentationScope(
    opentelemetry::sdk::instrumentationscope::InstrumentationScope const&
    /*instrumentation_scope*/) noexcept {}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace otel_internal
}  // namespace cloud
}  // namespace google
