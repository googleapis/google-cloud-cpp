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

#ifdef GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY
#include "google/cloud/bigtable/internal/traced_row_reader.h"
#include "google/cloud/bigtable/mocks/mock_row_reader.h"
#include "google/cloud/internal/make_status.h"
#include "google/cloud/internal/opentelemetry.h"
#include "google/cloud/testing_util/opentelemetry_matchers.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace bigtable_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::google::cloud::testing_util::EventNamed;
using ::google::cloud::testing_util::SpanNamed;
using ::google::cloud::testing_util::SpanWithStatus;
using ::google::cloud::testing_util::StatusIs;
using ::google::cloud::testing_util::ThereIsAnActiveSpan;
using ::testing::AllOf;
using ::testing::ElementsAre;
using ::testing::IsEmpty;

bigtable::RowReader MakeTestRowReader(bigtable::RowReader reader) {
  auto span = internal::MakeSpan("span");
  auto scope = opentelemetry::trace::Scope(span);
  return MakeTracedRowReader(std::move(span), std::move(reader));
}

std::vector<bigtable::Row> Rows(std::vector<std::string> row_keys) {
  std::vector<bigtable::Row> rows;
  rows.reserve(row_keys.size());
  for (auto& r : row_keys) {
    rows.emplace_back(std::move(r), std::vector<bigtable::Cell>{});
  }
  return rows;
}

TEST(TracedStreamRange, Success) {
  auto span_catcher = testing_util::InstallSpanCatcher();

  auto reader = bigtable_mocks::MakeRowReader(Rows({"r1", "r2", "r3"}));
  auto traced = MakeTestRowReader(std::move(reader));

  std::vector<bigtable::RowKeyType> actual;
  for (auto& v : traced) {
    ASSERT_STATUS_OK(v);
    actual.push_back(std::move(v->row_key()));
  }
  EXPECT_THAT(actual, ElementsAre("r1", "r2", "r3"));

  EXPECT_THAT(span_catcher->GetSpans(),
              ElementsAre(AllOf(
                  SpanNamed("span"),
                  SpanWithStatus(opentelemetry::trace::StatusCode::kOk))));
}

TEST(TracedStreamRange, Error) {
  auto span_catcher = testing_util::InstallSpanCatcher();

  auto reader =
      bigtable_mocks::MakeRowReader({}, internal::AbortedError("fail"));
  auto traced = MakeTestRowReader(std::move(reader));
  for (auto const& v : traced) {
    EXPECT_THAT(v, StatusIs(StatusCode::kAborted));
  }

  EXPECT_THAT(
      span_catcher->GetSpans(),
      ElementsAre(AllOf(
          SpanNamed("span"),
          SpanWithStatus(opentelemetry::trace::StatusCode::kError, "fail"))));
}

TEST(TracedStreamRange, SpanEndsWhenRangeEnds) {
  auto span_catcher = testing_util::InstallSpanCatcher();

  auto reader = bigtable_mocks::MakeRowReader(Rows({"r1", "r2", "r3"}));
  auto traced = MakeTestRowReader(std::move(reader));

  for (auto const& v : traced) {
    EXPECT_STATUS_OK(v);
    EXPECT_THAT(span_catcher->GetSpans(), IsEmpty());
  }

  EXPECT_THAT(span_catcher->GetSpans(), ElementsAre(SpanNamed("span")));
}

TEST(TracedStreamRange, SpanEndsWithSuccessOnUnfinishedRange) {
  auto span_catcher = testing_util::InstallSpanCatcher();

  {
    auto reader = bigtable_mocks::MakeRowReader(Rows({"r1", "r2", "r3"}));
    auto traced = MakeTestRowReader(std::move(reader));
  }
  EXPECT_FALSE(ThereIsAnActiveSpan());

  EXPECT_THAT(span_catcher->GetSpans(),
              ElementsAre(AllOf(
                  SpanNamed("span"),
                  SpanWithStatus(opentelemetry::trace::StatusCode::kOk))));
}

TEST(TracedStreamRange, SpanInactiveWhileIterating) {
  auto span_catcher = testing_util::InstallSpanCatcher();

  auto reader = bigtable_mocks::MakeRowReader(Rows({"r1", "r2", "r3"}));
  auto traced = MakeTestRowReader(std::move(reader));

  EXPECT_FALSE(ThereIsAnActiveSpan());
  for (auto const& v : traced) {
    EXPECT_STATUS_OK(v);
    EXPECT_FALSE(ThereIsAnActiveSpan());
  }
  EXPECT_FALSE(ThereIsAnActiveSpan());
}

TEST(TracedStreamRange, Cancel) {
  auto span_catcher = testing_util::InstallSpanCatcher();

  auto reader =
      bigtable_mocks::MakeRowReader({}, internal::CancelledError("cancelled"));
  auto traced = MakeTestRowReader(std::move(reader));
  traced.Cancel();
  for (auto const& v : traced) {
    EXPECT_THAT(v, StatusIs(StatusCode::kCancelled));
  }

  EXPECT_THAT(span_catcher->GetSpans(),
              ElementsAre(AllOf(SpanNamed("span"),
                                SpanEventsAre(EventNamed("gl-cpp.cancel")))));
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable_internal
}  // namespace cloud
}  // namespace google
#endif  // GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY
