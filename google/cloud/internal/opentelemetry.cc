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

#include "google/cloud/internal/opentelemetry.h"
#include "google/cloud/opentelemetry_options.h"
#include "google/cloud/options.h"
#ifdef GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY
#include <opentelemetry/trace/provider.h>
#include <opentelemetry/trace/span_startoptions.h>
#include <sstream>
#include <string>
#include <thread>
#endif  // GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {

#ifdef GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY

opentelemetry::nostd::shared_ptr<opentelemetry::trace::Tracer> GetTracer(
    Options const&) {
  auto provider = opentelemetry::trace::Provider::GetTracerProvider();
  return provider->GetTracer("gl-cpp", version_string());
}

opentelemetry::trace::StartSpanOptions DefaultStartSpanOptions() {
  opentelemetry::trace::StartSpanOptions default_options;
  default_options.kind = opentelemetry::trace::SpanKind::kClient;
  return default_options;
}

opentelemetry::nostd::shared_ptr<opentelemetry::trace::Span> MakeSpanImpl(
    opentelemetry::nostd::string_view name,
    opentelemetry::common::KeyValueIterable const& attributes,
    opentelemetry::trace::SpanContextKeyValueIterable const& links,
    opentelemetry::trace::StartSpanOptions const& options) {
  return GetTracer(CurrentOptions())
      ->StartSpan(name, attributes, links, options);
}

opentelemetry::nostd::shared_ptr<opentelemetry::trace::Span> MakeSpan(
    opentelemetry::nostd::string_view name,
    opentelemetry::trace::StartSpanOptions const& options) {
  return MakeSpan(name, {}, {}, options);
}

opentelemetry::nostd::shared_ptr<opentelemetry::trace::Span> MakeSpan(
    opentelemetry::nostd::string_view name,
    std::initializer_list<std::pair<opentelemetry::nostd::string_view,
                                    opentelemetry::common::AttributeValue>>
        attributes,
    opentelemetry::trace::StartSpanOptions const& options) {
  return MakeSpan(name, attributes, {}, options);
}

opentelemetry::nostd::shared_ptr<opentelemetry::trace::Span> MakeSpan(
    opentelemetry::nostd::string_view name,
    std::initializer_list<std::pair<opentelemetry::nostd::string_view,
                                    opentelemetry::common::AttributeValue>>
        attributes,
    std::initializer_list<
        std::pair<opentelemetry::trace::SpanContext,
                  std::initializer_list<
                      std::pair<opentelemetry::nostd::string_view,
                                opentelemetry::common::AttributeValue>>>>
        links,
    opentelemetry::trace::StartSpanOptions const& options) {
  return MakeSpan(
      name,
      opentelemetry::nostd::span<
          std::pair<opentelemetry::nostd::string_view,
                    opentelemetry::common::AttributeValue> const>{
          attributes.begin(), attributes.end()},
      opentelemetry::nostd::span<
          std::pair<opentelemetry::trace::SpanContext,
                    std::initializer_list<std::pair<
                        opentelemetry::nostd::string_view,
                        opentelemetry::common::AttributeValue>>> const>{
          links.begin(), links.end()},
      options);
}

void EndSpanImpl(opentelemetry::trace::Span& span, Status const& status) {
  if (status.ok()) {
    span.SetStatus(opentelemetry::trace::StatusCode::kOk);
    span.SetAttribute("gl-cpp.status_code", 0);
    span.End();
    return;
  }
  // Note that the Cloud Trace UI drops the span's status, so we also write it
  // as an attribute.
  span.SetStatus(opentelemetry::trace::StatusCode::kError, status.message());
  span.SetAttribute("gl-cpp.status_code", static_cast<int>(status.code()));
  span.SetAttribute("gl-cpp.error.message", status.message());
  auto const& ei = status.error_info();
  if (!ei.reason().empty()) {
    span.SetAttribute("gl-cpp.error.reason", ei.reason());
  }
  if (!ei.domain().empty()) {
    span.SetAttribute("gl-cpp.error.domain", ei.domain());
  }
  for (auto const& kv : ei.metadata()) {
    span.SetAttribute("gl-cpp.error.metadata." + kv.first, kv.second);
  }
  span.End();
}

Status EndSpan(opentelemetry::trace::Span& span, Status const& status) {
  EndSpanImpl(span, status);
  return status;
}

// Note: we aren't passing the opentelemetry::nostd::shared_ptr<Span> because
// clang-tidy thinks that it should be passed as const
// opentelemetry::nostd::shared_ptr<Span>& since the container isn't mutated.
void EndSpan(opentelemetry::trace::Span& span) { EndSpanImpl(span, Status{}); }

std::string ToString(opentelemetry::trace::TraceId const& trace_id) {
  constexpr int kSize = opentelemetry::trace::TraceId::kSize * 2;
  char trace_id_array[kSize];
  trace_id.ToLowerBase16(trace_id_array);
  return std::string(trace_id_array, kSize);
}

std::string ToString(opentelemetry::trace::SpanId const& span_id) {
  constexpr int kSize = opentelemetry::trace::SpanId::kSize * 2;
  char span_id_array[kSize];
  span_id.ToLowerBase16(span_id_array);
  return std::string(span_id_array, kSize);
}

std::string CurrentThreadId() {
  std::ostringstream os;
  os << std::this_thread::get_id();
  return std::move(os).str();
}

#endif  // GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY

#ifdef GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY
bool TracingEnabled(Options const& options) {
  return options.get<OpenTelemetryTracingOption>();
}
#else
bool TracingEnabled(Options const&) { return false; }
#endif  // GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY

#ifdef GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY
void AddSpanAttribute(Options const& options, std::string const& key,
                      std::string const& value) {
  if (!TracingEnabled(options)) return;
  auto span = opentelemetry::trace::Tracer::GetCurrentSpan();
  span->SetAttribute(key, value);
}
#else
void AddSpanAttribute(Options const&, std::string const&, std::string const&) {}
#endif  // GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY

}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google
