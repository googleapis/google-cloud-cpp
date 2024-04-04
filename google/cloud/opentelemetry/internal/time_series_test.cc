// Copyright 2024 Google LLC
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

#include "google/cloud/opentelemetry/internal/time_series.h"
#include "google/cloud/internal/time_utils.h"
#include "google/cloud/testing_util/chrono_output.h"
#include "google/cloud/testing_util/scoped_log.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace otel_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::testing::AllOf;
using ::testing::Contains;
using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::HasSubstr;
using ::testing::Pair;
using ::testing::ResultOf;
using ::testing::UnorderedElementsAre;

std::string ToString(opentelemetry::sdk::metrics::InstrumentValueType type) {
  switch (type) {
    case opentelemetry::sdk::metrics::InstrumentValueType::kInt:
      return "kInt";
    case opentelemetry::sdk::metrics::InstrumentValueType::kLong:
      return "kLong";
    case opentelemetry::sdk::metrics::InstrumentValueType::kFloat:
      return "kFloat";
    case opentelemetry::sdk::metrics::InstrumentValueType::kDouble:
      return "kDouble";
    default:
      return "unreachable";
  }
}

auto DoubleTypedValue(double v) {
  return ResultOf(
      "double_value",
      [](google::monitoring::v3::Point const& p) {
        return p.value().double_value();
      },
      Eq(v));
}

auto Int64TypedValue(std::int64_t v) {
  return ResultOf(
      "int64_value",
      [](google::monitoring::v3::Point const& p) {
        return p.value().int64_value();
      },
      Eq(v));
}

auto Interval(std::chrono::system_clock::time_point start,
              std::chrono::system_clock::time_point end) {
  return AllOf(
      ResultOf(
          "start_time",
          [](google::monitoring::v3::Point const& p) {
            return internal::ToChronoTimePoint(p.interval().start_time());
          },
          Eq(start)),
      ResultOf(
          "end_time",
          [](google::monitoring::v3::Point const& p) {
            return internal::ToChronoTimePoint(p.interval().end_time());
          },
          Eq(end)));
}

TEST(ToMetric, Simple) {
  opentelemetry::sdk::metrics::MetricData md;
  md.instrument_descriptor.name_ = "test";

  opentelemetry::sdk::metrics::PointAttributes attributes = {
      {"key1", "value1"}, {"_key2", "value2"}};

  auto metric = ToMetric(md, attributes);

  EXPECT_EQ(metric.type(), "workload.googleapis.com/test");
  EXPECT_THAT(metric.labels(), UnorderedElementsAre(Pair("key1", "value1"),
                                                    Pair("_key2", "value2")));
}

TEST(ToMetric, BadLabelNames) {
  testing_util::ScopedLog log;

  opentelemetry::sdk::metrics::PointAttributes attributes = {
      {"99", "dropped"}, {"a key-with.bad/characters", "value"}};

  auto metric = ToMetric({}, attributes);

  EXPECT_THAT(metric.labels(),
              UnorderedElementsAre(Pair("a_key_with_bad_characters", "value")));

  EXPECT_THAT(
      log.ExtractLines(),
      Contains(AllOf(HasSubstr("Dropping metric label"), HasSubstr("99"))));
}

TEST(SumPointData, Simple) {
  auto const start = std::chrono::system_clock::now();
  auto const end = start + std::chrono::seconds(5);

  opentelemetry::sdk::metrics::MetricData md;
  md.instrument_descriptor.unit_ = "unit";
  md.instrument_descriptor.value_type_ =
      opentelemetry::sdk::metrics::InstrumentValueType::kInt;
  md.start_ts = start;
  md.end_ts = end;

  opentelemetry::sdk::metrics::SumPointData point;
  point.value_ = 42L;

  auto ts = ToTimeSeries(md, point);
  EXPECT_EQ(ts.unit(), "unit");
  EXPECT_EQ(ts.metric_kind(), google::api::MetricDescriptor::CUMULATIVE);
  EXPECT_THAT(ts.points(),
              ElementsAre(AllOf(Int64TypedValue(42), Interval(start, end))));
}

TEST(SumPointData, IntValueTypes) {
  opentelemetry::sdk::metrics::SumPointData point;
  point.value_ = 42L;

  for (auto value_type : {
           opentelemetry::sdk::metrics::InstrumentValueType::kInt,
           opentelemetry::sdk::metrics::InstrumentValueType::kLong,
       }) {
    SCOPED_TRACE("value_type: " + ToString(value_type));

    opentelemetry::sdk::metrics::MetricData md;
    md.instrument_descriptor.value_type_ = value_type;

    auto ts = ToTimeSeries(md, point);
    EXPECT_THAT(ts.points(), ElementsAre(Int64TypedValue(42)));
  }
}

