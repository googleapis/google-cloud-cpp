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
#include "google/cloud/internal/absl_str_cat_quiet.h"
#include "google/cloud/internal/time_utils.h"
#include "absl/time/time.h"

namespace google {
namespace cloud {
namespace otel_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

// Taken from:
// http://35.193.25.4/TensorFlow/models/research/syntaxnet/util/utf8/unilib_utf8_utils.h
bool IsTrailByte(char x) { return static_cast<signed char>(x) < -0x40; }

class AttributeVisitor {
 public:
  AttributeVisitor(
      google::devtools::cloudtrace::v2::Span::Attributes& attributes,
      opentelemetry::nostd::string_view key, std::size_t limit)
      : attributes_(attributes), key_(key), limit_(limit) {}

  void operator()(bool value) {
    auto* proto = ProtoOrDrop();
    if (!proto) return Drop();
    proto->set_bool_value(value);
  }
  void operator()(std::int32_t value) {
    auto* proto = ProtoOrDrop();
    if (!proto) return Drop();
    proto->set_int_value(value);
  }
  void operator()(std::uint32_t value) {
    auto* proto = ProtoOrDrop();
    if (!proto) return Drop();
    proto->set_int_value(value);
  }
  void operator()(std::int64_t value) {
    auto* proto = ProtoOrDrop();
    if (!proto) return Drop();
    proto->set_int_value(value);
  }
  void operator()(std::uint64_t value) {
    auto* proto = ProtoOrDrop();
    if (!proto) return Drop();
    proto->set_int_value(value);
  }
  // The Cloud Trace proto does not accept floating point values, so we convert
  // them to strings.
  void operator()(double value) {
    auto* proto = ProtoOrDrop();
    if (!proto) return Drop();
    SetTruncatableString(*proto->mutable_string_value(), absl::StrCat(value),
                         kAttributeValueStringLimit);
  }
  void operator()(char const* value) {
    auto* proto = ProtoOrDrop();
    if (!proto) return Drop();
    SetTruncatableString(*proto->mutable_string_value(), value,
                         kAttributeValueStringLimit);
  }
  void operator()(opentelemetry::nostd::string_view value) {
    auto* proto = ProtoOrDrop();
    if (!proto) return Drop();
    SetTruncatableString(*proto->mutable_string_value(), value,
                         kAttributeValueStringLimit);
  }
  // There is no mapping from a `span<T>` to the Cloud Trace proto. We just drop
  // these attributes.
  template <typename T>
  void operator()(opentelemetry::nostd::span<T>) {
    Drop();
  }

 private:
  // Returns nullptr if we drop the attribute. Otherwise, returns an
  // AttributeValue proto to set.
  google::devtools::cloudtrace::v2::AttributeValue* ProtoOrDrop() {
    // We drop attributes whose keys are too long.
    if (key_.size() > kAttributeKeyStringLimit) return nullptr;

    auto& map = *attributes_.mutable_attribute_map();
    // We do not do any sampling. We just accept the first N attributes we are
    // given, and discard the rest. We may want to consider reservoir sampling
    // in the future. See: https://en.wikipedia.org/wiki/Reservoir_sampling
    if (map.size() < limit_) return &map[{key_.data(), key_.size()}];

    // If the map is full, we can still overwrite existing keys.
    auto it = map.find({key_.data(), key_.size()});
    if (it == map.end()) return nullptr;
    return &it->second;
  }

  void Drop() {
    attributes_.set_dropped_attributes_count(
        attributes_.dropped_attributes_count() + 1);
  }

  google::devtools::cloudtrace::v2::Span::Attributes& attributes_;
  opentelemetry::nostd::string_view key_;
  std::size_t limit_;
};

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

void AddAttribute(
    google::devtools::cloudtrace::v2::Span::Attributes& attributes,
    opentelemetry::nostd::string_view key,
    opentelemetry::common::AttributeValue const& value, std::size_t limit) {
  absl::visit(AttributeVisitor{attributes, key, limit}, value);
}

google::devtools::cloudtrace::v2::Span&& Recordable::as_proto() && {
  return std::move(span_);
}

void Recordable::SetIdentity(
    opentelemetry::trace::SpanContext const& span_context,
    opentelemetry::trace::SpanId parent_span_id) noexcept {
  std::array<char, 2 * opentelemetry::trace::TraceId::kSize> hex_trace_buf;
  span_context.trace_id().ToLowerBase16(hex_trace_buf);
  std::string const hex_trace(hex_trace_buf.data(),
                              2 * opentelemetry::trace::TraceId::kSize);

  std::array<char, 2 * opentelemetry::trace::SpanId::kSize> hex_span_buf;
  span_context.span_id().ToLowerBase16(hex_span_buf);
  std::string const hex_span(hex_span_buf.data(),
                             2 * opentelemetry::trace::SpanId::kSize);

  std::array<char, 2 * opentelemetry::trace::SpanId::kSize> hex_parent_span_buf;
  parent_span_id.ToLowerBase16(hex_parent_span_buf);
  std::string const hex_parent_span(hex_parent_span_buf.data(),
                                    2 * opentelemetry::trace::SpanId::kSize);

  span_.set_name(project_.FullName() + "/traces/" + hex_trace + "/spans/" +
                 hex_span);
  span_.set_span_id(hex_span);
  span_.set_parent_span_id(hex_parent_span);
}

void Recordable::SetAttribute(
    opentelemetry::nostd::string_view key,
    opentelemetry::common::AttributeValue const& value) noexcept {
  AddAttribute(*span_.mutable_attributes(), key, value, kSpanAttributeLimit);
}

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
    opentelemetry::common::SystemTimestamp start_time) noexcept {
  *span_.mutable_start_time() = internal::ToProtoTimestamp(start_time);
}

void Recordable::SetDuration(std::chrono::nanoseconds duration) noexcept {
  auto end_time =
      internal::ToAbslTime(span_.start_time()) + absl::FromChrono(duration);
  *span_.mutable_end_time() = internal::ToProtoTimestamp(std::move(end_time));
}

void Recordable::SetInstrumentationScope(
    opentelemetry::sdk::instrumentationscope::InstrumentationScope const&
    /*instrumentation_scope*/) noexcept {}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace otel_internal
}  // namespace cloud
}  // namespace google
