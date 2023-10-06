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
#endif  // GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {

#ifdef GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY

opentelemetry::nostd::shared_ptr<opentelemetry::trace::Tracer> GetTracer(
    Options const&) {
  auto provider = opentelemetry::trace::Provider::GetTracerProvider();
  return provider->GetTracer("gcloud-cpp", version_string());
}

opentelemetry::nostd::shared_ptr<opentelemetry::trace::Span> MakeSpan(
    opentelemetry::nostd::string_view name) {
  opentelemetry::trace::StartSpanOptions options;
  options.kind = opentelemetry::trace::SpanKind::kClient;
  return GetTracer(CurrentOptions())->StartSpan(name, options);
}

opentelemetry::nostd::shared_ptr<opentelemetry::trace::Span> MakeSpan(
    opentelemetry::nostd::string_view name,
    std::initializer_list<std::pair<opentelemetry::nostd::string_view,
                                    opentelemetry::common::AttributeValue>>
        attributes) {
  opentelemetry::trace::StartSpanOptions options;
  options.kind = opentelemetry::trace::SpanKind::kClient;
  return GetTracer(CurrentOptions())->StartSpan(name, attributes, options);
}

void EndSpanImpl(opentelemetry::trace::Span& span, Status const& status) {
  if (status.ok()) {
    span.SetStatus(opentelemetry::trace::StatusCode::kOk);
    span.SetAttribute("gl-cpp.status_code", 0);
    span.End();
    return;
  }
  span.SetStatus(opentelemetry::trace::StatusCode::kError, status.message());
  span.SetAttribute("gl-cpp.status_code", static_cast<int>(status.code()));
  auto const& ei = status.error_info();
  if (!ei.reason().empty()) {
    span.SetAttribute("gcloud.error.reason", ei.reason());
  }
  if (!ei.domain().empty()) {
    span.SetAttribute("gcloud.error.domain", ei.domain());
  }
  for (auto const& kv : ei.metadata()) {
    span.SetAttribute("gcloud.error.metadata." + kv.first, kv.second);
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

#endif  // GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY

#ifdef GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY
bool TracingEnabled(Options const& options) {
  return options.get<OpenTelemetryTracingOption>();
}
#else
bool TracingEnabled(Options const&) { return false; }
#endif  // GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY

std::function<void(std::chrono::milliseconds)> MakeTracedSleeper(
    Options const& options,
    std::function<void(std::chrono::milliseconds)> const& sleeper) {
#ifdef GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY
  if (TracingEnabled(options)) {
    return [=](std::chrono::milliseconds p) {
      auto span = MakeSpan("Backoff");
      sleeper(p);
      span->End();
    };
  }
#endif  // GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY
  (void)options;
  return sleeper;
}

#ifdef GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY
void AddSpanAttribute(std::string const& key, std::string const& value) {
  if (TracingEnabled(CurrentOptions())) {
    auto span = opentelemetry::trace::Tracer::GetCurrentSpan();
    span->SetAttribute(key, value);
  }
}
#else
void AddSpanAttribute(std::string const&, std::string const&) {}
#endif  // GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY

}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google
