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

#include "google/cloud/pubsub/internal/tracing_helpers.h"
#include "google/cloud/pubsub/options.h"
#include "google/cloud/internal/opentelemetry.h"
#include "google/cloud/testing_util/opentelemetry_matchers.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace pubsub_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::google::cloud::internal::MakeSpan;
using ::google::cloud::testing_util::InstallSpanCatcher;
using ::google::cloud::testing_util::OTelAttribute;
using ::google::cloud::testing_util::SpanHasAttributes;
using ::google::cloud::testing_util::SpanHasNoAttributes;
using ::google::cloud::testing_util::SpanIsRoot;
using ::google::cloud::testing_util::SpanNamed;
using ::testing::_;
using ::testing::AllOf;
using ::testing::Contains;
using ::testing::IsEmpty;
using ::testing::SizeIs;

#if OPENTELEMETRY_VERSION_MAJOR >= 2 || \
    (OPENTELEMETRY_VERSION_MAJOR == 1 && OPENTELEMETRY_VERSION_MINOR >= 13)
TEST(RootStartSpanOptions, CreateRootSpan) {
  auto span_catcher = InstallSpanCatcher();
  auto active_span = MakeSpan("active span");
  auto active_scope = opentelemetry::trace::Scope(active_span);
  active_span->End();
  auto options = RootStartSpanOptions();
  auto span = MakeSpan("test span", options);
  auto scope = opentelemetry::trace::Scope(span);
  span->End();

  auto spans = span_catcher->GetSpans();

  EXPECT_THAT(spans, Contains(AllOf(SpanNamed("test span"), SpanIsRoot())));
}
#endif

#if OPENTELEMETRY_ABI_VERSION_NO >= 2

using ::google::cloud::testing_util::SpanLinksSizeIs;
using ::testing::Pair;

TEST(CreateLinks, CreateLinks) {
  auto span_catcher = InstallSpanCatcher();
  auto span = MakeSpan("test span");
  auto scope = opentelemetry::trace::Scope(span);
  auto span_context = span->GetContext();
  span->End();

  auto links = CreateLinks(span->GetContext());

  EXPECT_THAT(links, SizeIs(1));
  EXPECT_THAT(links, Contains(Pair(span_context, _)));
}

TEST(CreateLinks, SkipsInvalidContext) {
  auto span = MakeSpan("test span");
  auto scope = opentelemetry::trace::Scope(span);
  span->End();

  auto links = CreateLinks(opentelemetry::trace::SpanContext::GetInvalid());

  EXPECT_THAT(links, IsEmpty());
}

TEST(CreateLinks, SkipsIfSpanNotSampled) {
  auto span = MakeSpan("test span");
  auto scope = opentelemetry::trace::Scope(span);
  span->End();
  // Install the span catcher after, so the span is never sampled.
  auto span_catcher = InstallSpanCatcher();

  auto links = CreateLinks(span->GetContext());

  EXPECT_THAT(links, IsEmpty());
}

TEST(MaybeAddLinkAttributes, DoesNotAddSpanIdAndTraceIdAttribute) {
  auto span_catcher = InstallSpanCatcher();
  auto link = internal::MakeSpan("link span");
  link->End();
  auto span = internal::MakeSpan("test span");
  auto scope = internal::OTelScope(span);

  MaybeAddLinkAttributes(*span, link->GetContext(), "test");

  span->End();
  auto spans = span_catcher->GetSpans();
  EXPECT_THAT(spans, SizeIs(2));
  EXPECT_THAT(spans.at(1), SpanNamed("test span"));
  EXPECT_THAT(spans.at(1), SpanHasNoAttributes());
}

#else

TEST(CreateLinks, Noop) {
  auto span = MakeSpan("test span");
  auto scope = opentelemetry::trace::Scope(span);
  span->End();

  auto links = CreateLinks(span->GetContext());

  EXPECT_THAT(links, IsEmpty());
}

TEST(MaybeAddLinkAttributes, AddsSpanIdAndTraceIdAttribute) {
  auto span_catcher = InstallSpanCatcher();
  auto link = internal::MakeSpan("link span");
  link->End();
  auto span = internal::MakeSpan("test span");
  auto scope = internal::OTelScope(span);

  MaybeAddLinkAttributes(*span, link->GetContext(), "test");

  span->End();
  auto spans = span_catcher->GetSpans();
  EXPECT_THAT(
      spans,
      Contains(AllOf(
          SpanNamed("test span"),
          SpanHasAttributes(
              OTelAttribute<std::string>("gcp_pubsub.test.trace_id", _),
              OTelAttribute<std::string>("gcp_pubsub.test.span_id", _)))));
}

TEST(MaybeAddLinkAttributes, IgnoreLinkIfSpanIsNotSampled) {
  auto link = internal::MakeSpan("link span");
  link->End();
  // Install span catcher AFTER the link span is created so it is not sampled.
  auto span_catcher = InstallSpanCatcher();
  auto span = internal::MakeSpan("test span");
  auto scope = internal::OTelScope(span);

  MaybeAddLinkAttributes(*span, link->GetContext(), "test");

  span->End();
  auto spans = span_catcher->GetSpans();
  EXPECT_THAT(spans, SizeIs(1));
  EXPECT_THAT(spans.at(0), SpanNamed("test span"));
  EXPECT_THAT(spans.at(0), SpanHasNoAttributes());
}

TEST(MaybeAddLinkAttributes, IgnoreLinkIfInvalidSpanContext) {
  auto span_catcher = InstallSpanCatcher();
  auto span = internal::MakeSpan("test span");
  auto scope = internal::OTelScope(span);

  MaybeAddLinkAttributes(*span, opentelemetry::trace::SpanContext::GetInvalid(),
                         "test");

  span->End();
  auto spans = span_catcher->GetSpans();
  EXPECT_THAT(spans, SizeIs(1));
  EXPECT_THAT(spans.at(0), SpanNamed("test span"));
  EXPECT_THAT(spans.at(0), SpanHasNoAttributes());
}

#endif

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace pubsub_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY
