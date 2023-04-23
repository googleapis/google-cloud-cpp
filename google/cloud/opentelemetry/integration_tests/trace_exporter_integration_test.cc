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
#include "google/cloud/internal/getenv.h"
#include "google/cloud/internal/opentelemetry.h"
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

using ::testing::IsEmpty;
using ::testing::Not;

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

TEST(TraceExporter, Basic) {
  auto project_id = internal::GetEnv("GOOGLE_CLOUD_PROJECT").value_or("");
  ASSERT_THAT(project_id, Not(IsEmpty()));
  auto project = Project(project_id);
  auto exporter = MakeTraceExporter(project);
  InstallExporter(std::move(exporter));
  auto provider = opentelemetry::trace::Provider::GetTracerProvider();
  auto tracer = provider->GetTracer("gcloud-cpp");

  // Create a test span, which should get exported to Cloud Trace.
  auto const name = "span-" + std::to_string(absl::GetCurrentTimeNanos());
  auto span = tracer->StartSpan(name);
  std::this_thread::sleep_for(std::chrono::milliseconds(2));
  span->End();

  auto trace_client =
      trace_v1::TraceServiceClient(trace_v1::MakeTraceServiceConnection());

  // A GetTraceRequest can only look up traces by trace ID. So we use a
  // ListTracesRequest, which can filter on span names.
  google::devtools::cloudtrace::v1::ListTracesRequest req;
  req.set_project_id(project_id);
  req.set_filter("+root:" + name);

  // Implement a retry loop to wait for the traces to propagate in Cloud Trace.
  for (auto backoff : {1, 2, 4, 8, 16, 32, 0}) {
    ASSERT_NE(backoff, 0) << "Trace did not show up in Cloud Trace";
    auto traces = trace_client.ListTraces(req);
    auto trace = traces.begin();
    if (trace != traces.end()) {
      ASSERT_STATUS_OK(*trace);
      break;
    }
    std::this_thread::sleep_for(std::chrono::seconds(backoff));
  }
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace otel
}  // namespace cloud
}  // namespace google
