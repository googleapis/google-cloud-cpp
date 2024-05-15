// Copyright 2024 Google LLC
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

#include "google/cloud/storage/internal/grpc/metrics_meter_provider.h"
#include "google/cloud/storage/grpc_plugin.h"
#include "google/cloud/storage/internal/grpc/default_options.h"
#include "google/cloud/storage/internal/grpc/metrics_histograms.h"
#include <gmock/gmock.h>
#include <grpcpp/grpcpp.h>
#include <opentelemetry/metrics/meter.h>
#include <opentelemetry/metrics/meter_provider.h>
#include <opentelemetry/metrics/sync_instruments.h>
#include <opentelemetry/sdk/metrics/export/metric_producer.h>
#include <opentelemetry/sdk/metrics/push_metric_exporter.h>
#include <opentelemetry/sdk/resource/resource.h>
#include <chrono>
#include <thread>

namespace google {
namespace cloud {
namespace storage_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::testing::Contains;
using ::testing::ElementsAre;
using ::testing::ElementsAreArray;
using ::testing::IsEmpty;
using ::testing::ResultOf;
using ::testing::Return;
using ::testing::VariantWith;

class MockPushMetricExporter
    : public opentelemetry::sdk::metrics::PushMetricExporter {
 public:
  // NOLINTBEGIN(bugprone-exception-escape)
  MOCK_METHOD(opentelemetry::sdk::common::ExportResult, Export,
              (opentelemetry::sdk::metrics::ResourceMetrics const&),
              (noexcept, override));

  MOCK_METHOD(opentelemetry::sdk::metrics::AggregationTemporality,
              GetAggregationTemporality,
              (opentelemetry::sdk::metrics::InstrumentType),
              (const, noexcept, override));

  MOCK_METHOD(bool, ForceFlush, (std::chrono::microseconds),
              (noexcept, override));
  MOCK_METHOD(bool, Shutdown, (std::chrono::microseconds),
              (noexcept, override));
  // NOLINTEND(bugprone-exception-escape)
};

auto constexpr kDurationMetric = "grpc.client.attempt.duration";
auto constexpr kRcvdSizeMetric =
    "grpc.client.attempt.rcvd_total_compressed_message_size";
auto constexpr kSentSizeMetric =
    "grpc.client.attempt.sent_total_compressed_message_size";

auto MatchesLatencyBoundaries() {
  return ResultOf(
      "boundaries are latency boundaries",
      [](opentelemetry::sdk::metrics::HistogramPointData const& hpd) {
        return hpd.boundaries_;
      },
      ElementsAreArray(MakeLatencyHistogramBoundaries()));
}

auto ExpectedLatencyHistogram() {
  return ResultOf(
      "data is histogram with right boundaries",
      [](opentelemetry::sdk::metrics::PointDataAttributes const& pda) {
        return pda.point_data;
      },
      VariantWith<opentelemetry::sdk::metrics::HistogramPointData>(
          MatchesLatencyBoundaries()));
}

auto MatchesSizeBoundaries() {
  return ResultOf(
      "boundaries are size boundaries",
      [](opentelemetry::sdk::metrics::HistogramPointData const& hpd) {
        return hpd.boundaries_;
      },
      ElementsAreArray(MakeSizeHistogramBoundaries()));
}

auto ExpectedSizeHistogram() {
  return ResultOf(
      "data is histogram with right boundaries",
      [](opentelemetry::sdk::metrics::PointDataAttributes const& pda) {
        return pda.point_data;
      },
      VariantWith<opentelemetry::sdk::metrics::HistogramPointData>(
          MatchesSizeBoundaries()));
}

template <typename Matcher>
auto WithPointData(Matcher&& matcher) {
  return ResultOf(
      "instrument descriptor name",
      [](opentelemetry::sdk::metrics::MetricData const& md) {
        return md.point_data_attr_;
      },
      std::forward<Matcher>(matcher));
}

template <typename Matcher>
auto WithMetricsData(Matcher&& matcher) {
  return ResultOf(
      "metric_data_",
      [](opentelemetry::sdk::metrics::ScopeMetrics const& sm) {
        return sm.metric_data_;
      },
      std::forward<Matcher>(matcher));
}

auto MatchesInstrumentName(std::string name) {
  return ResultOf(
      "instrument descriptor name",
      [](opentelemetry::sdk::metrics::MetricData const& md) {
        return md.instrument_descriptor.name_;
      },
      std::move(name));
}

auto MetricDataEmpty() {
  return ResultOf(
      "scope_metric_data_",
      [](opentelemetry::sdk::metrics::ResourceMetrics const& data) {
        return data.scope_metric_data_;
      },
      IsEmpty());
}

auto MetricDataNotEmpty() {
  return ResultOf(
      "scope_metric_data_",
      [](opentelemetry::sdk::metrics::ResourceMetrics const& data) {
        return data.scope_metric_data_;
      },
      Not(IsEmpty()));
}

auto constexpr kExportInterval = std::chrono::milliseconds(5);

auto TestReaderOptions() {
  opentelemetry::sdk::metrics::PeriodicExportingMetricReaderOptions
      reader_options;
  reader_options.export_interval_millis = kExportInterval;
  reader_options.export_timeout_millis =
      kExportInterval - std::chrono::milliseconds(1);
  return reader_options;
}

TEST(GrpcMetricsExporter, ValidateGrpcClientAttemptDuration) {
  auto mock = std::make_unique<MockPushMetricExporter>();
  EXPECT_CALL(*mock, Shutdown).WillOnce(Return(true));
  EXPECT_CALL(*mock, GetAggregationTemporality)
      .WillRepeatedly(Return(
          opentelemetry::sdk::metrics::AggregationTemporality::kCumulative));
  // The exporter may export some empty data, we just ignore those.
  EXPECT_CALL(*mock, Export(MetricDataEmpty()))
      .WillRepeatedly(
          Return(opentelemetry::sdk::common::ExportResult::kSuccess));
  // We use a different condition on the call to validate the real data.
  EXPECT_CALL(*mock, Export(MetricDataNotEmpty()))
      .Times(::testing::AtLeast(1))
      .WillRepeatedly(
          [](opentelemetry::sdk::metrics::ResourceMetrics const& data) {
            EXPECT_THAT(
                data.scope_metric_data_,
                Contains(WithMetricsData(Contains(AllOf(
                    MatchesInstrumentName(kDurationMetric),
                    WithPointData(ElementsAre(ExpectedLatencyHistogram())))))));
            return opentelemetry::sdk::common::ExportResult::kSuccess;
          });

  // Use a new scope to force a flush from the meter and provider before the
  // function exit.  Otherwise the mocks may not be called in time.
  {
    auto provider = MakeGrpcMeterProvider(std::move(mock), TestReaderOptions());
    auto meter = provider->GetMeter("grpc-c++", grpc::Version());
    auto histogram = meter->CreateDoubleHistogram(kDurationMetric,
                                                  "test-only-description", "s");
    // It may take several attempts before the periodic reader exports any data.
    // We do 50 iterations to minimize flakes: each iteration should be enough
    // to succeed, so we are giving this 50 chances to succeed. And the total
    // running time (about 250ms) is not too terrible.
    for (int i = 0; i != 50; ++i) {
      histogram->Record(1.0, opentelemetry::context::Context{});
      std::this_thread::sleep_for(kExportInterval);
    }
  }
}

TEST(GrpcMetricsExporter, ValidateGrpcClientAttemptSize) {
  for (std::string const size_metric : {kRcvdSizeMetric, kSentSizeMetric}) {
    SCOPED_TRACE("Testing with " + size_metric);
    auto mock = std::make_unique<MockPushMetricExporter>();
    EXPECT_CALL(*mock, Shutdown).WillOnce(Return(true));
    EXPECT_CALL(*mock, GetAggregationTemporality)
        .WillRepeatedly(Return(
            opentelemetry::sdk::metrics::AggregationTemporality::kCumulative));
    // The exporter may export some empty data, we just ignore those.
    EXPECT_CALL(*mock, Export(MetricDataEmpty()))
        .WillRepeatedly(
            Return(opentelemetry::sdk::common::ExportResult::kSuccess));
    // We use a different condition on the call to validate the real data.
    EXPECT_CALL(*mock, Export(MetricDataNotEmpty()))
        .Times(::testing::AtLeast(1))
        .WillRepeatedly(
            [size_metric](
                opentelemetry::sdk::metrics::ResourceMetrics const& data) {
              EXPECT_THAT(
                  data.scope_metric_data_,
                  Contains(WithMetricsData(Contains(AllOf(
                      MatchesInstrumentName(size_metric),
                      WithPointData(ElementsAre(ExpectedSizeHistogram())))))));
              return opentelemetry::sdk::common::ExportResult::kSuccess;
            });

    auto provider = MakeGrpcMeterProvider(std::move(mock), TestReaderOptions());
    auto meter = provider->GetMeter("grpc-c++", grpc::Version());
    auto histogram = meter->CreateDoubleHistogram(
        size_metric, "test-only-description", "By");
    // It may take several attempts before the periodic reader exports any data.
    // We do 50 iterations to minimize flakes: each iteration should be enough
    // to succeed, so we are giving this 50 chances to succeed. And the total
    // running time (about 250ms) is not too terrible.
    for (int i = 0; i != 50; ++i) {
      histogram->Record(1024.0, opentelemetry::context::Context{});
      std::this_thread::sleep_for(kExportInterval);
    }
  }
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_internal
}  // namespace cloud
}  // namespace google

#endif  //  GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY
