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

#include "google/cloud/opentelemetry/configure_basic_tracing.h"
#include "google/cloud/trace/v1/trace_client.h"
#include "google/cloud/internal/getenv.h"
#include "google/cloud/internal/opentelemetry.h"
#include "google/cloud/internal/random.h"
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

using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::IsEmpty;
using ::testing::Matcher;
using ::testing::Not;
using ::testing::ResultOf;

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

TEST(ConfigureBasicTracing, Basic) {
  auto project_id = internal::GetEnv("GOOGLE_CLOUD_PROJECT").value_or("");
  ASSERT_THAT(project_id, Not(IsEmpty()));

  // Create a basic tracing configuration.
  auto project = Project(project_id);
  auto configuration = ConfigureBasicTracing(project);

  // Create a span.
  auto provider = opentelemetry::trace::Provider::GetTracerProvider();
  auto tracer = provider->GetTracer("gcloud-cpp");

  // Create a test span, which should get exported to Cloud Trace.
  auto generator = google::cloud::internal::MakeDefaultPRNG();
  auto const name =
      "span-" + google::cloud::internal::Sample(generator, 32, "0123456789");

  auto span = tracer->StartSpan(name);
  opentelemetry::trace::StartSpanOptions options;
  options.kind = opentelemetry::trace::SpanKind::kClient;
  auto const elapsed = std::chrono::milliseconds(20);
  options.start_steady_time = std::chrono::steady_clock::now() - elapsed;
  options.start_system_time = std::chrono::system_clock::now() - elapsed;
  span->End();
  // Flush the data.
  configuration.reset();

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

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace otel
}  // namespace cloud
}  // namespace google
