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

#ifdef GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY
#include "google/cloud/testing_util/opentelemetry.h"
#include "absl/memory/memory.h"

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace testing_util {

using ::testing::AllOf;
using ::testing::Matcher;
using ::testing::Pointee;
using ::testing::Property;

Matcher<SpanDataPtr> SpanHasInstrumentationScope() {
  return Pointee(
      Property(&opentelemetry::sdk::trace::SpanData::GetInstrumentationScope,
               AllOf(Property(&opentelemetry::sdk::instrumentationscope::
                                  InstrumentationScope::GetName,
                              "gcloud-cpp"),
                     Property(&opentelemetry::sdk::instrumentationscope::
                                  InstrumentationScope::GetVersion,
                              version_string()))));
}

Matcher<SpanDataPtr> SpanKindIsClient() {
  return Pointee(Property(&opentelemetry::sdk::trace::SpanData::GetSpanKind,
                          opentelemetry::trace::SpanKind::kClient));
}

Matcher<SpanDataPtr> SpanNamed(std::string const& name) {
  return Pointee(Property(&opentelemetry::sdk::trace::SpanData::GetName, name));
}

OpenTelemetryTest::OpenTelemetryTest() {
  auto exporter = absl::make_unique<
      opentelemetry::exporter::memory::InMemorySpanExporter>();
  span_data_ = exporter->GetData();

  auto processor =
      absl::make_unique<opentelemetry::sdk::trace::SimpleSpanProcessor>(
          std::move(exporter));
  std::shared_ptr<opentelemetry::trace::TracerProvider> provider =
      opentelemetry::sdk::trace::TracerProviderFactory::Create(
          std::move(processor));
  opentelemetry::trace::Provider::SetTracerProvider(provider);
}

}  // namespace testing_util
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google
#endif  // GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY
