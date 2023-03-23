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
#include "google/cloud/internal/traced_stream_range.h"
#include "google/cloud/mocks/mock_stream_range.h"
#include "google/cloud/internal/make_status.h"
#include "google/cloud/options.h"
#include "google/cloud/testing_util/opentelemetry_matchers.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {
namespace {

using ::google::cloud::testing_util::SpanNamed;
using ::google::cloud::testing_util::SpanWithStatus;
using ::google::cloud::testing_util::StatusIs;
using ::google::cloud::testing_util::ThereIsAnActiveSpan;
using ::testing::AllOf;
using ::testing::ElementsAre;
using ::testing::IsEmpty;

StreamRange<int> MakeTestStreamRange(StreamRange<int> sr) {
  auto span = MakeSpan("span");
  auto scope = opentelemetry::trace::Scope(span);
  return MakeTracedStreamRange(std::move(span), std::move(sr));
}

TEST(TracedStreamRange, Success) {
  auto span_catcher = testing_util::InstallSpanCatcher();

  auto sr = mocks::MakeStreamRange<int>({1, 2, 3});
  auto traced = MakeTestStreamRange(std::move(sr));

  std::vector<int> actual;
  for (auto& v : traced) {
    ASSERT_STATUS_OK(v);
    actual.push_back(*std::move(v));
  }
  EXPECT_THAT(actual, ElementsAre(1, 2, 3));

  auto spans = span_catcher->GetSpans();
  EXPECT_THAT(
      spans, ElementsAre(
                 AllOf(SpanNamed("span"),
                       SpanWithStatus(opentelemetry::trace::StatusCode::kOk))));
}

TEST(TracedStreamRange, Error) {
  auto span_catcher = testing_util::InstallSpanCatcher();

  auto sr = mocks::MakeStreamRange<int>({}, AbortedError("fail"));
  auto traced = MakeTestStreamRange(std::move(sr));
  for (auto const& v : traced) {
    EXPECT_THAT(v, StatusIs(StatusCode::kAborted));
  }

  auto spans = span_catcher->GetSpans();
  EXPECT_THAT(
      spans,
      ElementsAre(AllOf(
          SpanNamed("span"),
          SpanWithStatus(opentelemetry::trace::StatusCode::kError, "fail"))));
}

TEST(TracedStreamRange, SpanEndsWhenRangeEnds) {
  auto span_catcher = testing_util::InstallSpanCatcher();

  auto sr = mocks::MakeStreamRange<int>({1, 2, 3});
  auto traced = MakeTestStreamRange(std::move(sr));

  for (auto const& v : traced) {
    EXPECT_STATUS_OK(v);
    auto spans = span_catcher->GetSpans();
    EXPECT_THAT(spans, IsEmpty());
  }

  auto spans = span_catcher->GetSpans();
  EXPECT_THAT(spans, ElementsAre(SpanNamed("span")));
}

TEST(TracedStreamRange, SpanEndsWithSuccessOnUnfinishedRange) {
  auto span_catcher = testing_util::InstallSpanCatcher();

  {
    auto sr = mocks::MakeStreamRange<int>({1, 2, 3});
    auto traced = MakeTestStreamRange(std::move(sr));
  }
  EXPECT_FALSE(ThereIsAnActiveSpan());

  auto spans = span_catcher->GetSpans();
  EXPECT_THAT(
      spans, ElementsAre(
                 AllOf(SpanNamed("span"),
                       SpanWithStatus(opentelemetry::trace::StatusCode::kOk))));
}

TEST(TracedStreamRange, SpanInactiveWhileIterating) {
  auto span_catcher = testing_util::InstallSpanCatcher();

  auto sr = mocks::MakeStreamRange<int>({1, 2, 3});
  auto traced = MakeTestStreamRange(std::move(sr));

  EXPECT_FALSE(ThereIsAnActiveSpan());
  for (auto const& v : traced) {
    EXPECT_STATUS_OK(v);
    EXPECT_FALSE(ThereIsAnActiveSpan());
  }
  EXPECT_FALSE(ThereIsAnActiveSpan());
}

}  // namespace
}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google
#endif  // GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY
