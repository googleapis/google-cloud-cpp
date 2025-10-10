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
#include "google/cloud/internal/algorithm.h"
#include "google/cloud/internal/time_utils.h"
#include "google/cloud/project.h"
#include "google/cloud/testing_util/chrono_output.h"
#include "google/cloud/testing_util/is_proto_equal.h"
#include "google/cloud/testing_util/scoped_log.h"
#include <google/protobuf/text_format.h>
#include <gmock/gmock.h>
#include <opentelemetry/sdk/metrics/export/metric_producer.h>
#include <opentelemetry/sdk/resource/resource.h>
#include <opentelemetry/sdk/resource/semantic_conventions.h>
#include <algorithm>
#include <cstdint>

namespace google {
namespace cloud {
namespace otel_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

namespace sc = opentelemetry::sdk::resource::SemanticConventions;

using ::google::cloud::testing_util::IsProtoEqual;
using ::google::protobuf::TextFormat;
using ::testing::_;
using ::testing::AllOf;
using ::testing::AnyOf;
using ::testing::Contains;
using ::testing::Each;
using ::testing::ElementsAre;
using ::testing::ElementsAreArray;
using ::testing::Eq;
using ::testing::HasSubstr;
using ::testing::IsEmpty;
using ::testing::Not;
using ::testing::Pair;
using ::testing::ResultOf;
using ::testing::SizeIs;
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

auto distribution = [](auto count, auto mean, auto bucket_counts_matcher,
                       auto bounds_matcher) {
  return AllOf(ResultOf(
                   "distribution count",
                   [](google::monitoring::v3::Point const& p) {
                     return p.value().distribution_value().count();
                   },
                   Eq(count)),
               ResultOf(
                   "distribution mean",
                   [](google::monitoring::v3::Point const& p) {
                     return p.value().distribution_value().mean();
                   },
                   Eq(mean)),
               ResultOf(
                   "distribution bucket_counts",
                   [](google::monitoring::v3::Point const& p) {
                     return p.value().distribution_value().bucket_counts();
                   },
                   bucket_counts_matcher),
               ResultOf(
                   "distribution bounds",
                   [](google::monitoring::v3::Point const& p) {
                     return p.value()
                         .distribution_value()
                         .bucket_options()
                         .explicit_buckets()
                         .bounds();
                   },
                   bounds_matcher));
};

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

auto TestResource() {
  return opentelemetry::sdk::resource::Resource::Create({
      {sc::kCloudProvider, "gcp"},
      {sc::kCloudPlatform, "gcp_compute_engine"},
      {sc::kHostId, "1020304050607080900"},
      {sc::kCloudAvailabilityZone, "us-central1-a"},
  });
}

auto IsTestResource() {
  return AllOf(
      ResultOf(
          "type",
          [](google::api::MonitoredResource const& mr) { return mr.type(); },
          Eq("gce_instance")),
      ResultOf(
          "labels",
          [](google::api::MonitoredResource const& mr) { return mr.labels(); },
          UnorderedElementsAre(Pair("zone", "us-central1-a"),
                               Pair("instance_id", "1020304050607080900"))));
}

auto MetricType(std::string const& type) {
  return ResultOf(
      "metric.type",
      [](google::monitoring::v3::TimeSeries const& ts) {
        return ts.metric().type();
      },
      Eq(type));
}

auto Unit(std::string const& unit) {
  return ResultOf(
      "unit",
      [](google::monitoring::v3::TimeSeries const& ts) { return ts.unit(); },
      Eq(unit));
}

auto SumTimeSeries() {
  return AllOf(ResultOf(
                   "metric_kind",
                   [](google::monitoring::v3::TimeSeries const& ts) {
                     return ts.metric_kind();
                   },
                   Eq(google::api::MetricDescriptor::CUMULATIVE)),
               ResultOf(
                   "value_type",
                   [](google::monitoring::v3::TimeSeries const& ts) {
                     return ts.value_type();
                   },
                   AnyOf(Eq(google::api::MetricDescriptor::INT64),
                         Eq(google::api::MetricDescriptor::DOUBLE))),
               ResultOf(
                   "points",
                   [](google::monitoring::v3::TimeSeries const& ts) {
                     return ts.points();
                   },
                   SizeIs(1)));
}

auto GaugeTimeSeries() {
  return AllOf(ResultOf(
                   "metric_kind",
                   [](google::monitoring::v3::TimeSeries const& ts) {
                     return ts.metric_kind();
                   },
                   Eq(google::api::MetricDescriptor::GAUGE)),
               ResultOf(
                   "value_type",
                   [](google::monitoring::v3::TimeSeries const& ts) {
                     return ts.value_type();
                   },
                   AnyOf(Eq(google::api::MetricDescriptor::INT64),
                         Eq(google::api::MetricDescriptor::DOUBLE))),
               ResultOf(
                   "points",
                   [](google::monitoring::v3::TimeSeries const& ts) {
                     return ts.points();
                   },
                   SizeIs(1)));
}

auto HistogramTimeSeries() {
  return AllOf(ResultOf(
                   "metric_kind",
                   [](google::monitoring::v3::TimeSeries const& ts) {
                     return ts.metric_kind();
                   },
                   Eq(google::api::MetricDescriptor::CUMULATIVE)),
               ResultOf(
                   "value_type",
                   [](google::monitoring::v3::TimeSeries const& ts) {
                     return ts.value_type();
                   },
                   Eq(google::api::MetricDescriptor::DISTRIBUTION)),
               ResultOf(
                   "points",
                   [](google::monitoring::v3::TimeSeries const& ts) {
                     return ts.points();
                   },
                   SizeIs(1)));
}

std::string PrefixWithWorkload(std::string s) {
  return "workload.googleapis.com/" + std::move(s);
}

TEST(ToMetric, Simple) {
  opentelemetry::sdk::metrics::MetricData md;
  md.instrument_descriptor.name_ = "test";

  opentelemetry::sdk::metrics::PointAttributes attributes = {
      {"key1", "value1"}, {"_key2", "value2"}};

  auto metric = ToMetric(md, attributes, {}, PrefixWithWorkload);

  EXPECT_EQ(metric.type(), "workload.googleapis.com/test");
  EXPECT_THAT(metric.labels(), UnorderedElementsAre(Pair("key1", "value1"),
                                                    Pair("_key2", "value2")));

  metric = ToMetric(md, {}, {}, [](std::string s) {
    std::replace(s.begin(), s.end(), 't', 'T');
    return "custom.googleapis.com/" + std::move(s);
  });
  EXPECT_EQ(metric.type(), "custom.googleapis.com/TesT");
}

TEST(ToMetric, BadLabelNames) {
  testing_util::ScopedLog log;

  opentelemetry::sdk::metrics::PointAttributes attributes = {
      {"99", "dropped"}, {"a key-with.bad/characters", "value"}};

  auto metric = ToMetric({}, attributes, {}, PrefixWithWorkload);

  EXPECT_THAT(metric.labels(),
              UnorderedElementsAre(Pair("a_key_with_bad_characters", "value")));

  EXPECT_THAT(
      log.ExtractLines(),
      Contains(AllOf(HasSubstr("Dropping metric label"), HasSubstr("99"))));
}

TEST(ToMetric, IncludesServiceLabelsFromResource) {
  opentelemetry::sdk::metrics::MetricData md;
  md.instrument_descriptor.name_ = "test";

  opentelemetry::sdk::resource::ResourceAttributes resource_attributes = {
      {"unused", "unused"},
      {sc::kServiceName, "test-name"},
      {sc::kServiceNamespace, "test-namespace"},
      {sc::kServiceInstanceId, "test-instance"},
  };
  auto resource =
      opentelemetry::sdk::resource::Resource::Create(resource_attributes);

  auto metric = ToMetric(md, {}, &resource, PrefixWithWorkload);
  EXPECT_THAT(
      metric.labels(),
      UnorderedElementsAre(Pair("service_name", "test-name"),
                           Pair("service_namespace", "test-namespace"),
                           Pair("service_instance_id", "test-instance")));
}

TEST(ToMetric, PointAttributesOverServiceResourceAttributes) {
  opentelemetry::sdk::metrics::MetricData md;
  md.instrument_descriptor.name_ = "test";

  opentelemetry::sdk::metrics::PointAttributes point_attributes = {
      {"service_name", "point-name"},
      {"service_namespace", "point-namespace"},
      {"service_instance_id", "point-instance"},
  };

  opentelemetry::sdk::resource::ResourceAttributes resource_attributes = {
      {sc::kServiceName, "resource-name"},
      {sc::kServiceNamespace, "resource-namespace"},
      {sc::kServiceInstanceId, "resource-instance"},
  };
  auto resource =
      opentelemetry::sdk::resource::Resource::Create(resource_attributes);

  auto metric = ToMetric(md, point_attributes, &resource, PrefixWithWorkload);
  EXPECT_THAT(
      metric.labels(),
      UnorderedElementsAre(Pair("service_name", "point-name"),
                           Pair("service_namespace", "point-namespace"),
                           Pair("service_instance_id", "point-instance")));
}

TEST(ToMetric, ResourceFilter) {
  opentelemetry::sdk::metrics::MetricData md;
  md.instrument_descriptor.name_ = "test";

  opentelemetry::sdk::metrics::PointAttributes point_attributes = {
      {"service_name", "point-name"},
      {"service_namespace", "point-namespace"},
      {"service_instance_id", "point-instance"},
      {sc::kServiceName, "resource-name"},
      {sc::kServiceNamespace, "resource-namespace"},
      {sc::kServiceInstanceId, "resource-instance"},
  };

  auto resource = opentelemetry::sdk::resource::Resource::Create({});

  auto resource_filter_fn =
      [resource_labels = std::set<std::string>{
           sc::kServiceName, sc::kServiceNamespace,
           sc::kServiceInstanceId}](std::string const& l) {
        return internal::Contains(resource_labels, l);
      };

  auto metric = ToMetric(md, point_attributes, &resource, PrefixWithWorkload,
                         resource_filter_fn);
  EXPECT_THAT(
      metric.labels(),
      UnorderedElementsAre(Pair("service_name", "point-name"),
                           Pair("service_namespace", "point-namespace"),
                           Pair("service_instance_id", "point-instance")));
}

TEST(SumPointData, Simple) {
  auto const start = std::chrono::system_clock::now();
  auto const end = start + std::chrono::seconds(5);

  opentelemetry::sdk::metrics::MetricData md;
  md.instrument_descriptor.value_type_ =
      opentelemetry::sdk::metrics::InstrumentValueType::kInt;
  md.start_ts = start;
  md.end_ts = end;

  opentelemetry::sdk::metrics::SumPointData point;
  point.value_ = std::int64_t{42};

  auto ts = ToTimeSeries(md, point);
  EXPECT_EQ(ts.metric_kind(), google::api::MetricDescriptor::CUMULATIVE);
  EXPECT_THAT(ts.points(),
              ElementsAre(AllOf(Int64TypedValue(42), Interval(start, end))));
}

TEST(SumPointData, IntValueTypes) {
  opentelemetry::sdk::metrics::SumPointData point;
  point.value_ = std::int64_t{42};

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
  md.instrument_descriptor.value_type_ =
      opentelemetry::sdk::metrics::InstrumentValueType::kInt;
  md.start_ts = now;
  md.end_ts = now;

  opentelemetry::sdk::metrics::LastValuePointData point;
  point.value_ = std::int64_t{42};

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
  EXPECT_EQ(ts.metric_kind(), google::api::MetricDescriptor::GAUGE);
  EXPECT_THAT(ts.points(),
              ElementsAre(AllOf(Int64TypedValue(42), interval(now))));
}

TEST(LastValuePointData, IntValueTypes) {
  opentelemetry::sdk::metrics::LastValuePointData point;
  point.value_ = std::int64_t{42};

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

TEST(HistogramPointData, SimpleWithInt64Sum) {
  auto const start = std::chrono::system_clock::now();
  auto const end = start + std::chrono::seconds(5);

  opentelemetry::sdk::metrics::MetricData md;
  md.instrument_descriptor.value_type_ =
      opentelemetry::sdk::metrics::InstrumentValueType::kInt;
  md.start_ts = start;
  md.end_ts = end;

  opentelemetry::sdk::metrics::HistogramPointData point;
  point.sum_ = std::int64_t{64};
  point.boundaries_ = {0.0, 1.0, 2.0, 3.0, 10.0, 30.0};
  point.counts_ = {0, 1, 4, 6, 4, 1, 0};
  point.count_ = 16;

  auto ts = ToTimeSeries(md, point);
  EXPECT_EQ(ts.metric_kind(), google::api::MetricDescriptor::CUMULATIVE);
  EXPECT_THAT(
      ts.points(),
      ElementsAre(AllOf(
          distribution(
              /*count=*/16, /*mean=*/4.0,
              /*bucket_counts_matcher=*/ElementsAre(0, 1, 4, 6, 4, 1, 0),
              /*bounds_matcher=*/ElementsAre(0.0, 1.0, 2.0, 3.0, 10.0, 30.0)),
          Interval(start, end))));
}

TEST(HistogramPointData, DoubleSum) {
  opentelemetry::sdk::metrics::HistogramPointData point;
  point.sum_ = 64.0;
  point.boundaries_ = {0.0, 1.0, 2.0, 3.0, 10.0, 30.0};
  point.counts_ = {0, 1, 4, 6, 4, 1, 0};
  point.count_ = 16;

  auto ts = ToTimeSeries(opentelemetry::sdk::metrics::MetricData{}, point);
  EXPECT_THAT(ts.points(), ElementsAre(distribution(
                               /*count=*/16, /*mean=*/4.0,
                               /*bucket_counts_matcher=*/_,
                               /*bounds_matcher=*/_)));
}

TEST(HistogramPointData, EmptyMean) {
  opentelemetry::sdk::metrics::HistogramPointData point;
  point.sum_ = std::int64_t{0};
  point.boundaries_ = {0.0, 1.0, 2.0, 3.0, 10.0, 30.0};
  point.counts_ = {0, 0, 0, 0, 0, 0, 0};
  point.count_ = 0;

  auto ts = ToTimeSeries(opentelemetry::sdk::metrics::MetricData{}, point);
  EXPECT_THAT(ts.points(), ElementsAre(distribution(
                               /*count=*/0, /*mean=*/0,
                               /*bucket_counts_matcher=*/_,
                               /*bounds_matcher=*/_)));
}

TEST(HistogramPointData, NonEmptyInterval) {
  auto const start = std::chrono::system_clock::now();
  auto const end = start - std::chrono::seconds(5);
  EXPECT_LE(end - start, std::chrono::seconds::zero());

  // The spec says to drop the end timestamp, and use the start timestamp plus
  // 1ms as the end timestamp.
  auto const expected_end = start + std::chrono::milliseconds(1);

  opentelemetry::sdk::metrics::MetricData md;
  md.start_ts = start;
  md.end_ts = end;

  auto ts = ToTimeSeries(md, opentelemetry::sdk::metrics::HistogramPointData{});
  EXPECT_THAT(ts.points(), ElementsAre(Interval(start, expected_end)));
}

TEST(ToMonitoredResource, FromMetricData) {
  opentelemetry::sdk::metrics::ResourceMetrics rm;
  auto const resource = TestResource();
  rm.resource_ = &resource;

  auto mr = ToMonitoredResource(rm, absl::nullopt);
  EXPECT_THAT(mr, IsTestResource());
}

TEST(ToMonitoredResource, Custom) {
  opentelemetry::sdk::metrics::ResourceMetrics rm;
  auto const resource = TestResource();
  rm.resource_ = &resource;

  // Use a custom resource, which will override the `TestResource()`
  google::api::MonitoredResource custom;
  custom.set_type("gcs_client");

  auto mr = ToMonitoredResource(rm, custom);
  EXPECT_THAT(mr, AllOf(IsProtoEqual(custom), Not(IsTestResource())));
}

TEST(IsEmptyTimeSeries, EmptyResourceMetrics) {
  opentelemetry::sdk::metrics::ResourceMetrics rm;
  EXPECT_TRUE(IsEmptyTimeSeries(rm));
}

TEST(IsEmptyTimeSeries, EmptyScopeMetrics) {
  opentelemetry::sdk::metrics::ScopeMetrics sm;
  opentelemetry::sdk::metrics::ResourceMetrics rm;
  rm.scope_metric_data_.push_back(std::move(sm));
  EXPECT_TRUE(IsEmptyTimeSeries(rm));
}

TEST(IsEmptyTimeSeries, EmptyMetricData) {
  opentelemetry::sdk::metrics::MetricData md;
  opentelemetry::sdk::metrics::ScopeMetrics sm;
  sm.metric_data_.push_back(std::move(md));
  opentelemetry::sdk::metrics::ResourceMetrics rm;
  rm.scope_metric_data_.push_back(std::move(sm));
  EXPECT_TRUE(IsEmptyTimeSeries(rm));
}

TEST(IsEmptyTimeSeries, NonEmptyPointData) {
  opentelemetry::sdk::metrics::PointDataAttributes pda;
  opentelemetry::sdk::metrics::MetricData md;
  md.point_data_attr_.push_back(std::move(pda));
  opentelemetry::sdk::metrics::ScopeMetrics sm;
  sm.metric_data_.push_back(std::move(md));
  opentelemetry::sdk::metrics::ResourceMetrics rm;
  rm.scope_metric_data_.push_back(std::move(sm));
  EXPECT_FALSE(IsEmptyTimeSeries(rm));
}

TEST(ToTimeSeries, Sum) {
  opentelemetry::sdk::metrics::PointDataAttributes pda;
  pda.point_data = opentelemetry::sdk::metrics::SumPointData{};

  opentelemetry::sdk::metrics::MetricData md;
  md.point_data_attr_.push_back(std::move(pda));
  md.instrument_descriptor.name_ = "metric-name";
  md.instrument_descriptor.unit_ = "unit";
  md.instrument_descriptor.type_ = {};
  md.instrument_descriptor.value_type_ = {};

  opentelemetry::sdk::metrics::ScopeMetrics sm;
  sm.metric_data_.push_back(md);
  sm.metric_data_.push_back(std::move(md));

  opentelemetry::sdk::metrics::ResourceMetrics rm;
  rm.scope_metric_data_.push_back(std::move(sm));
  rm.resource_ = nullptr;

  auto tss = ToTimeSeries(rm, PrefixWithWorkload);
  EXPECT_THAT(tss, ElementsAre(SumTimeSeries(), SumTimeSeries()));
  EXPECT_THAT(tss, Each(AllOf(MetricType("workload.googleapis.com/metric-name"),
                              Unit("unit"))));
}

TEST(ToTimeSeries, Gauge) {
  opentelemetry::sdk::metrics::PointDataAttributes pda;
  pda.point_data = opentelemetry::sdk::metrics::LastValuePointData{};

  opentelemetry::sdk::metrics::MetricData md;
  md.point_data_attr_.push_back(std::move(pda));
  md.instrument_descriptor.name_ = "metric-name";
  md.instrument_descriptor.unit_ = "unit";
  md.instrument_descriptor.type_ = {};
  md.instrument_descriptor.value_type_ = {};

  opentelemetry::sdk::metrics::ScopeMetrics sm;
  sm.metric_data_.push_back(md);
  sm.metric_data_.push_back(std::move(md));

  opentelemetry::sdk::metrics::ResourceMetrics rm;
  rm.scope_metric_data_.push_back(std::move(sm));
  rm.resource_ = nullptr;

  auto tss = ToTimeSeries(rm, PrefixWithWorkload);
  EXPECT_THAT(tss, ElementsAre(GaugeTimeSeries(), GaugeTimeSeries()));
  EXPECT_THAT(tss, Each(AllOf(MetricType("workload.googleapis.com/metric-name"),
                              Unit("unit"))));
}

TEST(ToTimeSeries, Histogram) {
  opentelemetry::sdk::metrics::PointDataAttributes pda;
  pda.point_data = opentelemetry::sdk::metrics::HistogramPointData{};

  opentelemetry::sdk::metrics::MetricData md;
  md.point_data_attr_.push_back(std::move(pda));
  md.instrument_descriptor.name_ = "metric-name";
  md.instrument_descriptor.unit_ = "unit";
  md.instrument_descriptor.type_ = {};
  md.instrument_descriptor.value_type_ = {};

  opentelemetry::sdk::metrics::ScopeMetrics sm;
  sm.metric_data_.push_back(md);
  sm.metric_data_.push_back(std::move(md));

  opentelemetry::sdk::metrics::ResourceMetrics rm;
  rm.scope_metric_data_.push_back(std::move(sm));
  rm.resource_ = nullptr;

  auto tss = ToTimeSeries(rm, PrefixWithWorkload);
  EXPECT_THAT(tss, ElementsAre(HistogramTimeSeries(), HistogramTimeSeries()));
  EXPECT_THAT(tss, Each(AllOf(MetricType("workload.googleapis.com/metric-name"),
                              Unit("unit"))));
}

TEST(ToTimeSeries, DropIgnored) {
  opentelemetry::sdk::metrics::PointDataAttributes pda;
  pda.point_data = opentelemetry::sdk::metrics::DropPointData{};

  opentelemetry::sdk::metrics::MetricData md;
  md.point_data_attr_.push_back(std::move(pda));
  md.instrument_descriptor.name_ = "metric-name";
  md.instrument_descriptor.unit_ = "unit";
  md.instrument_descriptor.type_ = {};
  md.instrument_descriptor.value_type_ = {};

  opentelemetry::sdk::metrics::ScopeMetrics sm;
  sm.metric_data_.push_back(md);
  sm.metric_data_.push_back(std::move(md));

  opentelemetry::sdk::metrics::ResourceMetrics rm;
  rm.scope_metric_data_.push_back(std::move(sm));
  rm.resource_ = nullptr;

  auto tss = ToTimeSeries(rm, PrefixWithWorkload);
  EXPECT_THAT(tss, IsEmpty());
}

TEST(ToTimeSeries, Combined) {
  opentelemetry::sdk::metrics::PointDataAttributes sum_pda;
  sum_pda.point_data = opentelemetry::sdk::metrics::SumPointData{};
  opentelemetry::sdk::metrics::PointDataAttributes gauge_pda;
  gauge_pda.point_data = opentelemetry::sdk::metrics::LastValuePointData{};
  opentelemetry::sdk::metrics::PointDataAttributes histogram_pda;
  histogram_pda.point_data = opentelemetry::sdk::metrics::HistogramPointData{};
  opentelemetry::sdk::metrics::PointDataAttributes drop_pda;
  drop_pda.point_data = opentelemetry::sdk::metrics::DropPointData{};

  opentelemetry::sdk::metrics::MetricData md;
  md.point_data_attr_.push_back(std::move(sum_pda));
  md.point_data_attr_.push_back(std::move(gauge_pda));
  md.point_data_attr_.push_back(std::move(histogram_pda));
  md.point_data_attr_.push_back(std::move(drop_pda));
  md.instrument_descriptor.name_ = "metric-name";
  md.instrument_descriptor.unit_ = "unit";
  md.instrument_descriptor.type_ = {};
  md.instrument_descriptor.value_type_ = {};

  opentelemetry::sdk::metrics::ScopeMetrics sm;
  sm.metric_data_.push_back(std::move(md));

  opentelemetry::sdk::metrics::ResourceMetrics rm;
  rm.scope_metric_data_.push_back(std::move(sm));
  rm.resource_ = nullptr;

  auto tss = ToTimeSeries(
      rm, [](std::string const& s) { return "custom.googleapis.com/" + s; });
  EXPECT_THAT(tss, UnorderedElementsAre(SumTimeSeries(), GaugeTimeSeries(),
                                        HistogramTimeSeries()));
  EXPECT_THAT(tss, Each(AllOf(MetricType("custom.googleapis.com/metric-name"),
                              Unit("unit"))));
}

TEST(ToRequests, Simple) {
  Project p("test-project");

  google::api::MonitoredResource mr;
  mr.set_type("gcs_client");

  google::monitoring::v3::TimeSeries ts;
  ts.set_unit("unit");

  auto constexpr kExpectedRequest = R"pb(
    name: "projects/test-project"
    time_series {
      unit: "unit"
      resource { type: "gcs_client" }
    }
    time_series {
      unit: "unit"
      resource { type: "gcs_client" }
    }
  )pb";
  google::monitoring::v3::CreateTimeSeriesRequest expected;
  EXPECT_TRUE(TextFormat::ParseFromString(kExpectedRequest, &expected));

  auto requests = ToRequests(p.FullName(), mr, {ts, ts});
  EXPECT_THAT(requests, ElementsAre(IsProtoEqual(expected)));
}

TEST(ToRequests, Batches) {
  // Use an alias for readability.
  auto const batch_limit = kMaxTimeSeriesPerRequest;

  struct TestCase {
    int time_series_count;
    std::vector<int> batch_sizes;
  };
  std::vector<TestCase> cases = {
      {0, {}},
      {1, {1}},
      {batch_limit, {batch_limit}},
      {batch_limit + 1, {batch_limit, 1}},
      {2 * batch_limit + 1, {batch_limit, batch_limit, 1}},
  };

  for (auto const& c : cases) {
    std::vector<google::monitoring::v3::TimeSeries> tss(c.time_series_count);
    auto requests = ToRequests("project", {}, std::move(tss));

    std::vector<int> batch_sizes;
    batch_sizes.reserve(requests.size());
    for (auto& r : requests) batch_sizes.push_back(r.time_series_size());
    EXPECT_THAT(batch_sizes, ElementsAreArray(c.batch_sizes));
  }
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace otel_internal
}  // namespace cloud
}  // namespace google
