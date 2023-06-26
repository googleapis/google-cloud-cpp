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
#include "google/cloud/opentelemetry/internal/monitored_resource.h"
#include "google/cloud/internal/absl_str_cat_quiet.h"
#include "google/cloud/internal/noexcept_action.h"
#include "google/cloud/internal/time_utils.h"
#include "absl/time/time.h"
#include <google/rpc/code.pb.h>

namespace google {
namespace cloud {
namespace otel_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

// Taken from:
// http://35.193.25.4/TensorFlow/models/research/syntaxnet/util/utf8/unilib_utf8_utils.h
bool IsTrailByte(char x) { return static_cast<signed char>(x) < -0x40; }

// OpenTelemetry's semantic conventions for attribute keys differ from Cloud
// Trace's semantics for label keys. So translate from one to the other.
//
// See: https://cloud.google.com/trace/docs/trace-labels#canonical_labels
void MapKey(opentelemetry::nostd::string_view& key) {
  static auto const* m = new std::map<opentelemetry::nostd::string_view,
                                      opentelemetry::nostd::string_view>{
      {"http.host", "/http/host"},
      {"http.method", "/http/method"},
      {"http.target", "/http/path"},
      {"http.status_code", "/http/status_code"},
      {"http.url", "/http/url"},
      {"http.user_agent", "/http/user_agent"},
      {"http.request_content_length", "/http/request/size"},
      {"http.response_content_length", "/http/response/size"},
      {"http.scheme", "/http/client_protocol"},
      {"http.route", "/http/route"},
  };
  auto it = m->find(key);
  if (it != m->end()) key = it->second;
}

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

