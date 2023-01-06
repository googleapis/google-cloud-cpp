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
#include <opentelemetry/sdk/trace/span_data.h>
#include <memory>
#include <string>

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace testing_util {

using SpanDataPtr = std::unique_ptr<opentelemetry::sdk::trace::SpanData>;

::testing::Matcher<SpanDataPtr> SpanHasInstrumentationScope();

::testing::Matcher<SpanDataPtr> SpanKindIsClient();

::testing::Matcher<SpanDataPtr> SpanNamed(std::string const& name);

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
