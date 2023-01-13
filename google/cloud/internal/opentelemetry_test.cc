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
#include "google/cloud/internal/opentelemetry.h"
#include "google/cloud/internal/opentelemetry_options.h"
#include "google/cloud/testing_util/opentelemetry_matchers.h"
#include <gmock/gmock.h>
#include <opentelemetry/trace/default_span.h>
#include <opentelemetry/version.h>

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {
namespace {

using ::google::cloud::testing_util::InstallSpanCatcher;
using ::google::cloud::testing_util::SpanAttribute;
using ::google::cloud::testing_util::SpanHasAttributes;
using ::google::cloud::testing_util::SpanHasInstrumentationScope;
using ::google::cloud::testing_util::SpanKindIsClient;
using ::google::cloud::testing_util::SpanNamed;
using ::google::cloud::testing_util::SpanWithStatus;
using ::testing::AllOf;
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

TEST(OpenTelemetry, TracingEnabled) {
  auto options = Options{};
  EXPECT_FALSE(TracingEnabled(options));

  options.set<OpenTelemetryTracingOption>(false);
  EXPECT_FALSE(TracingEnabled(options));

  options.set<OpenTelemetryTracingOption>(true);
  EXPECT_TRUE(TracingEnabled(options));
}

TEST(OpenTelemetry, GetTracer) {
  auto span_catcher = InstallSpanCatcher();

  auto options = Options{};
  auto tracer = GetTracer(options);

  auto s1 = tracer->StartSpan("span");
  s1->End();

  auto spans = span_catcher->GetSpans();
  EXPECT_THAT(spans, ElementsAre(SpanHasInstrumentationScope()));
}

TEST(OpenTelemetry, MakeSpan) {
  auto span_catcher = InstallSpanCatcher();

  auto options = Options{};
  OptionsSpan current(options);

  auto s1 = MakeSpan("span1");
  s1->End();
  auto s2 = MakeSpan("span2");
  s2->End();

  auto spans = span_catcher->GetSpans();
  EXPECT_THAT(spans, Each(SpanHasInstrumentationScope()));
  EXPECT_THAT(spans, Each(SpanKindIsClient()));
  EXPECT_THAT(spans, ElementsAre(SpanNamed("span1"), SpanNamed("span2")));
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
          SpanHasAttributes(SpanAttribute<int>("gcloud.status_code", 0)))));
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
          SpanHasAttributes(SpanAttribute<int>("gcloud.status_code", code)))));
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
              SpanAttribute<int>("gcloud.status_code", code),
              SpanAttribute<std::string>("gcloud.error.reason", "reason")))));

  span = MakeSpan("domain");
  EndSpanImpl(*span, Status(StatusCode::kAborted, "not good",
                            ErrorInfo({}, "domain", {})));
  spans = span_catcher->GetSpans();
  EXPECT_THAT(
      spans,
      ElementsAre(AllOf(
          SpanWithStatus(opentelemetry::trace::StatusCode::kError, "not good"),
          SpanHasAttributes(
              SpanAttribute<int>("gcloud.status_code", code),
              SpanAttribute<std::string>("gcloud.error.domain", "domain")))));

  span = MakeSpan("metadata");
  EndSpanImpl(*span, Status(StatusCode::kAborted, "not good",
                            ErrorInfo({}, {}, {{"k1", "v1"}, {"k2", "v2"}})));
  spans = span_catcher->GetSpans();
  EXPECT_THAT(
      spans,
      ElementsAre(AllOf(
          SpanWithStatus(opentelemetry::trace::StatusCode::kError, "not good"),
          SpanHasAttributes(
              SpanAttribute<int>("gcloud.status_code", code),
              SpanAttribute<std::string>("gcloud.error.metadata.k1", "v1"),
              SpanAttribute<std::string>("gcloud.error.metadata.k2", "v2")))));
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

}  // namespace
}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google
#endif  // GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY
