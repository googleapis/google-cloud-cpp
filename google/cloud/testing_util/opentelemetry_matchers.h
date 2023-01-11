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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_TESTING_UTIL_OPENTELEMETRY_MATCHERS_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_TESTING_UTIL_OPENTELEMETRY_MATCHERS_H

#ifdef GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY
#include "google/cloud/version.h"
#include <gmock/gmock.h>
#include <opentelemetry/exporters/memory/in_memory_span_exporter.h>
#include <opentelemetry/sdk/instrumentationscope/instrumentation_scope.h>
#include <opentelemetry/sdk/trace/span_data.h>
#include <opentelemetry/trace/span.h>
#include <memory>
#include <string>

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace testing_util {

using SpanDataPtr = std::unique_ptr<opentelemetry::sdk::trace::SpanData>;

MATCHER(SpanHasInstrumentationScope, "") {
  auto const& scope = arg->GetInstrumentationScope();
  auto const& name = scope.GetName();
  auto const& version = scope.GetVersion();

  bool match = true;
  if (name != "gcloud-cpp") {
    *result_listener << "instrumentation scope name: " << name << ")\n";
    match = false;
  }
  if (version != version_string()) {
    *result_listener << "instrumentation scope version: " << version << ")\n";
    match = false;
  }
  return match;
}

MATCHER(SpanKindIsClient, "") {
  auto to_str = [](opentelemetry::trace::SpanKind k) {
    switch (k) {
      case opentelemetry::trace::SpanKind::kInternal:
        return "INTERNAL";
      case opentelemetry::trace::SpanKind::kServer:
        return "SERVER";
      case opentelemetry::trace::SpanKind::kClient:
        return "CLIENT";
      case opentelemetry::trace::SpanKind::kProducer:
        return "PRODUCER";
      case opentelemetry::trace::SpanKind::kConsumer:
      default:
        return "UNKNOWN";
    }
  };

  auto const& kind = arg->GetSpanKind();
  if (kind != opentelemetry::trace::SpanKind::kClient) {
    *result_listener << "span kind: " << to_str(kind) << ")\n";
    return false;
  }
  return true;
}

MATCHER_P(SpanNamed, name, "") {
  auto const& actual = arg->GetName();
  if (actual != name) {
    *result_listener << "span name: " << actual << ")\n";
    return false;
  }
  return true;
}

/**
 * Provides access to created spans.
 *
 * Calling this method will install an in-memory trace exporter. It returns a
 * type that provides access to captured spans.
 *
 * To extract the spans, call `InMemorySpanData::GetSpans()`. Note that each
 * call to `GetSpans()` will clear the previously collected spans.
 */
std::shared_ptr<opentelemetry::exporter::memory::InMemorySpanData>
InstallSpanCatcher();

}  // namespace testing_util
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google
#endif  // GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_TESTING_UTIL_OPENTELEMETRY_MATCHERS_H
