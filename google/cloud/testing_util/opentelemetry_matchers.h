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
#include "google/cloud/future.h"
#include "google/cloud/internal/opentelemetry_context.h"
#include "google/cloud/options.h"
#include "google/cloud/version.h"
#include <gmock/gmock.h>
#include <opentelemetry/context/propagation/text_map_propagator.h>
#include <opentelemetry/exporters/memory/in_memory_span_exporter.h>
#include <opentelemetry/sdk/instrumentationscope/instrumentation_scope.h>
#include <opentelemetry/sdk/trace/span_data.h>
#include <opentelemetry/trace/span.h>
#include <opentelemetry/trace/span_metadata.h>
#include <opentelemetry/trace/tracer.h>
#include <opentelemetry/trace/tracer_provider.h>
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

using OTelAttributeMap =
    std::unordered_map<std::string,
                       opentelemetry::sdk::common::OwnedAttributeValue>;

MATCHER_P(SpanAttributesImpl, matcher,
          ::testing::DescribeMatcher<OTelAttributeMap>(matcher)) {
  return ::testing::ExplainMatchResult(matcher, arg->GetAttributes(),
                                       result_listener);
}

MATCHER_P(EventAttributesImpl, matcher,
          ::testing::DescribeMatcher<OTelAttributeMap>(matcher)) {
  return ::testing::ExplainMatchResult(matcher, arg.GetAttributes(),
                                       result_listener);
}

