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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_OPENTELEMETRY_INTERNAL_RECORDABLE_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_OPENTELEMETRY_INTERNAL_RECORDABLE_H

#include "google/cloud/project.h"
#include "google/cloud/version.h"
#include "google/devtools/cloudtrace/v2/trace.pb.h"
#include "google/devtools/cloudtrace/v2/tracing.pb.h"
#include <opentelemetry/common/attribute_value.h>
#include <opentelemetry/sdk/trace/recordable.h>
#include <opentelemetry/version.h>
#include <functional>
#include <utility>

namespace google {
namespace cloud {
namespace otel_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

// https://github.com/googleapis/googleapis/blob/11ddd42bcee5961c34e8529638d51e9866b65065/google/devtools/cloudtrace/v2/trace.proto#L255-L256
std::size_t constexpr kSpanAttributeLimit = 32;

// https://github.com/googleapis/googleapis/blob/a5e03049afc43ccb1217d5dd3be53006fc278643/google/devtools/cloudtrace/v2/trace.proto#L233
std::size_t constexpr kDisplayNameStringLimit = 128;

// https://github.com/googleapis/googleapis/blob/4e8d3907aec680562c9243774c67adc6d713fe50/google/devtools/cloudtrace/v2/trace.proto#L49
std::size_t constexpr kAttributeKeyStringLimit = 128;

// https://github.com/googleapis/googleapis/blob/4e8d3907aec680562c9243774c67adc6d713fe50/google/devtools/cloudtrace/v2/trace.proto#L50
std::size_t constexpr kAttributeValueStringLimit = 256;

// https://github.com/googleapis/googleapis/blob/6774ccbbc3f182f6ae3a32dca29e1da489ad8a8f/google/devtools/cloudtrace/v2/trace.proto#L165-L166
std::size_t constexpr kSpanLinkAttributeLimit = 32;

// https://github.com/googleapis/googleapis/blob/52180f8ba240022dd8ce756ee69fe5a3c429ad4d/google/devtools/cloudtrace/v2/trace.proto#L266
std::size_t constexpr kSpanLinkLimit = 128;

// https://github.com/googleapis/googleapis/blob/0d5dd6137743d4052122f530349c714b77b25ed3/google/devtools/cloudtrace/v2/trace.proto#L68-L69
std::size_t constexpr kAnnotationDescriptionStringLimit = 256;

// https://github.com/googleapis/googleapis/blob/0d5dd6137743d4052122f530349c714b77b25ed3/google/devtools/cloudtrace/v2/trace.proto#L72-L73
std::size_t constexpr kAnnotationAttributeLimit = 4;

// https://github.com/googleapis/googleapis/blob/0d5dd6137743d4052122f530349c714b77b25ed3/google/devtools/cloudtrace/v2/trace.proto#L262-L263
std::size_t constexpr kSpanAnnotationLimit = 32;

/**
 * Helper to set [TruncatableString] fields in a [Span] proto.
 *
 * The service defines the limit for such fields in its documentation.
 *
 * [Span]:
 * https://cloud.google.com/trace/docs/reference/v2/rpc/google.devtools.cloudtrace.v2#google.devtools.cloudtrace.v2.Span
 * [TruncatableString]:
 * https://cloud.google.com/trace/docs/reference/v2/rpc/google.devtools.cloudtrace.v2#google.devtools.cloudtrace.v2.TruncatableString
 */
void SetTruncatableString(
    google::devtools::cloudtrace::v2::TruncatableString& proto,
    opentelemetry::nostd::string_view value, std::size_t limit);

/**
 * Helper to set [Attributes] fields in a [Span] proto.
 *
 * This function converts OpenTelemetry attributes to Cloud Trace attributes.
 * Sadly there is not a great mapping for all attributes.
 *
 * Cloud Trace attributes also have limits, which are defined in the proto
 * comments. For example, we are limited to 32 attributes per Span. We enforce
 * the limit by accepting the first N attributes we are given, and dropping the
 * rest.
 *
 * [Attributes]:
 * https://cloud.google.com/trace/docs/reference/v2/rpc/google.devtools.cloudtrace.v2#google.devtools.cloudtrace.v2.Span.Attributes
 * [Span]:
 * https://cloud.google.com/trace/docs/reference/v2/rpc/google.devtools.cloudtrace.v2#google.devtools.cloudtrace.v2.Span
 */
void AddAttribute(
    google::devtools::cloudtrace::v2::Span::Attributes& attributes,
    opentelemetry::nostd::string_view key,
    opentelemetry::common::AttributeValue const& value, std::size_t limit);

/**
 * Implements the OpenTelemetry Recordable specification.
 *
 * This class is responsible for mapping an OpenTelemetry span into a Cloud
 * Trace span.
 */
class Recordable final : public opentelemetry::sdk::trace::Recordable {
 public:
  using Generator = std::function<int(int)>;