TEST(SumPointData, DoubleValueTypes) {
  opentelemetry::sdk::metrics::SumPointData point;
  point.value_ = 42.0;

  for (auto value_type : {
           opentelemetry::sdk::metrics::InstrumentValueType::kFloat,
           opentelemetry::sdk::metrics::InstrumentValueType::kDouble,
       }) {
    SCOPED_TRACE("value_type: " + ToString(value_type));

    opentelemetry::sdk::metrics::MetricData md;
    md.instrument_descriptor.value_type_ = value_type;

    auto ts = ToTimeSeries(md, point);
    EXPECT_THAT(ts.points(), ElementsAre(DoubleTypedValue(42)));
  }
}

TEST(SumPointData, NonEmptyInterval) {
  auto const start = std::chrono::system_clock::now();
  auto const end = start - std::chrono::seconds(5);
  EXPECT_LE(end - start, std::chrono::seconds::zero());

  // The spec says to drop the end timestamp, and use the start timestamp plus
  // 1ms as the end timestamp.
  auto const expected_end = start + std::chrono::milliseconds(1);

  opentelemetry::sdk::metrics::MetricData md;
  md.instrument_descriptor.value_type_ =
      opentelemetry::sdk::metrics::InstrumentValueType::kInt;
  md.start_ts = start;
  md.end_ts = end;

  auto ts = ToTimeSeries(md, opentelemetry::sdk::metrics::SumPointData{});
  EXPECT_THAT(ts.points(), ElementsAre(Interval(start, expected_end)));
}

TEST(LastValuePointData, Simple) {
  auto const now = std::chrono::system_clock::now();

  opentelemetry::sdk::metrics::MetricData md;
  md.instrument_descriptor.unit_ = "unit";
  md.instrument_descriptor.value_type_ =
      opentelemetry::sdk::metrics::InstrumentValueType::kInt;
  md.start_ts = now;
  md.end_ts = now;

  opentelemetry::sdk::metrics::LastValuePointData point;
  point.value_ = 42L;

  auto interval = [](std::chrono::system_clock::time_point end) {
    return AllOf(
        ResultOf(
            "has_start_time",
            [](google::monitoring::v3::Point const& p) {
              return p.interval().has_start_time();
            },
            Eq(false)),
        ResultOf(
            "end_time",
            [](google::monitoring::v3::Point const& p) {
              return internal::ToChronoTimePoint(p.interval().end_time());
            },
            Eq(end)));
  };

  auto ts = ToTimeSeries(md, point);
  EXPECT_EQ(ts.unit(), "unit");
  EXPECT_EQ(ts.metric_kind(), google::api::MetricDescriptor::GAUGE);
  EXPECT_THAT(ts.points(),
              ElementsAre(AllOf(Int64TypedValue(42), interval(now))));
}

TEST(LastValuePointData, IntValueTypes) {
  opentelemetry::sdk::metrics::LastValuePointData point;
  point.value_ = 42L;

  for (auto value_type : {
           opentelemetry::sdk::metrics::InstrumentValueType::kInt,
           opentelemetry::sdk::metrics::InstrumentValueType::kLong,
       }) {
    SCOPED_TRACE("value_type: " + ToString(value_type));

    opentelemetry::sdk::metrics::MetricData md;
    md.instrument_descriptor.value_type_ = value_type;

    auto ts = ToTimeSeries(md, point);
    EXPECT_THAT(ts.points(), ElementsAre(Int64TypedValue(42)));
  }
}

TEST(LastValuePointData, DoubleValueTypes) {
  opentelemetry::sdk::metrics::LastValuePointData point;
  point.value_ = 42.0;

  for (auto value_type : {
           opentelemetry::sdk::metrics::InstrumentValueType::kFloat,
           opentelemetry::sdk::metrics::InstrumentValueType::kDouble,
       }) {
    SCOPED_TRACE("value_type: " + ToString(value_type));

    opentelemetry::sdk::metrics::MetricData md;
    md.instrument_descriptor.value_type_ = value_type;

    auto ts = ToTimeSeries(md, point);
    EXPECT_THAT(ts.points(), ElementsAre(DoubleTypedValue(42)));
  }
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace otel_internal
}  // namespace cloud
}  // namespace google
