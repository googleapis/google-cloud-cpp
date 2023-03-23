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
#include "google/cloud/testing_util/opentelemetry_matchers.h"
#include <opentelemetry/context/propagation/global_propagator.h>
#include <opentelemetry/sdk/trace/simple_processor.h>
#include <opentelemetry/sdk/trace/tracer.h>
#include <opentelemetry/sdk/trace/tracer_provider_factory.h>
#include <opentelemetry/trace/provider.h>

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace testing_util {

std::string ToString(opentelemetry::trace::SpanKind k) {
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
      return "CONSUMER";
    default:
      return "UNKNOWN";
  }
}

std::string ToString(opentelemetry::trace::StatusCode c) {
  switch (c) {
    case opentelemetry::trace::StatusCode::kError:
      return "ERROR";
    case opentelemetry::trace::StatusCode::kOk:
      return "OK";
    case opentelemetry::trace::StatusCode::kUnset:
    default:
      return "UNSET";
  }
}

bool ThereIsAnActiveSpan() {
  return opentelemetry::trace::Tracer::GetCurrentSpan()->GetContext().IsValid();
}

std::shared_ptr<opentelemetry::exporter::memory::InMemorySpanData>
InstallSpanCatcher() {
  auto exporter =
      std::make_unique<opentelemetry::exporter::memory::InMemorySpanExporter>();
  auto span_data = exporter->GetData();
  auto processor =
      std::make_unique<opentelemetry::sdk::trace::SimpleSpanProcessor>(
          std::move(exporter));
  std::shared_ptr<opentelemetry::trace::TracerProvider> provider =
      opentelemetry::sdk::trace::TracerProviderFactory::Create(
          std::move(processor));
  opentelemetry::trace::Provider::SetTracerProvider(provider);
  return span_data;
}

std::shared_ptr<MockTextMapPropagator> InstallMockPropagator() {
  namespace propagation = opentelemetry::context::propagation;
  auto mock = std::make_shared<MockTextMapPropagator>();
  propagation::GlobalTextMapPropagator::SetGlobalPropagator(
      std::shared_ptr<propagation::TextMapPropagator>(mock));
  return mock;
}

}  // namespace testing_util
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google
#endif  // GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY
