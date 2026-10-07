// Copyright 2026 Google LLC
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

#ifdef GOOGLE_CLOUD_CPP_STORAGE_WITH_OTEL_METRICS

#include "google/cloud/storage/internal/grpc/channel_metrics.h"
#include "google/cloud/storage/grpc_plugin.h"
#include "google/cloud/storage/internal/grpc/channel_telemetry.h"
#include "google/cloud/storage/internal/grpc/default_options.h"
#include "google/cloud/storage/internal/grpc/metrics_exporter_impl.h"
#include "google/cloud/storage/internal/grpc/metrics_histograms.h"
#include "google/cloud/storage/internal/grpc/metrics_meter_provider.h"
#include "google/cloud/storage/options.h"
#include "google/cloud/common_options.h"
#include "google/cloud/credentials.h"
#include "google/cloud/options.h"
#include "google/cloud/testing_util/mock_opentelemetry_metrics.h"
#include "google/cloud/testing_util/scoped_environment.h"
#include <gmock/gmock.h>
#include <opentelemetry/sdk/metrics/export/metric_producer.h>
#include <opentelemetry/sdk/metrics/push_metric_exporter.h>
#include <opentelemetry/sdk/resource/resource.h>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <thread>
#include <utility>
#include <variant>

namespace google {
namespace cloud {
namespace storage_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::google::cloud::testing_util::MockMeter;
using ::google::cloud::testing_util::MockMeterProvider;
using ::google::cloud::testing_util::ScopedEnvironment;
using ::testing::AllOf;
using ::testing::AtLeast;
using ::testing::ByMove;
using ::testing::Contains;
using ::testing::DoubleNear;
using ::testing::ElementsAreArray;
using ::testing::Ge;
using ::testing::IsEmpty;
using ::testing::Not;
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

auto constexpr kExportInterval = std::chrono::milliseconds(50);
auto constexpr kExportTimeout = std::chrono::milliseconds(25);

auto TestReaderOptions() {
  opentelemetry::sdk::metrics::PeriodicExportingMetricReaderOptions
      reader_options;
  reader_options.export_interval_millis = kExportInterval;
  reader_options.export_timeout_millis = kExportTimeout;
  return reader_options;
}

auto MatchesLatencyBoundaries() {
  return ResultOf(
      "boundaries are latency boundaries",
      [](opentelemetry::sdk::metrics::HistogramPointData const& hpd) {
        return hpd.boundaries_;
      },
      ElementsAreArray(MakeLatencyHistogramBoundaries()));
}

/// Matches a histogram whose every recorded value is @p expected_seconds.
///
/// The reader uses cumulative temporality and the test records the same value
/// until the first export, so `sum_` is a multiple of the value rather than
/// the value itself. Checking the mean catches a wrong duration-to-seconds
/// conversion, which the bucket boundaries alone would not.
auto MatchesRecordedSeconds(double expected_seconds) {
  return AllOf(
      ResultOf(
          "count",
          [](opentelemetry::sdk::metrics::HistogramPointData const& hpd) {
            return hpd.count_;
          },
          Ge(std::uint64_t{1})),
      ResultOf(
          "mean of recorded values",
          [](opentelemetry::sdk::metrics::HistogramPointData const& hpd) {
            return std::get<double>(hpd.sum_) / static_cast<double>(hpd.count_);
          },
          DoubleNear(expected_seconds, 1e-9)));
}

auto MatchesTransportType(std::string value) {
  return ResultOf(
      "transport_type attribute",
      [](opentelemetry::sdk::metrics::PointDataAttributes const& pda) {
        auto const& attributes = pda.attributes.GetAttributes();
        auto const l = attributes.find("transport_type");
        if (l == attributes.end()) return std::string{};
        return std::get<std::string>(l->second);
      },
      std::move(value));
}

auto ExpectedPointData(std::string transport_type, double expected_seconds) {
  return AllOf(
      MatchesTransportType(std::move(transport_type)),
      ResultOf(
          "data is histogram with latency boundaries",
          [](opentelemetry::sdk::metrics::PointDataAttributes const& pda) {
            return pda.point_data;
          },
          VariantWith<opentelemetry::sdk::metrics::HistogramPointData>(
              AllOf(MatchesLatencyBoundaries(),
                    MatchesRecordedSeconds(expected_seconds)))));
}

template <typename Matcher>
auto WithPointData(Matcher&& matcher) {
  return ResultOf(
      "point_data_attr_",
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

auto MatchesInstrumentUnit(std::string unit) {
  return ResultOf(
      "instrument descriptor unit",
      [](opentelemetry::sdk::metrics::MetricData const& md) {
        return md.instrument_descriptor.unit_;
      },
      std::move(unit));
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

/// @test Verify a null meter provider yields no callback.
///
/// Recording into a null provider would crash. An empty callback also means
/// `StartChannelTelemetry()` skips the work entirely.
TEST(ChannelMetrics, NoProviderYieldsEmptyCallback) {
  std::shared_ptr<opentelemetry::metrics::MeterProvider> provider;
  EXPECT_FALSE(MakeChannelMetricsCallback(provider));
}

/// @test Verify a provider that returns a null meter yields no callback.
///
/// Creating the instrument would dereference the null meter. This mirrors
/// `HedgedReadMetricsTest.NoMeterRecordsNothing`.
TEST(ChannelMetrics, NullMeterYieldsEmptyCallback) {
  auto mock = std::make_shared<MockMeterProvider>();
  EXPECT_CALL(*mock, GetMeter)
      .WillOnce(Return(
          opentelemetry::nostd::shared_ptr<opentelemetry::metrics::Meter>{
              nullptr}));
  std::shared_ptr<opentelemetry::metrics::MeterProvider> provider =
      std::move(mock);
  EXPECT_FALSE(MakeChannelMetricsCallback(provider));
}

/// @test Verify a meter that returns a null histogram yields no callback.
///
/// `Meter::CreateDoubleHistogram()` returns an owning pointer with no
/// non-null guarantee. Returning a callback would defer the null dereference
/// to the completion queue thread, once the channel becomes ready.
TEST(ChannelMetrics, NullHistogramYieldsEmptyCallback) {
  auto mock_meter = std::make_shared<MockMeter>();
  EXPECT_CALL(*mock_meter, CreateDoubleHistogram)
      .WillOnce(
          Return(ByMove(opentelemetry::nostd::unique_ptr<
                        opentelemetry::metrics::Histogram<double>>{nullptr})));
  auto mock = std::make_shared<MockMeterProvider>();
  EXPECT_CALL(*mock, GetMeter)
      .WillOnce(Return(
          opentelemetry::nostd::shared_ptr<opentelemetry::metrics::Meter>{
              mock_meter}));
  std::shared_ptr<opentelemetry::metrics::MeterProvider> provider =
      std::move(mock);
  EXPECT_FALSE(MakeChannelMetricsCallback(provider));
}

/// @test Verify an authority with no registered provider yields no callback.
///
/// `EnableGrpcMetricsImpl()` never ran for this authority, which is what
/// happens when the customer disables metrics or when no monitoring project
/// can be determined.
TEST(ChannelMetrics, UnknownAuthorityYieldsEmptyCallback) {
  Options const options =
      Options{}
          .set<AuthorityOption>("test-only-unknown.example.com")
          .set<storage_experimental::EnableGrpcMetricsOption>(true);
  EXPECT_FALSE(MakeChannelMetricsCallback(options));
}

/// @test Verify a client's own opt-out wins over a registered provider.
///
/// The registry is process-wide and keyed by authority. Once any client
/// enables metrics for an authority, a later client for the same authority
/// that sets `EnableGrpcMetricsOption(false)` must still record nothing,
/// even though `FindMeterProvider()` would succeed.
TEST(ChannelMetrics, RegisteredAuthorityHonorsOption) {
  // The registry is process-wide, so use an authority unique to this test.
  std::string const authority = "test-only-registered.example.com";
  ScopedEnvironment const env("GOOGLE_CLOUD_PROJECT", std::nullopt);
  Options const base = DefaultOptionsGrpc(
      Options{}
          .set<AuthorityOption>(authority)
          .set<storage::ProjectIdOption>("test-project")
          .set<UnifiedCredentialsOption>(MakeAccessTokenCredentials(
              "unused",
              std::chrono::system_clock::now() + std::chrono::minutes(15))));
  std::optional<ExporterConfig> config = MakeMeterProviderConfig(
      opentelemetry::sdk::resource::Resource::Create({}), base);
  ASSERT_TRUE(config.has_value());
  EnableGrpcMetricsImpl(*std::move(config));
  ASSERT_TRUE(FindMeterProvider(authority));

  struct TestCase {
    bool enabled;
    bool expect_callback;
  } const cases[] = {{true, true}, {false, false}};
  for (auto const& t : cases) {
    SCOPED_TRACE(std::string("EnableGrpcMetricsOption=") +
                 (t.enabled ? "true" : "false"));
    Options options = base;
    options.set<storage_experimental::EnableGrpcMetricsOption>(t.enabled);
    EXPECT_EQ(static_cast<bool>(MakeChannelMetricsCallback(options)),
              t.expect_callback);
  }
}

/// @test Verify a recorded latency reaches the exporter as a histogram.
///
/// This is the end-to-end path: instrument name and unit, the
/// `transport_type` label, the latency bucket boundaries, and the recorded
/// value in seconds are all part of the contract with Cloud Monitoring, so
/// each is asserted rather than assumed.
TEST(ChannelMetrics, RecordsChannelCreationLatency) {
  auto constexpr kElapsed = std::chrono::milliseconds(250);
  auto constexpr kExpectedSeconds = 0.25;
  std::atomic<int> export_count{0};
  auto mock = std::make_unique<MockPushMetricExporter>();
  EXPECT_CALL(*mock, Shutdown).WillOnce(Return(true));
  EXPECT_CALL(*mock, GetAggregationTemporality)
      .WillRepeatedly(Return(
          opentelemetry::sdk::metrics::AggregationTemporality::kCumulative));
  // The exporter may export some empty data, we just ignore those.
  EXPECT_CALL(*mock, Export(MetricDataEmpty()))
      .WillRepeatedly(
          Return(opentelemetry::sdk::common::ExportResult::kSuccess));
  EXPECT_CALL(*mock, Export(MetricDataNotEmpty()))
      .Times(AtLeast(1))
      .WillRepeatedly(
          [&export_count](
              opentelemetry::sdk::metrics::ResourceMetrics const& data) {
            EXPECT_THAT(
                data.scope_metric_data_,
                Contains(WithMetricsData(Contains(AllOf(
                    MatchesInstrumentName(ChannelCreationLatencyInstrument()),
                    MatchesInstrumentUnit("s"),
                    WithPointData(Contains(ExpectedPointData(
                        "DirectPathInterconnect", kExpectedSeconds))))))));
            ++export_count;
            return opentelemetry::sdk::common::ExportResult::kSuccess;
          });

  // Use a new scope to force a flush from the meter and provider before the
  // function exits. Otherwise the mocks may not be called in time.
  {
    auto provider = MakeGrpcMeterProvider(std::move(mock), TestReaderOptions());
    ChannelReadyCallback const callback = MakeChannelMetricsCallback(provider);
    ASSERT_TRUE(callback);
    // It may take several attempts before the periodic reader exports any
    // data. We do 50 iterations to minimize flakes: each iteration should be
    // enough to succeed, so we are giving this 50 chances to succeed.
    for (int i = 0; i != 50 && export_count.load() == 0; ++i) {
      callback(TransportType::kDirectPathInterconnect, kElapsed);
      std::this_thread::sleep_for(kExportInterval);
    }
  }
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_STORAGE_WITH_OTEL_METRICS
