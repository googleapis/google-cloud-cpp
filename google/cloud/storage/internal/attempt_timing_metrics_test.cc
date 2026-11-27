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

#include "google/cloud/storage/internal/attempt_timing_metrics.h"
#include "google/cloud/testing_util/mock_opentelemetry_metrics.h"
#include "google/cloud/version.h"
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <opentelemetry/common/attribute_value.h>
#include <opentelemetry/common/key_value_iterable.h>
#include <opentelemetry/context/context.h>
#include <opentelemetry/metrics/meter.h>
#include <opentelemetry/metrics/meter_provider.h>
#include <opentelemetry/metrics/sync_instruments.h>
#include <opentelemetry/nostd/shared_ptr.h>
#include <opentelemetry/nostd/string_view.h>
#include <opentelemetry/nostd/unique_ptr.h>
#include <opentelemetry/nostd/variant.h>
#include <opentelemetry/version.h>
#include <chrono>
#include <cstdint>
#include <map>
#include <memory>
#include <string>

namespace google {
namespace cloud {
namespace storage_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::google::cloud::testing_util::MockHistogram;
using ::google::cloud::testing_util::MockMeter;
using ::google::cloud::testing_util::MockMeterProvider;
using ::testing::_;
using ::testing::ByMove;
using ::testing::DoubleEq;
using ::testing::Eq;
using ::testing::Pair;
using ::testing::Return;
using ::testing::UnorderedElementsAre;

using HistPtr =
    opentelemetry::nostd::unique_ptr<opentelemetry::metrics::Histogram<double>>;

std::map<std::string, std::string> ExtractStringAttrs(
    opentelemetry::common::KeyValueIterable const& iterable) {
  std::map<std::string, std::string> result;
  iterable.ForEachKeyValue(
      [&](opentelemetry::nostd::string_view key,
          opentelemetry::common::AttributeValue value) noexcept {
        if (auto const* sv =
                opentelemetry::nostd::get_if<opentelemetry::nostd::string_view>(
                    &value)) {
          result.emplace(std::string(key.data(), key.size()),
                         std::string(sv->data(), sv->size()));
        } else {
          ADD_FAILURE() << "Unexpected non-string attribute for key: "
                        << std::string(key.data(), key.size());
        }
        return true;
      });
  return result;
}

opentelemetry::nostd::shared_ptr<opentelemetry::metrics::MeterProvider>
MakeProvider(opentelemetry::nostd::shared_ptr<MockMeter> const& meter) {
  auto provider = std::make_shared<MockMeterProvider>();
  EXPECT_CALL(*provider, GetMeter)
      .WillOnce(Return(
          opentelemetry::nostd::shared_ptr<opentelemetry::metrics::Meter>(
              meter)));
  return opentelemetry::nostd::shared_ptr<
      opentelemetry::metrics::MeterProvider>(provider);
}

TEST(AttemptTimingMetricsTest, RecordsTtfbAndRpcCallDurationWithAttributes) {
  std::map<std::string, std::string> ttfb_attrs;
  auto ttfb = std::make_unique<MockHistogram<double>>();
  EXPECT_CALL(*ttfb, Record(DoubleEq(0.125), _, _))
      .WillOnce([&](double, opentelemetry::common::KeyValueIterable const& kv,
                    opentelemetry::context::Context const& ctx) {
        opentelemetry::context::ContextValue const val =
            ctx.GetValue("trace_marker");
        auto const* marker = opentelemetry::nostd::get_if<std::int64_t>(&val);
        ASSERT_NE(marker, nullptr);
        EXPECT_EQ(*marker, 42);
        ttfb_attrs = ExtractStringAttrs(kv);
      });

  std::map<std::string, std::string> rpc_attrs;
  auto rpc = std::make_unique<MockHistogram<double>>();
  EXPECT_CALL(*rpc, Record(DoubleEq(0.5), _, _))
      .WillOnce([&](double, opentelemetry::common::KeyValueIterable const& kv,
                    opentelemetry::context::Context const& ctx) {
        opentelemetry::context::ContextValue const val =
            ctx.GetValue("trace_marker");
        auto const* marker = opentelemetry::nostd::get_if<std::int64_t>(&val);
        ASSERT_NE(marker, nullptr);
        EXPECT_EQ(*marker, 42);
        rpc_attrs = ExtractStringAttrs(kv);
      });

  opentelemetry::nostd::shared_ptr<MockMeter> meter =
      std::make_shared<MockMeter>();
  EXPECT_CALL(*meter,
              CreateDoubleHistogram(Eq("gcp.storage.client.ttfb"), _, Eq("s")))
      .WillOnce(Return(ByMove(HistPtr(ttfb.release()))));
  EXPECT_CALL(*meter,
              CreateDoubleHistogram(Eq("rpc.client.call.duration"), _, Eq("s")))
      .WillOnce(Return(ByMove(HistPtr(rpc.release()))));

  auto provider = std::make_shared<MockMeterProvider>();
  EXPECT_CALL(*provider, GetMeter)
#if OPENTELEMETRY_ABI_VERSION_NO >= 2
      .WillOnce([&](opentelemetry::nostd::string_view scope,
                    opentelemetry::nostd::string_view scope_version,
                    opentelemetry::nostd::string_view,
                    opentelemetry::common::KeyValueIterable const*) {
#else
      .WillOnce([&](opentelemetry::nostd::string_view scope,
                    opentelemetry::nostd::string_view scope_version,
                    opentelemetry::nostd::string_view) {
#endif
        EXPECT_EQ(scope, "gl-cpp");
        EXPECT_EQ(scope_version, google::cloud::version_string());
        return opentelemetry::nostd::shared_ptr<opentelemetry::metrics::Meter>(
            meter);
      });

  AttemptTimingMetrics metrics(
      opentelemetry::nostd::shared_ptr<opentelemetry::metrics::MeterProvider>(
          provider),
      "private.googleapis.com");

  opentelemetry::context::Context const ctx("trace_marker", std::int64_t{42});
  metrics.RecordTtfb({"http", "ReadObject", "cloudpath"},
                     std::chrono::milliseconds(125), ctx);
  metrics.RecordRpcCallDuration({"google.storage.v2.Storage/ReadObject",
                                 "CANCELLED", "TIMEOUT", "directpath"},
                                std::chrono::milliseconds(500), ctx);

  std::string const expected_version =
      std::to_string(google::cloud::version_major()) + "." +
      std::to_string(google::cloud::version_minor()) + "." +
      std::to_string(google::cloud::version_patch());

  EXPECT_THAT(ttfb_attrs, UnorderedElementsAre(
                              Pair("gcp.client.version", expected_version),
                              Pair("gcp.client.artifact", "google-cloud-cpp"),
                              Pair("gcp.client.service", "storage"),
                              Pair("server.address", "private.googleapis.com"),
                              Pair("rpc.system.name", "http"),
                              Pair("gcp.storage.operation", "ReadObject"),
                              Pair("network.path", "cloudpath")));

  EXPECT_THAT(
      rpc_attrs,
      UnorderedElementsAre(
          Pair("gcp.client.version", expected_version),
          Pair("gcp.client.artifact", "google-cloud-cpp"),
          Pair("gcp.client.service", "storage"),
          Pair("server.address", "private.googleapis.com"),
          Pair("rpc.system.name", "grpc"),
          Pair("rpc.method", "google.storage.v2.Storage/ReadObject"),
          Pair("rpc.response.status_code", "CANCELLED"),
          Pair("error.type", "TIMEOUT"), Pair("network.path", "directpath")));
}

TEST(AttemptTimingMetricsTest, ClampsNegativeAndRecordsZeroDurations) {
  auto ttfb = std::make_unique<MockHistogram<double>>();
  EXPECT_CALL(*ttfb, Record(DoubleEq(0.0), _, _)).Times(2);

  auto rpc = std::make_unique<MockHistogram<double>>();
  EXPECT_CALL(*rpc, Record(DoubleEq(0.0), _, _)).Times(2);

  opentelemetry::nostd::shared_ptr<MockMeter> meter =
      std::make_shared<MockMeter>();
  EXPECT_CALL(*meter,
              CreateDoubleHistogram(Eq("gcp.storage.client.ttfb"), _, _))
      .WillOnce(Return(ByMove(HistPtr(ttfb.release()))));
  EXPECT_CALL(*meter,
              CreateDoubleHistogram(Eq("rpc.client.call.duration"), _, _))
      .WillOnce(Return(ByMove(HistPtr(rpc.release()))));

  AttemptTimingMetrics metrics(MakeProvider(meter), "storage.googleapis.com");
  opentelemetry::context::Context const ctx;
  metrics.RecordTtfb({"http", "ReadObject", "cloudpath"},
                     std::chrono::milliseconds(-5), ctx);
  metrics.RecordTtfb({"http", "ReadObject", "cloudpath"},
                     std::chrono::milliseconds(0), ctx);
  metrics.RecordRpcCallDuration(
      {"google.storage.v2.Storage/GetObject", "OK", "OK", "cloudpath"},
      std::chrono::milliseconds(-10), ctx);
  metrics.RecordRpcCallDuration(
      {"google.storage.v2.Storage/GetObject", "OK", "OK", "cloudpath"},
      std::chrono::milliseconds(0), ctx);
}

TEST(AttemptTimingMetricsTest, NullProviderAndNullMeterAreNoOps) {
  AttemptTimingMetrics null_provider(nullptr, "storage.googleapis.com");
  opentelemetry::context::Context const ctx;
  null_provider.RecordTtfb({"grpc", "ReadObject", "cloudpath"},
                           std::chrono::milliseconds(10), ctx);
  null_provider.RecordRpcCallDuration(
      {"google.storage.v2.Storage/ReadObject", "OK", "OK", "cloudpath"},
      std::chrono::milliseconds(10), ctx);

  auto provider = std::make_shared<MockMeterProvider>();
  EXPECT_CALL(*provider, GetMeter)
      .WillOnce(Return(
          opentelemetry::nostd::shared_ptr<opentelemetry::metrics::Meter>{}));
  AttemptTimingMetrics null_meter(
      opentelemetry::nostd::shared_ptr<opentelemetry::metrics::MeterProvider>(
          provider),
      "storage.googleapis.com");
  null_meter.RecordTtfb({"grpc", "ReadObject", "cloudpath"},
                        std::chrono::milliseconds(10), ctx);
  null_meter.RecordRpcCallDuration(
      {"google.storage.v2.Storage/ReadObject", "OK", "OK", "cloudpath"},
      std::chrono::milliseconds(10), ctx);
}

TEST(AttemptTimingMetricsTest, NullAndPartialInstrumentsAreSafe) {
  opentelemetry::context::Context const ctx;

  // Both instruments null.
  opentelemetry::nostd::shared_ptr<MockMeter> empty_meter =
      std::make_shared<MockMeter>();
  EXPECT_CALL(*empty_meter, CreateDoubleHistogram)
      .WillOnce(Return(ByMove(HistPtr{})))
      .WillOnce(Return(ByMove(HistPtr{})));
  AttemptTimingMetrics both_null(MakeProvider(empty_meter),
                                 "storage.googleapis.com");
  both_null.RecordTtfb({"grpc", "ReadObject", "cloudpath"},
                       std::chrono::milliseconds(5), ctx);
  both_null.RecordRpcCallDuration(
      {"google.storage.v2.Storage/ReadObject", "OK", "OK", "cloudpath"},
      std::chrono::milliseconds(5), ctx);

  // Only ttfb_ non-null.
  auto ttfb = std::make_unique<MockHistogram<double>>();
  EXPECT_CALL(*ttfb, Record(DoubleEq(0.01), _, _)).Times(1);
  opentelemetry::nostd::shared_ptr<MockMeter> ttfb_only_meter =
      std::make_shared<MockMeter>();
  EXPECT_CALL(*ttfb_only_meter,
              CreateDoubleHistogram(Eq("gcp.storage.client.ttfb"), _, _))
      .WillOnce(Return(ByMove(HistPtr(ttfb.release()))));
  EXPECT_CALL(*ttfb_only_meter,
              CreateDoubleHistogram(Eq("rpc.client.call.duration"), _, _))
      .WillOnce(Return(ByMove(HistPtr{})));
  AttemptTimingMetrics ttfb_only(MakeProvider(ttfb_only_meter),
                                 "storage.googleapis.com");
  ttfb_only.RecordTtfb({"grpc", "ReadObject", "cloudpath"},
                       std::chrono::milliseconds(10), ctx);
  ttfb_only.RecordRpcCallDuration(
      {"google.storage.v2.Storage/ReadObject", "OK", "OK", "cloudpath"},
      std::chrono::milliseconds(10), ctx);

  // Only rpc_call_duration_ non-null.
  auto rpc = std::make_unique<MockHistogram<double>>();
  EXPECT_CALL(*rpc, Record(DoubleEq(0.02), _, _)).Times(1);
  opentelemetry::nostd::shared_ptr<MockMeter> rpc_only_meter =
      std::make_shared<MockMeter>();
  EXPECT_CALL(*rpc_only_meter,
              CreateDoubleHistogram(Eq("gcp.storage.client.ttfb"), _, _))
      .WillOnce(Return(ByMove(HistPtr{})));
  EXPECT_CALL(*rpc_only_meter,
              CreateDoubleHistogram(Eq("rpc.client.call.duration"), _, _))
      .WillOnce(Return(ByMove(HistPtr(rpc.release()))));
  AttemptTimingMetrics rpc_only(MakeProvider(rpc_only_meter),
                                "storage.googleapis.com");
  rpc_only.RecordTtfb({"grpc", "ReadObject", "cloudpath"},
                      std::chrono::milliseconds(20), ctx);
  rpc_only.RecordRpcCallDuration(
      {"google.storage.v2.Storage/ReadObject", "OK", "OK", "cloudpath"},
      std::chrono::milliseconds(20), ctx);
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_internal
}  // namespace cloud
}  // namespace google
