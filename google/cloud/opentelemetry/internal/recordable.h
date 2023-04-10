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
#include <google/devtools/cloudtrace/v2/trace.pb.h>
#include <google/devtools/cloudtrace/v2/tracing.pb.h>
#include <opentelemetry/common/attribute_value.h>
#include <opentelemetry/sdk/trace/recordable.h>

namespace google {
namespace cloud {
namespace otel_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

// https://github.com/googleapis/googleapis/blob/a5e03049afc43ccb1217d5dd3be53006fc278643/google/devtools/cloudtrace/v2/trace.proto#L233
std::size_t constexpr kDisplayNameStringLimit = 128;

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

// TODO(#11156) - Implement this class.
/**
 * Implements the OpenTelemetry Recordable specification.
 *
 * This class is responsible for mapping an OpenTelemetry span into a Cloud
 * Trace span.
 */
class Recordable final : public opentelemetry::sdk::trace::Recordable {
 public:
  explicit Recordable(Project project) : project_(std::move(project)) {}

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
  Project project_;
  google::devtools::cloudtrace::v2::Span span_;
};

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace otel_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_OPENTELEMETRY_INTERNAL_RECORDABLE_H