MATCHER_P(LinkAttributesImpl, matcher,
          ::testing::DescribeMatcher<OTelAttributeMap>(matcher)) {
  return ::testing::ExplainMatchResult(matcher, arg.GetAttributes(),
                                       result_listener);
}

}  // namespace testing_util_internal
namespace testing_util {

using SpanDataPtr = std::unique_ptr<opentelemetry::sdk::trace::SpanData>;

std::string ToString(opentelemetry::trace::SpanKind k);

std::string ToString(opentelemetry::trace::StatusCode c);

std::string ToString(opentelemetry::trace::SpanContext const& span_context);

std::string ToString(opentelemetry::trace::SpanId span_id);

// Returns true if there is an active span, as tracked by opentelemetry-cpp.
bool ThereIsAnActiveSpan();

/**
 * Returns true if the current context, as tracked by opentelemetry-cpp, matches
 * the current context, as tracked by google-cloud-cpp.
 *
 * This duplication is necessary for operations that might complete in a
 * different thread than they are created.
 */
bool OTelContextCaptured();

/**
 * Note that all spans created by a `NoopTracerProvider` will compare equal. To
 * avoid this, ensure that a trace exporter is set by the test fixture, e.g. by
 * calling `InstallSpanCatcher()`.
 */
MATCHER(IsActive, "") {
  return opentelemetry::trace::Tracer::GetCurrentSpan() == arg;
}

MATCHER(SpanHasInstrumentationScope,
        "has instrumentation scope (name: gl-cpp | version: " +
            version_string() + ")") {
  auto const& scope = arg->GetInstrumentationScope();
  auto const& name = scope.GetName();
  auto const& version = scope.GetVersion();
  *result_listener << "has instrumentation scope (name: " << name
                   << " | version: " << version << ")";
  return name == "gl-cpp" && version == version_string();
}

MATCHER(SpanKindIsClient,
        "has kind: " + ToString(opentelemetry::trace::SpanKind::kClient)) {
  auto const& kind = arg->GetSpanKind();
  *result_listener << "has kind: " << ToString(kind);
  return kind == opentelemetry::trace::SpanKind::kClient;
}

MATCHER(SpanKindIsInternal,
        "has kind: " + ToString(opentelemetry::trace::SpanKind::kInternal)) {
  auto const& kind = arg->GetSpanKind();
  *result_listener << "has kind: " << ToString(kind);
  return kind == opentelemetry::trace::SpanKind::kInternal;
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

MATCHER_P(SpanWithParent, span,
          "has parent span id: " + ToString(span->GetContext().span_id())) {
  auto const& actual = arg->GetParentSpanId();
  *result_listener << "has parent span id: " << ToString(actual);
  return actual == span->GetContext().span_id();
}

MATCHER(SpanIsRoot, "is root span") {
  auto const actual = arg->GetParentSpanId() == opentelemetry::trace::SpanId();
  *result_listener << "is root span: " << (actual ? "true" : "false");
  return actual;
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

MATCHER(SpanHasNoAttributes, " has no attributes") {
  auto const actual = arg->GetAttributes().size() == 0;
  *result_listener << "has no attributes: " << (actual ? "true" : "false");
  return actual;
}

template <typename T>
::testing::Matcher<
    std::pair<std::string, opentelemetry::sdk::common::OwnedAttributeValue>>
OTelAttribute(std::string const& key, ::testing::Matcher<T const&> matcher) {
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
::testing::Matcher<SpanDataPtr> SpanHasEvents(Args const&... matchers) {
  return SpanEventsAreImpl(::testing::IsSupersetOf({matchers...}));
}

template <typename... Args>
::testing::Matcher<SpanDataPtr> SpanEventsAre(Args const&... matchers) {
  return SpanEventsAreImpl(::testing::ElementsAre(matchers...));
}

MATCHER_P(LinkHasSpanContext, context, "has context" + ToString(context)) {
  auto const& actual = arg.GetSpanContext();
  *result_listener << "has context: " << ToString(actual);
  return actual == context;
}

template <typename... Args>
::testing::Matcher<opentelemetry::sdk::trace::SpanDataLink>
SpanLinkAttributesAre(Args const&... matchers) {
  return testing_util_internal::LinkAttributesImpl(
      ::testing::UnorderedElementsAre(matchers...));
}

MATCHER_P(SpanLinksAreImpl, matcher,
          ::testing::DescribeMatcher<
              std::vector<opentelemetry::sdk::trace::SpanDataLink>>(matcher)) {
  return ::testing::ExplainMatchResult(matcher, arg->GetLinks(),
                                       result_listener);
}

template <typename... Args>
::testing::Matcher<SpanDataPtr> SpanHasLinks(Args const&... matchers) {
  return SpanLinksAreImpl(::testing::IsSupersetOf({matchers...}));
}

template <typename... Args>
::testing::Matcher<SpanDataPtr> SpanLinksAre(Args const&... matchers) {
  return SpanLinksAreImpl(::testing::ElementsAre(matchers...));
}

MATCHER_P(SpanLinksSizeIs, span_links,
          "has size: " + std::to_string(span_links)) {
  auto const& actual = static_cast<std::int64_t>(arg->GetLinks().size());
  *result_listener << "has size: " + std::to_string(actual);
  return actual == span_links;
}

MATCHER_P(EqualsSpanContext, context, "has context" + ToString(context)) {
  *result_listener << "has context: " << ToString(arg);
  return arg == context;
}

class SpanCatcher {
 public:
  SpanCatcher();
  ~SpanCatcher();

  std::vector<std::unique_ptr<opentelemetry::sdk::trace::SpanData>> GetSpans();

 private:
  std::shared_ptr<opentelemetry::exporter::memory::InMemorySpanData> span_data_;
  opentelemetry::nostd::shared_ptr<opentelemetry::trace::TracerProvider>
      previous_;
};

/**
 * Provides access to created spans.
 *
 * Calling this method will install an in-memory trace exporter. It returns a
 * type that provides access to captured spans.
 *
 * To extract the spans, call `SpanCatcher::GetSpans()`. Note that each
 * call to `GetSpans()` will clear the previously collected spans.
 *
 * Also note that this sets the global trace exporter. Thus it is important that
 * the tests within a fixture do not execute in parallel.
 */
std::shared_ptr<SpanCatcher> InstallSpanCatcher();

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

// Returns options with OpenTelemetry tracing enabled. Uses the global tracer
// provider.
Options EnableTracing(Options options);

// Returns options with OpenTelemetry tracing disabled.
Options DisableTracing(Options options);

/**
 * A promise that acts more like an `AsyncGrpcOperation` with respect to
 * `OTelContext`.
 *
 * The context is snapshotted when the future is returned. This is like the
 * constructor for our `AsyncGrpcOperation`s. The context is reinstated when we
 * set the value of the promise. This simulates the conditions of
 * `AsyncGrpcOperation::Notify()`.
 *
 * Use this class to verify that spans do not remain active into the future of
 * an async operation. For example, the following library code...
 *
 * @code
 * TracingConnection::AsyncFoo() {
 *   auto span = MakeSpan("span");
 *   OTelScope scope(span);
 *   using ::opentelemetry::context::RuntimeContext;
 *   return child_->AsyncFoo().then([oc = RuntimeContext::GetCurrent()] {
 *     ...
 *     DetachOTelContext(oc);
 *     return ...;
 *   });
 * }
 * @endcode
 *
 * ... can be tested as follows:
 *
 * @code
 * PromiseWithOTelContext<Response> p;
 * EXPECT_CALL(*mock, AsyncFoo).WillOnce([] {
 *   EXPECT_TRUE(ThereIsAnActiveSpan());
 *   EXPECT_TRUE(OTelContextCaptured());
 *   return p.get_future();
 * });
 * auto f = AsyncFoo().then([](auto f) {
 *   auto t = f.get();
 *   EXPECT_FALSE(ThereIsAnActiveSpan());
 *   EXPECT_FALSE(OTelContextCaptured());
 *   return t;
 * });
 * p.set_value(Response{});
 * EXPECT_THAT(f.get(), ...);
 * @endcode
 */
template <typename T>
class PromiseWithOTelContext {
 public:
  // Return a future as if from an AsyncGrpcOperation constructor
  future<T> get_future() {
    oc_ = internal::CurrentOTelContext();
    return p_.get_future();
  }

  // Satisfy the future as if from an AsyncGrpcOperation::Notify()
  void set_value(T value) {
    internal::ScopedOTelContext scope(oc_);
    p_.set_value(std::move(value));
  }

 private:
  promise<T> p_;
  internal::OTelContext oc_;
};

}  // namespace testing_util
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google
#endif  // GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_TESTING_UTIL_OPENTELEMETRY_MATCHERS_H
