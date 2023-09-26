// Copyright 2023 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "google/cloud/opentelemetry/configure_basic_tracing.h"
#include "google/cloud/opentelemetry/resource_detector.h"
#include "google/cloud/opentelemetry/trace_exporter.h"
#include <opentelemetry/sdk/trace/batch_span_processor.h>
#include <opentelemetry/sdk/trace/batch_span_processor_options.h>
#include <opentelemetry/sdk/trace/samplers/trace_id_ratio_factory.h>
#include <opentelemetry/sdk/trace/tracer_provider.h>
#include <opentelemetry/trace/provider.h>

namespace google {
namespace cloud {
namespace otel {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

class BasicTracingConfigurationImpl : public BasicTracingConfiguration {
 public:
  explicit BasicTracingConfigurationImpl(
      std::shared_ptr<opentelemetry::sdk::trace::TracerProvider> provider)
      : provider_(std::move(provider)),
        previous_(opentelemetry::trace::Provider::GetTracerProvider()) {
    opentelemetry::trace::Provider::SetTracerProvider(
        opentelemetry::nostd::shared_ptr<opentelemetry::trace::TracerProvider>(
            provider_));
  }

  ~BasicTracingConfigurationImpl() override {
    if (provider_) provider_->ForceFlush(std::chrono::microseconds(1000));
    // Reset the global tracer provider.
    opentelemetry::trace::Provider::SetTracerProvider(std::move(previous_));
  }

 private:
  std::shared_ptr<opentelemetry::sdk::trace::TracerProvider> provider_;
  opentelemetry::nostd::shared_ptr<opentelemetry::trace::TracerProvider>
      previous_;
};

}  // namespace

std::unique_ptr<BasicTracingConfiguration> ConfigureBasicTracing(
    Project project, Options options) {
  // Just return a nullptr if the project is not configured. This is intended
  // as a function to make things easy, no reason to return complicated errors.
  if (project.project_id().empty()) return {};
  auto ratio = options.has<BasicTracingRateOption>()
                   ? options.get<BasicTracingRateOption>()
                   : 1.0;
  auto detector = MakeResourceDetector();
  auto processor =
      std::make_unique<opentelemetry::sdk::trace::BatchSpanProcessor>(
          MakeTraceExporter(std::move(project), std::move(options)),
          opentelemetry::sdk::trace::BatchSpanProcessorOptions{});
  auto provider = std::make_shared<opentelemetry::sdk::trace::TracerProvider>(
      std::move(processor), detector->Detect(),
      opentelemetry::sdk::trace::TraceIdRatioBasedSamplerFactory::Create(
          ratio));
  return std::make_unique<BasicTracingConfigurationImpl>(std::move(provider));
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace otel
}  // namespace cloud
}  // namespace google
