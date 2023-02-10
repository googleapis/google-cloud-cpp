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
#include "google/cloud/internal/opentelemetry_options.h"
#include "google/cloud/options.h"
#ifdef GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY
#include <opentelemetry/context/propagation/global_propagator.h>
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

opentelemetry::nostd::shared_ptr<
    opentelemetry::context::propagation::TextMapPropagator>
GetTextMapPropagator(Options const&) {
  return opentelemetry::context::propagation::GlobalTextMapPropagator::
      GetGlobalPropagator();
}

opentelemetry::nostd::shared_ptr<opentelemetry::trace::Span> MakeSpan(
    opentelemetry::nostd::string_view name) {
  opentelemetry::trace::StartSpanOptions options;
  options.kind = opentelemetry::trace::SpanKind::kClient;
  return GetTracer(CurrentOptions())->StartSpan(name, options);
}

void EndSpanImpl(opentelemetry::trace::Span& span, Status const& status) {
  if (status.ok()) {
    span.SetStatus(opentelemetry::trace::StatusCode::kOk);
    span.SetAttribute("gcloud.status_code", 0);
    span.End();
    return;
  }
  span.SetStatus(opentelemetry::trace::StatusCode::kError, status.message());
  span.SetAttribute("gcloud.status_code", static_cast<int>(status.code()));
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

#endif  // GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY

#ifdef GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY
bool TracingEnabled(Options const& options) {
  return options.get<OpenTelemetryTracingOption>();
}
#else
bool TracingEnabled(Options const&) { return false; }
#endif  // GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY

std::function<void(std::chrono::milliseconds)> MakeTracedSleeper(
    std::function<void(std::chrono::milliseconds)> const& sleeper) {
#ifdef GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY
  if (TracingEnabled(CurrentOptions())) {
    return [=](std::chrono::milliseconds p) {
      auto span = MakeSpan("Backoff");
      sleeper(p);
      span->End();
    };
  }
#endif  // GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY
  return sleeper;
}

}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google
