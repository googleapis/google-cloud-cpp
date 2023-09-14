// Copyright 2022 Google LLC
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
#include "google/cloud/internal/trace_propagator.h"
#include <gmock/gmock.h>
#include <opentelemetry/trace/default_span.h>
#include <opentelemetry/trace/scope.h>
#include <opentelemetry/trace/span.h>
#include <opentelemetry/trace/span_context.h>

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {
namespace {

using ::testing::_;
using ::testing::IsSupersetOf;
using ::testing::Pair;
using ::testing::UnorderedElementsAre;

class TestCarrier : public opentelemetry::context::propagation::TextMapCarrier {
 public:
  opentelemetry::nostd::string_view Get(
      opentelemetry::nostd::string_view) const noexcept override {
    return {};
  }

  void Set(opentelemetry::nostd::string_view key,
           opentelemetry::nostd::string_view value) noexcept override {
    headers_.insert({{key.data(), key.size()}, {value.data(), value.size()}});
  }

  std::multimap<std::string, std::string> Headers() { return headers_; }

 private:
  std::multimap<std::string, std::string> headers_;
};

TEST(CloudTraceContextPropagator, Inject) {
  using opentelemetry::trace::TraceFlags;

  opentelemetry::trace::TraceId const trace_id(
      std::array<uint8_t const, opentelemetry::trace::TraceId::kSize>(
          {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15}));

  opentelemetry::trace::SpanId const span_id(
      std::array<uint8_t const, opentelemetry::trace::SpanId::kSize>(
          {0, 0, 0, 0, 0, 0, 0, 123}));

  struct TestCase {
    TraceFlags flags;
    std::string value;
  };
  std::vector<TestCase> tests = {{TraceFlags{}, "0"},
                                 {TraceFlags{TraceFlags::kIsSampled}, "1"}};

  for (auto const& t : tests) {
    auto const expected_value =
        "000102030405060708090a0b0c0d0e0f/123;o=" + t.value;

    opentelemetry::trace::TraceFlags{};
    opentelemetry::trace::SpanContext span_context(trace_id, span_id, t.flags,
                                                   false);

    // Create a span from our fake span context, and set it as active. This is
    // the only way to get an `opentelemetry::context::Context` from an
    // `opentelemetry::trace::SpanContext`.
    std::shared_ptr<opentelemetry::trace::Span> span =
        std::make_shared<opentelemetry::trace::DefaultSpan>(span_context);
    opentelemetry::trace::Scope scope{span};

    auto p = MakePropagator();
    TestCarrier carrier;
    p->Inject(carrier, opentelemetry::context::RuntimeContext::GetCurrent());

    EXPECT_THAT(
        carrier.Headers(),
        UnorderedElementsAre(Pair("x-cloud-trace-context", expected_value),
                             Pair("traceparent", _)));
  }
}

TEST(CloudTraceContextPropagator, Fields) {
  std::vector<std::string> keys;
  auto save_keys = [&keys](auto key) {
    keys.emplace_back(key.data(), key.size());
    return true;
  };

  auto p = MakePropagator();
  EXPECT_TRUE(p->Fields(save_keys));

  // We use `IsSupersetOf` instead of `UnorderedElementsAre` because
  // OpenTelemetry's `HttpTraceContext` implementation fires a callback for
  // "tracestate", even when it omits the "tracestate" header.
  EXPECT_THAT(keys, IsSupersetOf({"x-cloud-trace-context", "traceparent"}));
}

}  // namespace
}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google
#endif  // GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY
