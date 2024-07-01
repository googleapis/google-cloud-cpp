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

#include "google/cloud/internal/opentelemetry.h"
#include "google/cloud/internal/make_status.h"
#include "google/cloud/opentelemetry_options.h"
#include "google/cloud/testing_util/opentelemetry_matchers.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>
#ifdef GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY
#include <opentelemetry/trace/default_span.h>
#include <opentelemetry/version.h>
#endif  // GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {
namespace {

using ms = std::chrono::milliseconds;
using ::testing::MockFunction;

#ifdef GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY
using ::google::cloud::testing_util::DisableTracing;
using ::google::cloud::testing_util::EnableTracing;
using ::google::cloud::testing_util::InstallSpanCatcher;
using ::google::cloud::testing_util::LinkHasSpanContext;
using ::google::cloud::testing_util::OTelAttribute;
using ::google::cloud::testing_util::SpanHasAttributes;
using ::google::cloud::testing_util::SpanHasInstrumentationScope;
using ::google::cloud::testing_util::SpanHasLinks;
using ::google::cloud::testing_util::SpanKindIsClient;
using ::google::cloud::testing_util::SpanKindIsProducer;
using ::google::cloud::testing_util::SpanLinkAttributesAre;
using ::google::cloud::testing_util::SpanNamed;
using ::google::cloud::testing_util::SpanWithParent;
using ::google::cloud::testing_util::SpanWithStatus;
using ::testing::AllOf;
using ::testing::Contains;
using ::testing::Each;
using ::testing::ElementsAre;
using ::testing::IsEmpty;
using ::testing::Not;

TEST(OpenTelemetry, IsUsable) {
  auto version = std::string{OPENTELEMETRY_VERSION};
  EXPECT_THAT(version, Not(IsEmpty()));
  auto span = opentelemetry::trace::DefaultSpan::GetInvalid();
  EXPECT_EQ(span.ToString(), "DefaultSpan");
}

TEST(OpenTelemetry, GetTracer) {
  auto span_catcher = InstallSpanCatcher();

  auto tracer = GetTracer(Options{});

  auto s1 = tracer->StartSpan("span");
  s1->End();

  auto spans = span_catcher->GetSpans();
  EXPECT_THAT(spans, ElementsAre(SpanHasInstrumentationScope()));
}

TEST(OpenTelemetry, MakeSpan) {
  auto span_catcher = InstallSpanCatcher();

  auto s1 = MakeSpan("span1");
  s1->End();
  auto s2 = MakeSpan("span2");
  s2->End();

  auto spans = span_catcher->GetSpans();
  EXPECT_THAT(spans, Each(SpanHasInstrumentationScope()));
  EXPECT_THAT(spans, Each(SpanKindIsClient()));
  EXPECT_THAT(spans, ElementsAre(SpanNamed("span1"), SpanNamed("span2")));
}

TEST(OpenTelemetry, MakeSpanWithAttributes) {
  auto span_catcher = InstallSpanCatcher();

  auto s1 = MakeSpan("span1", {{"key", "value"}});
  s1->End();

  auto spans = span_catcher->GetSpans();
  EXPECT_THAT(spans, Each(SpanHasInstrumentationScope()));
  EXPECT_THAT(spans, Each(SpanKindIsClient()));
  EXPECT_THAT(spans, ElementsAre(SpanNamed("span1")));
  EXPECT_THAT(spans, ElementsAre(SpanHasAttributes(
                         OTelAttribute<std::string>("key", "value"))));
}

TEST(OpenTelemetry, MakeSpanWithLink) {
  auto span_catcher = InstallSpanCatcher();

  auto s1 = MakeSpan("span1");
  auto s2 = MakeSpan("span2", {}, {{s1->GetContext(), {{"key", "value"}}}});
  s1->End();
  s2->End();

  auto spans = span_catcher->GetSpans();
  EXPECT_THAT(
      spans,
      Contains(AllOf(
          SpanNamed("span2"),
          SpanHasLinks(AllOf(LinkHasSpanContext(s1->GetContext()),
                             SpanLinkAttributesAre(OTelAttribute<std::string>(
                                 "key", "value")))))));
}

TEST(OpenTelemetry, MakeSpanWithKind) {
  auto span_catcher = InstallSpanCatcher();

  opentelemetry::trace::StartSpanOptions options;
  options.kind = opentelemetry::trace::SpanKind::kProducer;
  auto s1 = MakeSpan("span1", options);
  s1->End();

  auto spans = span_catcher->GetSpans();
  EXPECT_THAT(spans, Each(SpanKindIsProducer()));
}

TEST(OpenTelemetry, MakeSpanWithParent) {
  auto span_catcher = InstallSpanCatcher();

  auto parent = MakeSpan("parent");
  opentelemetry::trace::StartSpanOptions options;
  options.parent = parent->GetContext();
  auto child = MakeSpan("child", options);
  child->End();
  parent->End();

  auto spans = span_catcher->GetSpans();
  EXPECT_THAT(spans,
              Contains(AllOf(SpanNamed("child"), SpanWithParent(parent))));
}

TEST(OpenTelemetry, EndSpanImplEndsSpan) {
  auto span_catcher = InstallSpanCatcher();

  auto span = MakeSpan("span");
  auto spans = span_catcher->GetSpans();
  EXPECT_THAT(spans, IsEmpty());

  EndSpanImpl(*span, Status());
  spans = span_catcher->GetSpans();
  EXPECT_THAT(spans, ElementsAre(SpanNamed("span")));
}

TEST(OpenTelemetry, EndSpanImplSuccess) {
  auto span_catcher = InstallSpanCatcher();

  auto span = MakeSpan("success");
  EndSpanImpl(*span, Status());

  auto spans = span_catcher->GetSpans();
  EXPECT_THAT(
      spans,
      ElementsAre(AllOf(
          SpanWithStatus(opentelemetry::trace::StatusCode::kOk),
          SpanHasAttributes(OTelAttribute<int>("gl-cpp.status_code", 0)))));
}

TEST(OpenTelemetry, EndSpanImplFail) {
  auto span_catcher = InstallSpanCatcher();
  auto const code = static_cast<int>(StatusCode::kAborted);

  auto span = MakeSpan("fail");
  EndSpanImpl(*span, Status(StatusCode::kAborted, "not good"));

  auto spans = span_catcher->GetSpans();
  EXPECT_THAT(
      spans,
      ElementsAre(AllOf(
          SpanWithStatus(opentelemetry::trace::StatusCode::kError, "not good"),
          SpanHasAttributes(OTelAttribute<int>("gl-cpp.status_code", code),
                            OTelAttribute<std::string>("gl-cpp.error.message",
                                                       "not good")))));
}

TEST(OpenTelemetry, EndSpanImplErrorInfo) {
  auto span_catcher = InstallSpanCatcher();
  auto const code = static_cast<int>(StatusCode::kAborted);

  auto span = MakeSpan("reason");
  EndSpanImpl(*span, Status(StatusCode::kAborted, "not good",
                            ErrorInfo("reason", {}, {})));
  auto spans = span_catcher->GetSpans();
  EXPECT_THAT(
      spans,
      ElementsAre(AllOf(
          SpanWithStatus(opentelemetry::trace::StatusCode::kError, "not good"),
          SpanHasAttributes(
              OTelAttribute<int>("gl-cpp.status_code", code),
              OTelAttribute<std::string>("gl-cpp.error.message", "not good"),
              OTelAttribute<std::string>("gl-cpp.error.reason", "reason")))));

  span = MakeSpan("domain");
  EndSpanImpl(*span, Status(StatusCode::kAborted, "not good",
                            ErrorInfo({}, "domain", {})));
  spans = span_catcher->GetSpans();
  EXPECT_THAT(
      spans,
      ElementsAre(AllOf(
          SpanWithStatus(opentelemetry::trace::StatusCode::kError, "not good"),
          SpanHasAttributes(
              OTelAttribute<int>("gl-cpp.status_code", code),
              OTelAttribute<std::string>("gl-cpp.error.message", "not good"),
              OTelAttribute<std::string>("gl-cpp.error.domain", "domain")))));

  span = MakeSpan("metadata");
  EndSpanImpl(*span, Status(StatusCode::kAborted, "not good",
                            ErrorInfo({}, {}, {{"k1", "v1"}, {"k2", "v2"}})));
  spans = span_catcher->GetSpans();
  EXPECT_THAT(
      spans,
      ElementsAre(AllOf(
          SpanWithStatus(opentelemetry::trace::StatusCode::kError, "not good"),
          SpanHasAttributes(
              OTelAttribute<int>("gl-cpp.status_code", code),
              OTelAttribute<std::string>("gl-cpp.error.message", "not good"),
              OTelAttribute<std::string>("gl-cpp.error.metadata.k1", "v1"),
              OTelAttribute<std::string>("gl-cpp.error.metadata.k2", "v2")))));
}

TEST(OpenTelemetry, EndSpanStatus) {
  auto span_catcher = InstallSpanCatcher();

  auto v1 = Status();
  auto s1 = MakeSpan("s1");
  auto r1 = EndSpan(*s1, v1);
  EXPECT_EQ(r1, v1);

  auto v2 = Status(StatusCode::kAborted, "fail");
  auto s2 = MakeSpan("s2");
  auto r2 = EndSpan(*s2, v2);
  EXPECT_EQ(r2, v2);

  auto spans = span_catcher->GetSpans();
  EXPECT_THAT(
      spans,
      ElementsAre(SpanWithStatus(opentelemetry::trace::StatusCode::kOk),
                  SpanWithStatus(opentelemetry::trace::StatusCode::kError)));
}

TEST(OpenTelemetry, EndSpanStatusOr) {
  auto span_catcher = InstallSpanCatcher();

  auto v1 = StatusOr<int>(5);
  auto s1 = MakeSpan("s1");
  auto r1 = EndSpan(*s1, v1);
  EXPECT_EQ(r1, v1);

  auto v2 = StatusOr<int>(Status(StatusCode::kAborted, "fail"));
  auto s2 = MakeSpan("s2");
  auto r2 = EndSpan(*s2, v2);
  EXPECT_EQ(r2, v2);

  auto spans = span_catcher->GetSpans();
  EXPECT_THAT(
      spans,
      ElementsAre(SpanWithStatus(opentelemetry::trace::StatusCode::kOk),
                  SpanWithStatus(opentelemetry::trace::StatusCode::kError)));
}

TEST(OpenTelemetry, EndSpanFutureStatus) {
  auto span_catcher = InstallSpanCatcher();

  promise<Status> p1;
  auto v1 = Status();
  auto r1 = EndSpan(MakeSpan("s1"), p1.get_future());
  EXPECT_FALSE(r1.is_ready());
  p1.set_value(v1);
  EXPECT_TRUE(r1.is_ready());
  EXPECT_EQ(r1.get(), v1);

  promise<Status> p2;
  auto v2 = Status(StatusCode::kAborted, "fail");
  auto r2 = EndSpan(MakeSpan("s2"), p2.get_future());
  EXPECT_FALSE(r2.is_ready());
  p2.set_value(v2);
  EXPECT_TRUE(r2.is_ready());
  EXPECT_EQ(r2.get(), v2);

  auto spans = span_catcher->GetSpans();
  EXPECT_THAT(
      spans,
      ElementsAre(SpanWithStatus(opentelemetry::trace::StatusCode::kOk),
                  SpanWithStatus(opentelemetry::trace::StatusCode::kError)));
}

TEST(OpenTelemetry, EndSpanFutureStatusOr) {
  auto span_catcher = InstallSpanCatcher();

  promise<StatusOr<int>> p1;
  auto v1 = StatusOr<int>(5);
  auto r1 = EndSpan(MakeSpan("s1"), p1.get_future());
  EXPECT_FALSE(r1.is_ready());
  p1.set_value(v1);
  EXPECT_TRUE(r1.is_ready());
  EXPECT_EQ(r1.get(), v1);

  promise<StatusOr<int>> p2;
  auto v2 = StatusOr<int>(Status(StatusCode::kAborted, "fail"));
  auto r2 = EndSpan(MakeSpan("s2"), p2.get_future());
  EXPECT_FALSE(r2.is_ready());
  p2.set_value(v2);
  EXPECT_TRUE(r2.is_ready());
  EXPECT_EQ(r2.get(), v2);

  auto spans = span_catcher->GetSpans();
  EXPECT_THAT(
      spans,
      ElementsAre(SpanWithStatus(opentelemetry::trace::StatusCode::kOk),
                  SpanWithStatus(opentelemetry::trace::StatusCode::kError)));
}

TEST(OpenTelemetry, EndSpanFutureDetachesContext) {
  auto span_catcher = InstallSpanCatcher();

  // Set the `OTelContext` like we do in `AsyncOperation`s
  auto c = opentelemetry::context::Context("key", true);
  ScopedOTelContext scope({c});
  EXPECT_EQ(c, opentelemetry::context::RuntimeContext::GetCurrent());

  promise<Status> p;
  auto f = EndSpan(MakeSpan("span"), p.get_future()).then([](auto f) {
    auto s = f.get();
    // The `OTelContext` should be cleared by the time we exit `EndSpan()`.
    EXPECT_EQ(opentelemetry::context::Context{},
              opentelemetry::context::RuntimeContext::GetCurrent());
    return s;
  });

  p.set_value(Status());
  EXPECT_STATUS_OK(f.get());
  EXPECT_THAT(span_catcher->GetSpans(), ElementsAre(SpanNamed("span")));
}

TEST(OpenTelemetry, EndSpanVoid) {
  auto span_catcher = InstallSpanCatcher();

  auto span = MakeSpan("success");
  EndSpan(*span, Status());

  auto spans = span_catcher->GetSpans();
  EXPECT_THAT(
      spans,
      ElementsAre(AllOf(
          SpanWithStatus(opentelemetry::trace::StatusCode::kOk),
          SpanHasAttributes(OTelAttribute<int>("gl-cpp.status_code", 0)))));
}

TEST(OpenTelemetry, TracingEnabled) {
  auto options = Options{};
  EXPECT_FALSE(TracingEnabled(options));

  options.set<OpenTelemetryTracingOption>(false);
  EXPECT_FALSE(TracingEnabled(options));

  options.set<OpenTelemetryTracingOption>(true);
  EXPECT_TRUE(TracingEnabled(options));
}

TEST(OpenTelemetry, MakeTracedSleeperEnabled) {
  auto span_catcher = InstallSpanCatcher();

  MockFunction<void(ms)> mock_sleeper;
  EXPECT_CALL(mock_sleeper, Call(ms(42)));

  auto sleeper = mock_sleeper.AsStdFunction();
  auto result = MakeTracedSleeper(EnableTracing(Options{}), sleeper, "Backoff");
  result(ms(42));

  // Verify that a span was made.
  auto spans = span_catcher->GetSpans();
  EXPECT_THAT(spans, ElementsAre(SpanNamed("Backoff")));
}

TEST(OpenTelemetry, MakeTracedSleeperDisabled) {
  auto span_catcher = InstallSpanCatcher();

  MockFunction<void(ms)> mock_sleeper;
  EXPECT_CALL(mock_sleeper, Call(ms(42)));

  auto sleeper = mock_sleeper.AsStdFunction();
  auto result =
      MakeTracedSleeper(DisableTracing(Options{}), sleeper, "Backoff");
  result(ms(42));

  // Verify that no spans were made.
  auto spans = span_catcher->GetSpans();
  EXPECT_THAT(spans, IsEmpty());
}

TEST(OpenTelemetry, MakeTracedSleeperNoSpansIfNoSleep) {
  auto span_catcher = InstallSpanCatcher();

  MockFunction<void(ms)> mock_sleeper;
  EXPECT_CALL(mock_sleeper, Call(ms(0)));

  auto sleeper = mock_sleeper.AsStdFunction();
  auto result = MakeTracedSleeper(EnableTracing(Options{}), sleeper, "Backoff");
  result(ms(0));

  // Verify that no spans were made.
  auto spans = span_catcher->GetSpans();
  EXPECT_THAT(spans, IsEmpty());
}

TEST(OpenTelemetry, AddSpanAttributeEnabled) {
  auto span_catcher = InstallSpanCatcher();

  auto span = MakeSpan("span");
  auto scope = opentelemetry::trace::Scope(span);
  AddSpanAttribute(EnableTracing(Options{}), "key", "value");
  span->End();

  auto spans = span_catcher->GetSpans();
  EXPECT_THAT(spans,
              ElementsAre(AllOf(SpanNamed("span"),
                                SpanHasAttributes(OTelAttribute<std::string>(
                                    "key", "value")))));
}

TEST(OpenTelemetry, AddSpanAttributeDisabled) {
  auto span_catcher = InstallSpanCatcher();

  auto span = MakeSpan("span");
  auto scope = opentelemetry::trace::Scope(span);
  AddSpanAttribute(DisableTracing(Options{}), "key", "value");
  span->End();

  auto spans = span_catcher->GetSpans();
  EXPECT_THAT(
      spans,
      ElementsAre(AllOf(
          SpanNamed("span"),
          Not(SpanHasAttributes(OTelAttribute<std::string>("key", "value"))))));
}

#else

TEST(NoOpenTelemetry, TracingEnabled) {
  EXPECT_FALSE(TracingEnabled(Options{}));
}

TEST(NoOpenTelemetry, MakeTracedSleeper) {
  MockFunction<void(ms)> mock_sleeper;
  EXPECT_CALL(mock_sleeper, Call(ms(42)));

  auto sleeper = mock_sleeper.AsStdFunction();
  auto result = MakeTracedSleeper(Options{}, sleeper, "Backoff");
  result(ms(42));
}

#endif  // GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY

}  // namespace
}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google