    MapKey(key_);

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

google::devtools::cloudtrace::v2::Span::SpanKind MapSpanKind(
    opentelemetry::trace::SpanKind span_kind) {
  switch (span_kind) {
    case opentelemetry::trace::SpanKind::kInternal:
      return ::google::devtools::cloudtrace::v2::Span::INTERNAL;
    case opentelemetry::trace::SpanKind::kServer:
      return ::google::devtools::cloudtrace::v2::Span::SERVER;
    case opentelemetry::trace::SpanKind::kClient:
      return ::google::devtools::cloudtrace::v2::Span::CLIENT;
    case opentelemetry::trace::SpanKind::kProducer:
      return ::google::devtools::cloudtrace::v2::Span::PRODUCER;
    case opentelemetry::trace::SpanKind::kConsumer:
      return ::google::devtools::cloudtrace::v2::Span::CONSUMER;
    default:
      return ::google::devtools::cloudtrace::v2::Span::SPAN_KIND_UNSPECIFIED;
  }
}

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
  valid_ = valid_ && internal::NoExceptAction([&] {
             SetIdentityImpl(span_context, parent_span_id);
           });
}

void Recordable::SetAttribute(
    opentelemetry::nostd::string_view key,
    opentelemetry::common::AttributeValue const& value) noexcept {
  valid_ = valid_ && internal::NoExceptAction([&] {
             AddAttribute(*span_.mutable_attributes(), key, value,
                          kSpanAttributeLimit);
           });
}

void Recordable::AddEvent(
    opentelemetry::nostd::string_view name,
    opentelemetry::common::SystemTimestamp timestamp,
    opentelemetry::common::KeyValueIterable const& attributes) noexcept {
  valid_ = valid_ && internal::NoExceptAction(
                         [&] { AddEventImpl(name, timestamp, attributes); });
}

void Recordable::AddLink(
    opentelemetry::trace::SpanContext const& span_context,
    opentelemetry::common::KeyValueIterable const& attributes) noexcept {
  valid_ = valid_ && internal::NoExceptAction(
                         [&] { AddLinkImpl(span_context, attributes); });
}

void Recordable::SetStatus(
    opentelemetry::trace::StatusCode code,
    opentelemetry::nostd::string_view description) noexcept {
  valid_ = valid_ &&
           internal::NoExceptAction([&] { SetStatusImpl(code, description); });
}

void Recordable::SetName(opentelemetry::nostd::string_view name) noexcept {
  valid_ = valid_ && internal::NoExceptAction([&] {
             // Note that the `name` field in the `Span` proto refers to the GCP
             // resource name. We want to set the `display_name` field.
             SetTruncatableString(*span_.mutable_display_name(), name,
                                  kDisplayNameStringLimit);
           });
}

void Recordable::SetSpanKind(
    opentelemetry::trace::SpanKind span_kind) noexcept {
  valid_ = valid_ && internal::NoExceptAction(
                         [&] { span_.set_span_kind(MapSpanKind(span_kind)); });
}

void Recordable::SetResource(
    opentelemetry::sdk::resource::Resource const& resource) noexcept {
  valid_ =
      valid_ && internal::NoExceptAction([&] { SetResourceImpl(resource); });
}

void Recordable::SetStartTime(
    opentelemetry::common::SystemTimestamp start_time) noexcept {
  valid_ = valid_ && internal::NoExceptAction([&] {
             // std::chrono::system_clock may not have nanosecond resolution on
             // some platforms, so we avoid using it for conversions between
             // OpenTelemetry time and Protobuf time.
             auto t =
                 absl::FromUnixNanos(start_time.time_since_epoch().count());
             *span_.mutable_start_time() = internal::ToProtoTimestamp(t);
           });
}

void Recordable::SetDuration(std::chrono::nanoseconds duration) noexcept {
  valid_ = valid_ && internal::NoExceptAction([&] {
             auto end_time = internal::ToAbslTime(span_.start_time()) +
                             absl::FromChrono(duration);
             *span_.mutable_end_time() = internal::ToProtoTimestamp(end_time);
           });
}

void Recordable::SetInstrumentationScope(
    opentelemetry::sdk::instrumentationscope::InstrumentationScope const&
        instrumentation_scope) noexcept {
  SetAttribute("otel.scope.name", instrumentation_scope.GetName());
  if (!instrumentation_scope.GetVersion().empty()) {
    SetAttribute("otel.scope.version", instrumentation_scope.GetVersion());
  }
}

void Recordable::SetIdentityImpl(
    opentelemetry::trace::SpanContext const& span_context,
    opentelemetry::trace::SpanId parent_span_id) {
  std::array<char, 2 * opentelemetry::trace::TraceId::kSize> trace;
  span_context.trace_id().ToLowerBase16(trace);

  std::array<char, 2 * opentelemetry::trace::SpanId::kSize> span;
  span_context.span_id().ToLowerBase16(span);

  std::array<char, 2 * opentelemetry::trace::SpanId::kSize> parent_span;
  parent_span_id.ToLowerBase16(parent_span);

  span_.set_name(absl::StrCat(project_.FullName(), "/traces/",
                              absl::string_view{trace.data(), trace.size()},
                              "/spans/",
                              absl::string_view{span.data(), span.size()}));
  span_.set_span_id({span.data(), span.size()});
  span_.set_parent_span_id({parent_span.data(), parent_span.size()});
}

void Recordable::AddEventImpl(
    opentelemetry::nostd::string_view name,
    opentelemetry::common::SystemTimestamp timestamp,
    opentelemetry::common::KeyValueIterable const& attributes) {
  // Accept the first N events. Drop the rest.
  auto& events = *span_.mutable_time_events();
  if (events.time_event().size() == kSpanAnnotationLimit) {
    events.set_dropped_annotations_count(1 +
                                         events.dropped_annotations_count());
    return;
  }

  auto& event = *events.add_time_event();
  auto t = absl::FromUnixNanos(timestamp.time_since_epoch().count());
  *event.mutable_time() = internal::ToProtoTimestamp(t);
  // We assume this is an `Annotation` (which has arbitrary attributes) instead
  // of a `MessageEvent`, which has specific attributes.
  SetTruncatableString(*event.mutable_annotation()->mutable_description(), name,
                       kAnnotationDescriptionStringLimit);
  auto& proto = *event.mutable_annotation()->mutable_attributes();
  attributes.ForEachKeyValue(
      [&proto](opentelemetry::nostd::string_view key,
               opentelemetry::common::AttributeValue const& value) {
        AddAttribute(proto, key, value, kAnnotationAttributeLimit);
        return proto.attribute_map().size() != kAnnotationAttributeLimit;
      });
  proto.set_dropped_attributes_count(
      static_cast<int>(attributes.size() - proto.attribute_map().size()));
}

void Recordable::AddLinkImpl(
    opentelemetry::trace::SpanContext const& span_context,
    opentelemetry::common::KeyValueIterable const& attributes) {
  // Accept the first N links. Drop the rest.
  auto& links = *span_.mutable_links();
  if (links.link().size() == kSpanLinkLimit) {
    links.set_dropped_links_count(links.dropped_links_count() + 1);
    return;
  }

  std::array<char, 2 * opentelemetry::trace::TraceId::kSize> trace;
  span_context.trace_id().ToLowerBase16(trace);

  std::array<char, 2 * opentelemetry::trace::SpanId::kSize> span;
  span_context.span_id().ToLowerBase16(span);

  auto& link = *links.add_link();
  link.set_trace_id({trace.data(), trace.size()});
  link.set_span_id({span.data(), span.size()});

  auto& proto = *link.mutable_attributes();
  attributes.ForEachKeyValue(
      [&proto](opentelemetry::nostd::string_view key,
               opentelemetry::common::AttributeValue const& value) {
        AddAttribute(proto, key, value, kSpanLinkAttributeLimit);
        return proto.attribute_map().size() != kSpanLinkAttributeLimit;
      });
  proto.set_dropped_attributes_count(
      static_cast<int>(attributes.size() - proto.attribute_map().size()));
}

void Recordable::SetStatusImpl(opentelemetry::trace::StatusCode code,
                               opentelemetry::nostd::string_view description) {
  if (code == opentelemetry::trace::StatusCode::kUnset) return;
  auto& s = *span_.mutable_status();
  if (code == opentelemetry::trace::StatusCode::kOk) {
    s.set_code(google::rpc::Code::OK);
    return;
  }
  s.set_code(google::rpc::Code::UNKNOWN);
  *s.mutable_message() = std::string{description.data(), description.size()};
}

void Recordable::SetResourceImpl(
    opentelemetry::sdk::resource::Resource const& resource) {
  auto const& attributes = resource.GetAttributes();
  // TODO(#11775) - add resource attributes as span attributes
  auto mr = ToMonitoredResource(attributes);
  for (auto const& label : mr.labels) {
    SetAttribute(absl::StrCat("g.co/r/", mr.type, "/", label.first),
                 label.second);
  }
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace otel_internal
}  // namespace cloud
}  // namespace google
