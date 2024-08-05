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

#include "google/cloud/opentelemetry/trace_exporter.h"
#include "google/cloud/trace/v1/trace_client.h"
#include "google/cloud/common_options.h"
#include "google/cloud/credentials.h"
#include "google/cloud/internal/getenv.h"
#include "google/cloud/internal/opentelemetry.h"
#include "google/cloud/testing_util/opentelemetry_matchers.h"
#include "google/cloud/testing_util/scoped_environment.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>
#include <opentelemetry/sdk/trace/simple_processor.h>
#include <opentelemetry/sdk/trace/tracer.h>
#include <opentelemetry/sdk/trace/tracer_provider_factory.h>
#include <opentelemetry/trace/provider.h>
#include <opentelemetry/trace/span.h>
#include <opentelemetry/trace/tracer.h>

namespace google {
namespace cloud {
namespace otel {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::google::cloud::testing_util::InstallSpanCatcher;
using ::google::cloud::testing_util::ScopedEnvironment;
using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::IsEmpty;
using ::testing::Matcher;
using ::testing::Not;
using ::testing::ResultOf;

// Installs a TracerProvider that:
// - uses the given exporter
// - flushes spans individually as they are ended
void InstallExporter(
    std::unique_ptr<opentelemetry::sdk::trace::SpanExporter> exporter) {
  auto processor =
      std::make_unique<opentelemetry::sdk::trace::SimpleSpanProcessor>(
          std::move(exporter));
  std::shared_ptr<opentelemetry::trace::TracerProvider> provider =
      opentelemetry::sdk::trace::TracerProviderFactory::Create(
          std::move(processor));
  opentelemetry::trace::Provider::SetTracerProvider(provider);
}

std::string TraceId(opentelemetry::trace::SpanContext const& span_context) {
  std::array<char, 2 * opentelemetry::trace::TraceId::kSize> trace_id;
  span_context.trace_id().ToLowerBase16(trace_id);
  return std::string{trace_id.data(), trace_id.size()};
}

Matcher<google::devtools::cloudtrace::v1::TraceSpan const&> TraceSpan(
    std::string const& name) {
  return ResultOf(
      "name",
      [](google::devtools::cloudtrace::v1::TraceSpan const& s) {
        return s.name();
      },
      Eq(name));
}

std::string RandomSpanName() {
  auto generator = internal::MakeDefaultPRNG();
  return "span-" + internal::Sample(generator, 32, "0123456789");
}

TEST(TraceExporter, Basic) {
  auto project_id = internal::GetEnv("GOOGLE_CLOUD_PROJECT").value_or("");
  ASSERT_THAT(project_id, Not(IsEmpty()));

  auto project = Project(project_id);
  auto exporter = MakeTraceExporter(project);
  InstallExporter(std::move(exporter));

  // Create a test span using the global TracerProvider. It should get exported
  // to Cloud Trace.
  auto provider = opentelemetry::trace::Provider::GetTracerProvider();
  auto tracer = provider->GetTracer("gcloud-cpp");
  auto const name = RandomSpanName();
  auto span = tracer->StartSpan(name);
  span->End();

  auto trace_client =
      trace_v1::TraceServiceClient(trace_v1::MakeTraceServiceConnection());

  google::devtools::cloudtrace::v1::GetTraceRequest req;
  req.set_project_id(project_id);
  req.set_trace_id(TraceId(span->GetContext()));

  // Implement a retry loop to wait for the traces to propagate in Cloud Trace.
  StatusOr<google::devtools::cloudtrace::v1::Trace> trace;
  for (auto backoff : {10, 60, 120, 120}) {
    // Because we are limited by quota, start with a backoff.
    std::this_thread::sleep_for(std::chrono::seconds(backoff));

    trace = trace_client.GetTrace(req);
    if (trace.ok()) break;
  }
  ASSERT_STATUS_OK(trace) << "Trace did not show up in Cloud Trace";
  EXPECT_THAT(trace->spans(), ElementsAre(TraceSpan(name)));
}

TEST(TraceExporter, NoInfiniteExportLoop14611) {
  auto span_catcher = InstallSpanCatcher();

  ScopedEnvironment env{"GOOGLE_CLOUD_CPP_OPENTELEMETRY_TRACING", "ON"};

  auto project = Project("test-project");
  auto options = Options{}
                     .set<EndpointOption>("localhost:1")
                     .set<UnifiedCredentialsOption>(MakeInsecureCredentials());
  auto exporter = MakeTraceExporter(project, options);

  // Simulate an export which should not create any additional spans.
  auto recordable = exporter->MakeRecordable();
  recordable->SetName("span");
  (void)exporter->Export({&recordable, 1});

  // Verify that no spans were created.
  EXPECT_THAT(span_catcher->GetSpans(), IsEmpty());
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace otel
}  // namespace cloud
}  // namespace google