  explicit Recordable(Project project, Generator generator)
      : project_(std::move(project)), generator_(std::move(generator)) {}

  bool valid() const { return valid_; }

  google::devtools::cloudtrace::v2::Span&& as_proto() &&;

  void SetIdentity(
      opentelemetry::trace::SpanContext const& span_context,
      opentelemetry::trace::SpanId parent_span_id) noexcept override;

  void SetAttribute(
      opentelemetry::nostd::string_view key,
      opentelemetry::common::AttributeValue const& value) noexcept override;

  void AddEvent(opentelemetry::nostd::string_view name,
                opentelemetry::common::SystemTimestamp timestamp,
                opentelemetry::common::KeyValueIterable const&
                    attributes) noexcept override;

  void AddLink(opentelemetry::trace::SpanContext const& span_context,
               opentelemetry::common::KeyValueIterable const&
                   attributes) noexcept override;

  void SetStatus(
      opentelemetry::trace::StatusCode code,
      opentelemetry::nostd::string_view description) noexcept override;

  void SetName(opentelemetry::nostd::string_view name) noexcept override;

#if OPENTELEMETRY_VERSION_MAJOR > 1 || \
    (OPENTELEMETRY_VERSION_MAJOR == 1 && OPENTELEMETRY_VERSION_MINOR >= 14)
  void SetTraceFlags(opentelemetry::trace::TraceFlags) noexcept override {}
#endif

  void SetSpanKind(opentelemetry::trace::SpanKind span_kind) noexcept override;

  void SetResource(
      opentelemetry::sdk::resource::Resource const& resource) noexcept override;

  void SetStartTime(
      opentelemetry::common::SystemTimestamp start_time) noexcept override;

  void SetDuration(std::chrono::nanoseconds duration) noexcept override;

  void SetInstrumentationScope(
      opentelemetry::sdk::instrumentationscope::InstrumentationScope const&
          instrumentation_scope) noexcept override;

 private:
  void SetIdentityImpl(opentelemetry::trace::SpanContext const& span_context,
                       opentelemetry::trace::SpanId parent_span_id);
  void AddEventImpl(opentelemetry::nostd::string_view name,
                    opentelemetry::common::SystemTimestamp timestamp,
                    opentelemetry::common::KeyValueIterable const& attributes);
  void AddLinkImpl(opentelemetry::trace::SpanContext const& span_context,
                   opentelemetry::common::KeyValueIterable const& attributes);
  void SetStatusImpl(opentelemetry::trace::StatusCode code,
                     opentelemetry::nostd::string_view description);
  void SetResourceImpl(opentelemetry::sdk::resource::Resource const& resource);
  void SetInstrumentationScopeImpl();

  Project project_;
  google::devtools::cloudtrace::v2::Span span_;
  bool valid_ = true;

  std::string scope_name_;
  std::string scope_version_;
  Generator generator_;
  int timed_event_count_ = 0;
};

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace otel_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_OPENTELEMETRY_INTERNAL_RECORDABLE_H
