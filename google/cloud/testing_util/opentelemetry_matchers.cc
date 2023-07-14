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
#include "google/cloud/internal/absl_str_join_quiet.h"
#include "google/cloud/opentelemetry_options.h"
#include <opentelemetry/context/propagation/global_propagator.h>
#include <opentelemetry/sdk/trace/simple_processor.h>
#include <opentelemetry/sdk/trace/tracer.h>
#include <opentelemetry/sdk/trace/tracer_provider_factory.h>
#include <opentelemetry/trace/provider.h>

namespace {
void AttributeFormatter(
    std::string* out,
    std::pair<std::string const,
              opentelemetry::sdk::common::OwnedAttributeValue> const& kv) {
  *out += kv.first;
  *out += "=";
  struct Visitor {
    std::string* out;
    void operator()(bool v) const {
      *out += "bool:";
      *out += v ? "true" : "false";
    }
    void operator()(double v) const { *out += "double:" + std::to_string(v); }
    void operator()(std::int32_t v) const {
      *out += "std::int32_t:" + std::to_string(v);
    }
    void operator()(std::uint32_t v) const {
      *out += "std::uint32_t:" + std::to_string(v);
    }
    void operator()(std::int64_t v) const {
      *out += "std::int64_t:" + std::to_string(v);
    }
    void operator()(std::uint64_t v) const {
      *out += "std::uint64_t:" + std::to_string(v);
    }
    void operator()(std::string const& v) const { *out += "std::string:" + v; }
    void operator()(std::vector<bool> const& v) const {
      auto format = [](std::string* out, bool b) {
        *out += b ? "true" : "false";
      };
      *out += "std::vector<bool>:[" + absl::StrJoin(v, ", ", format) + "]";
    }
    void operator()(std::vector<double> const& v) const {
      *out += "std::vector<double>:[" + absl::StrJoin(v, ", ") + "]";
    }
    void operator()(std::vector<std::string> const& v) const {
      *out += "std::vector<std::string>:[" + absl::StrJoin(v, ", ") + "]";
    }
    void operator()(std::vector<std::uint8_t> const& v) const {
      *out += "std::vector<std::uint8_t>:[" + absl::StrJoin(v, ", ") + "]";
      ;
    }
    void operator()(std::vector<std::int32_t> const& v) const {
      *out += "std::vector<std::int32_t>:[" + absl::StrJoin(v, ", ") + "]";
    }
    void operator()(std::vector<std::uint32_t> const& v) const {
      *out += "std::vector<std::uint32_t>:[" + absl::StrJoin(v, ", ") + "]";
    }
    void operator()(std::vector<std::int64_t> const& v) const {
      *out += "std::vector<std::int64_t>:[" + absl::StrJoin(v, ", ") + "]";
    }
    void operator()(std::vector<std::uint64_t> const& v) const {
      *out += "std::vector<std::uint64_t>:[" + absl::StrJoin(v, ", ") + "]";
    }
  };
  absl::visit(Visitor{out}, kv.second);
}
}  // namespace

OPENTELEMETRY_BEGIN_NAMESPACE
namespace sdk {
namespace trace {

std::ostream& operator<<(std::ostream& os, SpanData const& rhs) {
  os << "Span {name=" << rhs.GetName()
     << ", kind=" << google::cloud::testing_util::ToString(rhs.GetSpanKind())
     << ", instrumentation_scope {" << rhs.GetInstrumentationScope().GetName()
     << ", " << rhs.GetInstrumentationScope().GetVersion() << "}, attributes=["
     << absl::StrJoin(rhs.GetAttributes(), ", ", AttributeFormatter)
     << "], events=[";
  char const* sep = " ";
  for (auto const& e : rhs.GetEvents()) {
    os << sep << "Event {name=" << e.GetName() << ", attributes=["
       << absl::StrJoin(e.GetAttributes(), ", ", AttributeFormatter) << "]}";
    sep = ", ";
  }
  return os << "]}";
}

}  // namespace trace
}  // namespace sdk
OPENTELEMETRY_END_NAMESPACE

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

SpanCatcher::SpanCatcher()
    : previous_(opentelemetry::trace::Provider::GetTracerProvider()) {
  auto exporter =
      std::make_unique<opentelemetry::exporter::memory::InMemorySpanExporter>();
  span_data_ = exporter->GetData();
  auto processor =
      std::make_unique<opentelemetry::sdk::trace::SimpleSpanProcessor>(
          std::move(exporter));
  std::shared_ptr<opentelemetry::trace::TracerProvider> provider =
      opentelemetry::sdk::trace::TracerProviderFactory::Create(
          std::move(processor));
  opentelemetry::trace::Provider::SetTracerProvider(provider);
}

SpanCatcher::~SpanCatcher() {
  opentelemetry::trace::Provider::SetTracerProvider(std::move(previous_));
}

std::vector<std::unique_ptr<opentelemetry::sdk::trace::SpanData>>
SpanCatcher::GetSpans() {
  return span_data_->GetSpans();
}

std::shared_ptr<SpanCatcher> InstallSpanCatcher() {
  return std::make_shared<SpanCatcher>();
}

std::shared_ptr<MockTextMapPropagator> InstallMockPropagator() {
  namespace propagation = opentelemetry::context::propagation;
  auto mock = std::make_shared<MockTextMapPropagator>();
  propagation::GlobalTextMapPropagator::SetGlobalPropagator(
      std::shared_ptr<propagation::TextMapPropagator>(mock));
  return mock;
}

Options EnableTracing(Options options) {
  return options.set<experimental::OpenTelemetryTracingOption>(true);
}

Options DisableTracing(Options options) {
  return options.set<experimental::OpenTelemetryTracingOption>(false);
}

}  // namespace testing_util
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google
#endif  // GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY
