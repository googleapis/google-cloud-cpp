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
#include "google/cloud/opentelemetry/trace_exporter.h"
#include "google/cloud/internal/getenv.h"
#include "google/cloud/testing_util/example_driver.h"
#include <opentelemetry/sdk/trace/batch_span_processor_factory.h>
#include <opentelemetry/sdk/trace/batch_span_processor_options.h>
#include <opentelemetry/sdk/trace/processor.h>
#include <opentelemetry/sdk/trace/tracer_provider_factory.h>
#include <opentelemetry/trace/provider.h>
#include <string>
#include <vector>

namespace {
namespace examples = ::google::cloud::testing_util;
using ::google::cloud::internal::GetEnv;

std::pair<std::string, examples::CommandType> MakeExample(
    std::string const& name, examples::CommandType const& command) {
  auto adapter = [&](std::vector<std::string> const& argv) {
    if (argv.size() != 1 || argv[0] == "--help") {
      throw examples::Usage{name + " <project-id>"};
    }
    command(argv);
  };
  return {name, adapter};
};

void MakeSpan(std::string const& name) {
  auto provider = opentelemetry::trace::Provider::GetTracerProvider();
  auto tracer = provider->GetTracer("gcloud-cpp/otel-samples");
  tracer->StartSpan(name)->End();
}

class Client {
 public:
  // NOLINTNEXTLINE(readability-convert-member-functions-to-static)
  void CreateFoo() { MakeSpan("CreateFoo()"); }
  // NOLINTNEXTLINE(readability-convert-member-functions-to-static)
  void DeleteFoo() { MakeSpan("DeleteFoo()"); }
};

void MyApplicationCode() { MakeSpan("otel-samples"); }

void BasicTracing(std::vector<std::string> const& argv) {
  //! [otel-basic-tracing] [START trace_exporter_basic_tracing]
  namespace gc = ::google::cloud;
  [](std::string project_id) {
    auto project = gc::Project(std::move(project_id));
    auto configuration = gc::otel::ConfigureBasicTracing(project);

    MyApplicationCode();
  }
  //! [otel-basic-tracing] [END trace_exporter_basic_tracing]
  (argv.at(0));
}

void BasicTracingRate(std::vector<std::string> const& argv) {
  //! [otel-basic-tracing-rate] [START trace_exporter_basic_tracing_rate]
  namespace gc = ::google::cloud;
  [](std::string project_id) {
    auto project = gc::Project(std::move(project_id));
    auto options = gc::Options{}.set<gc::otel::BasicTracingRateOption>(.001);
    auto configuration = gc::otel::ConfigureBasicTracing(project, options);

    MyApplicationCode();
  }
  //! [otel-basic-tracing-rate] [END trace_exporter_basic_tracing_rate]
  (argv.at(0));
}

void InstrumentedApplication(std::vector<std::string> const& argv) {
  //! [otel-instrumented-application]
  //! [START trace_exporter_instrumented_application]
  // For more details on the OpenTelemetry code in this sample, see:
  //     https://opentelemetry.io/docs/instrumentation/cpp/manual/
  namespace gc = ::google::cloud;
  using ::opentelemetry::trace::Scope;
  [](std::string project_id) {
    auto project = gc::Project(std::move(project_id));
    auto configuration = gc::otel::ConfigureBasicTracing(project);

    // Initialize the `Tracer`. This would typically be done once.
    auto provider = opentelemetry::trace::Provider::GetTracerProvider();
    auto tracer = provider->GetTracer("my-application");

    // If your application makes multiple client calls that are logically
    // connected, you may want to instrument your application.
    auto my_function = [tracer] {
      // Start an active span. The span is ended when the `Scope` object is
      // destroyed.
      auto scope = Scope(tracer->StartSpan("my-function-span"));

      // Any spans created by the client library will be children of
      // "my-function-span". i.e. In the distributed trace, the client calls are
      // sub-units of work of `my_function()`, and will be displayed as such in
      // Cloud Trace.
      Client client;
      client.CreateFoo();
      client.DeleteFoo();
    };

    // As an example, start a span to cover both calls to `my_function()`.
    auto scope = Scope(tracer->StartSpan("my-application-span"));
    my_function();
    my_function();
  }
  //! [END trace_exporter_instrumented_application]
  //! [otel-instrumented-application]
  (argv.at(0));
}

void CustomTracerProvider(std::vector<std::string> const& argv) {
  //! [otel-custom-tracer-provider]
  //! [START trace_exporter_custom_tracer_provider]
  namespace gc = ::google::cloud;
  using ::opentelemetry::trace::Scope;
  [](std::string project_id) {
    // Use the Cloud Trace Exporter directly.
    auto project = gc::Project(std::move(project_id));
    auto exporter = gc::otel::MakeTraceExporter(project);

    // Advanced use cases may need to create their own tracer provider, e.g. to
    // export to Cloud Trace and another backend simultaneously. In this
    // example, we just tweak some OpenTelemetry settings that google-cloud-cpp
    // does not expose.
    opentelemetry::sdk::trace::BatchSpanProcessorOptions options;
    options.schedule_delay_millis = std::chrono::milliseconds(1000);
    auto processor =
        opentelemetry::sdk::trace::BatchSpanProcessorFactory::Create(
            std::move(exporter), options);
    auto provider = opentelemetry::sdk::trace::TracerProviderFactory::Create(
        std::move(processor));

    // Set the global trace provider
    std::shared_ptr<opentelemetry::trace::TracerProvider> api_provider =
        std::move(provider);
    opentelemetry::trace::Provider::SetTracerProvider(std::move(api_provider));

    MyApplicationCode();

    // Clear the global trace provider
    opentelemetry::trace::Provider::SetTracerProvider(
        opentelemetry::nostd::shared_ptr<
            opentelemetry::trace::TracerProvider>());
  }
  //! [END trace_exporter_custom_tracer_provider]
  //! [otel-custom-tracer-provider]
  (argv.at(0));
}

void AutoRun(std::vector<std::string> const& argv) {
  if (!argv.empty()) throw examples::Usage{"auto"};
  examples::CheckEnvironmentVariablesAreSet({
      "GOOGLE_CLOUD_PROJECT",
  });
  auto project_id = GetEnv("GOOGLE_CLOUD_PROJECT").value();

  std::cout << "\nRunning BasicTracing() sample" << std::endl;
  BasicTracing({project_id});

  std::cout << "\nRunning BasicTracingRate() sample" << std::endl;
  BasicTracingRate({project_id});

  std::cout << "\nRunning InstrumentedApplication() sample" << std::endl;
  InstrumentedApplication({project_id});

  std::cout << "\nRunning CustomTracerProvider() sample" << std::endl;
  CustomTracerProvider({project_id});

  std::cout << "\nAutoRun done" << std::endl;
}

}  // namespace

int main(int argc, char* argv[]) {  // NOLINT(bugprone-exception-escape)
  examples::Example example({
      MakeExample("basic-tracing", BasicTracing),
      MakeExample("basic-tracing-rate", BasicTracingRate),
      MakeExample("instrumented-application", InstrumentedApplication),
      MakeExample("custom-tracer-provider", CustomTracerProvider),
      {"auto", AutoRun},
  });
  return example.Run(argc, argv);
}
