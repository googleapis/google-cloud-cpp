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
namespace {
// Taken from:
// http://35.193.25.4/TensorFlow/models/research/syntaxnet/util/utf8/unilib_utf8_utils.h
bool IsTrailByte(char x) { return static_cast<signed char>(x) < -0x40; }
}  // namespace

void SetTruncatableString(
    google::devtools::cloudtrace::v2::TruncatableString& proto,
    opentelemetry::nostd::string_view value, std::size_t limit) {
  if (value.size() <= limit) {
    proto.set_value(value.data(), value.size());
    proto.set_truncated_byte_count(0);
    return;
  }

  // If limit points to the beginning of a utf8 character, truncate at the
  // limit. Otherwise, backtrack to the beginning of utf8 character.
  auto truncation_pos = limit;
  while (truncation_pos > 0 && IsTrailByte(value[truncation_pos])) {
    --truncation_pos;
  }

  proto.set_value(value.data(), truncation_pos);
  proto.set_truncated_byte_count(
      static_cast<std::int32_t>(value.size() - truncation_pos));
}

google::devtools::cloudtrace::v2::Span&& Recordable::as_proto() && {
  return std::move(span_);
}

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

void Recordable::SetName(opentelemetry::nostd::string_view name) noexcept {
  // Note that the `name` field in the `Span` proto refers to the GCP resource
  // name. We want to set the `display_name` field.
  SetTruncatableString(*span_.mutable_display_name(), name,
                       kDisplayNameStringLimit);
}

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
