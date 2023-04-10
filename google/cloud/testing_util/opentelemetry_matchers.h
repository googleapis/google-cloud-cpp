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
#include <opentelemetry/context/propagation/text_map_propagator.h>
#include <opentelemetry/exporters/memory/in_memory_span_exporter.h>
#include <opentelemetry/sdk/instrumentationscope/instrumentation_scope.h>
#include <opentelemetry/sdk/trace/span_data.h>
#include <opentelemetry/trace/span.h>
#include <opentelemetry/trace/span_metadata.h>
#include <opentelemetry/trace/tracer.h>
#include <iosfwd>
#include <memory>
#include <string>

/**
 * Provide `opentelemetry::sdk::trace::SpanData` output streaming.
 *
 * Generally not a good idea to open a namespace outside our control, but it
 * works, and -- in this test-only case -- is well worth it.
 */
OPENTELEMETRY_BEGIN_NAMESPACE
namespace sdk {
namespace trace {

// Make the output from googletest human readable.
std::ostream& operator<<(std::ostream&, SpanData const&);

}  // namespace trace
}  // namespace sdk
OPENTELEMETRY_END_NAMESPACE

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace testing_util_internal {

using SpanAttributeMap =
    std::unordered_map<std::string,
                       opentelemetry::sdk::common::OwnedAttributeValue>;
MATCHER_P(SpanAttributesImpl, matcher,
          ::testing::DescribeMatcher<SpanAttributeMap>(matcher)) {
  return ::testing::ExplainMatchResult(matcher, arg->GetAttributes(),
                                       result_listener);
}

MATCHER_P(EventAttributesImpl, matcher,
          ::testing::DescribeMatcher<SpanAttributeMap>(matcher)) {
  return ::testing::ExplainMatchResult(matcher, arg.GetAttributes(),
                                       result_listener);
}

}  // namespace testing_util_internal
namespace testing_util {

using SpanDataPtr = std::unique_ptr<opentelemetry::sdk::trace::SpanData>;

std::string ToString(opentelemetry::trace::SpanKind k);

std::string ToString(opentelemetry::trace::StatusCode c);

bool ThereIsAnActiveSpan();

/**
 * Note that all spans created by a `NoopTracerProvider` will compare equal. To
 * avoid this, ensure that a trace exporter is set by the test fixture, e.g. by
 * calling `InstallSpanCatcher()`.
 */
MATCHER(IsActive, "") {
  return opentelemetry::trace::Tracer::GetCurrentSpan() == arg;
}

MATCHER(SpanHasInstrumentationScope,
        "has instrumentation scope (name: gcloud-cpp | version: " +
            version_string() + ")") {
  auto const& scope = arg->GetInstrumentationScope();
  auto const& name = scope.GetName();
  auto const& version = scope.GetVersion();
  *result_listener << "has instrumentation scope (name: " << name
                   << " | version: " << version << ")";
  return name == "gcloud-cpp" && version == version_string();
}

MATCHER(SpanKindIsClient,
        "has kind: " + ToString(opentelemetry::trace::SpanKind::kClient)) {
  auto const& kind = arg->GetSpanKind();
  *result_listener << "has kind: " << ToString(kind);
  return kind == opentelemetry::trace::SpanKind::kClient;
}

MATCHER(SpanKindIsConsumer,
        "has kind: " + ToString(opentelemetry::trace::SpanKind::kConsumer)) {
  auto const& kind = arg->GetSpanKind();
  *result_listener << "has kind: " << ToString(kind);
  return kind == opentelemetry::trace::SpanKind::kConsumer;
}

MATCHER(SpanKindIsProducer,
        "has kind: " + ToString(opentelemetry::trace::SpanKind::kProducer)) {
  auto const& kind = arg->GetSpanKind();
  *result_listener << "has kind: " << ToString(kind);
  return kind == opentelemetry::trace::SpanKind::kProducer;
}

MATCHER_P(SpanNamed, name, "has name: " + std::string{name}) {
  auto const& actual = arg->GetName();
  *result_listener << "has name: " << actual;
  return actual == name;
}

MATCHER_P(SpanWithStatus, status, "has status: " + ToString(status)) {
  auto const& actual = arg->GetStatus();
  *result_listener << "has status: " << ToString(actual);
  return actual == status;
}

MATCHER_P2(SpanWithStatus, status, description,
           "has (status: " + ToString(status) +
               " | description: " + description + ")") {
  auto const& s = arg->GetStatus();
  auto const& d = arg->GetDescription();
  *result_listener << "has (status: " << ToString(s) << " | description: " << d
                   << ")";
  return s == status && d == description;
}

template <typename... Args>
::testing::Matcher<SpanDataPtr> SpanHasAttributes(Args const&... matchers) {
  return testing_util_internal::SpanAttributesImpl(
      ::testing::IsSupersetOf({matchers...}));
}

template <typename T>
::testing::Matcher<
    std::pair<std::string, opentelemetry::sdk::common::OwnedAttributeValue>>
SpanAttribute(std::string const& key, ::testing::Matcher<T const&> matcher) {
  return ::testing::Pair(key, ::testing::VariantWith<T>(matcher));
}

MATCHER_P(EventNamed, name, "has name: " + std::string{name}) {
  auto const& actual = arg.GetName();
  *result_listener << "has name: " << actual;
  return actual == name;
}

template <typename... Args>
::testing::Matcher<opentelemetry::sdk::trace::SpanDataEvent>
SpanEventAttributesAre(Args const&... matchers) {
  return testing_util_internal::EventAttributesImpl(
      ::testing::UnorderedElementsAre(matchers...));
}

MATCHER_P(SpanEventsAreImpl, matcher,
          ::testing::DescribeMatcher<
              std::vector<opentelemetry::sdk::trace::SpanDataEvent>>(matcher)) {
  return ::testing::ExplainMatchResult(matcher, arg->GetEvents(),
                                       result_listener);
}

template <typename... Args>
::testing::Matcher<SpanDataPtr> SpanEventsAre(Args const&... matchers) {
  return SpanEventsAreImpl(::testing::ElementsAre(matchers...));
}

/**
 * Provides access to created spans.
 *
 * Calling this method will install an in-memory trace exporter. It returns a
 * type that provides access to captured spans.
 *
 * To extract the spans, call `InMemorySpanData::GetSpans()`. Note that each
 * call to `GetSpans()` will clear the previously collected spans.
 *
 * Also note that this sets the global trace exporter, which will persist from
 * one test in a test fixture to the next. Thus it is important that:
 * 1. a new exporter is installed for each test
 * 2. the tests within a fixture do not execute in parallel
 */
std::shared_ptr<opentelemetry::exporter::memory::InMemorySpanData>
InstallSpanCatcher();

class MockTextMapPropagator
    : public opentelemetry::context::propagation::TextMapPropagator {
 public:
  explicit MockTextMapPropagator() = default;

  // NOLINTNEXTLINE(bugprone-exception-escape)
  MOCK_METHOD(opentelemetry::context::Context, Extract,
              (opentelemetry::context::propagation::TextMapCarrier const&,
               opentelemetry::context::Context&),
              (noexcept, override));

  // NOLINTNEXTLINE(bugprone-exception-escape)
  MOCK_METHOD(void, Inject,
              (opentelemetry::context::propagation::TextMapCarrier&,
               opentelemetry::context::Context const&),
              (noexcept, override));

  // NOLINTNEXTLINE(bugprone-exception-escape)
  MOCK_METHOD(bool, Fields,
              (opentelemetry::nostd::function_ref<
                  bool(opentelemetry::nostd::string_view)>
                   callback),
              (const, noexcept, override));
};

/**
 * Note that this sets the global context propagator, which will persist from
 * one test in a test fixture to the next. Thus it is important that:
 * 1. a new propagator is installed for each test
 * 2. the tests within a fixture do not execute in parallel
 */
std::shared_ptr<MockTextMapPropagator> InstallMockPropagator();

}  // namespace testing_util
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google
#endif  // GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_TESTING_UTIL_OPENTELEMETRY_MATCHERS_H
