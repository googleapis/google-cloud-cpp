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

#include "google/cloud/storage/internal/payload_metrics.h"
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
using ::testing::Eq;
using ::testing::Pair;
using ::testing::Return;
using ::testing::UnorderedElementsAre;

using HistPtr = opentelemetry::nostd::unique_ptr<
    opentelemetry::metrics::Histogram<std::uint64_t>>;

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

TEST(PayloadMetricsTest, RecordsRequestAndResponseBodySizesWithAttributes) {
  std::map<std::string, std::string> req_attrs;
  auto req_hist = std::make_unique<MockHistogram<std::uint64_t>>();
  EXPECT_CALL(*req_hist, Record(std::uint64_t{4096}, _, _))
      .WillOnce([&](std::uint64_t,
                    opentelemetry::common::KeyValueIterable const& kv,
                    opentelemetry::context::Context const& ctx) {
        opentelemetry::context::ContextValue const val =
            ctx.GetValue("trace_marker");
        auto const* marker = opentelemetry::nostd::get_if<std::int64_t>(&val);
        ASSERT_NE(marker, nullptr);
        EXPECT_EQ(*marker, 42);
        req_attrs = ExtractStringAttrs(kv);
      });
  constexpr std::uint64_t kLargeBytes = std::uint64_t{5} << 30;
  EXPECT_CALL(*req_hist, Record(std::uint64_t{0}, _, _)).Times(1);
  EXPECT_CALL(*req_hist, Record(kLargeBytes, _, _)).Times(1);

  std::map<std::string, std::string> resp_attrs;
  auto resp_hist = std::make_unique<MockHistogram<std::uint64_t>>();
  EXPECT_CALL(*resp_hist, Record(std::uint64_t{8192}, _, _))
      .WillOnce([&](std::uint64_t,
                    opentelemetry::common::KeyValueIterable const& kv,
                    opentelemetry::context::Context const& ctx) {
        opentelemetry::context::ContextValue const val =
            ctx.GetValue("trace_marker");
        auto const* marker = opentelemetry::nostd::get_if<std::int64_t>(&val);
        ASSERT_NE(marker, nullptr);
        EXPECT_EQ(*marker, 42);
        resp_attrs = ExtractStringAttrs(kv);
      });
  EXPECT_CALL(*resp_hist, Record(std::uint64_t{0}, _, _)).Times(1);

  opentelemetry::nostd::shared_ptr<MockMeter> meter =
      std::make_shared<MockMeter>();
  EXPECT_CALL(*meter,
              CreateUInt64Histogram(Eq("gcp.storage.client.request.body.size"),
                                    _, Eq("By")))
      .WillOnce(Return(ByMove(HistPtr(req_hist.release()))));
  EXPECT_CALL(*meter,
              CreateUInt64Histogram(Eq("gcp.storage.client.response.body.size"),
                                    _, Eq("By")))
      .WillOnce(Return(ByMove(HistPtr(resp_hist.release()))));

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

  PayloadMetrics metrics(
      opentelemetry::nostd::shared_ptr<opentelemetry::metrics::MeterProvider>(
          provider),
      "private.googleapis.com", "http");

  opentelemetry::context::Context const ctx("trace_marker", std::int64_t{42});
  metrics.RecordRequestBodySize({"WriteObject", "Client::WriteObject"}, 4096,
                                ctx);
  metrics.RecordRequestBodySize({"WriteObject", "Client::WriteObject"}, 0, ctx);
  metrics.RecordRequestBodySize({"WriteObject", "Client::WriteObject"},
                                kLargeBytes, ctx);
  metrics.RecordResponseBodySize({"ReadObject", "Client::ReadObject"}, 8192,
                                 ctx);
  metrics.RecordResponseBodySize({"ReadObject", "Client::ReadObject"}, 0, ctx);

  std::string const expected_version =
      std::to_string(google::cloud::version_major()) + "." +
      std::to_string(google::cloud::version_minor()) + "." +
      std::to_string(google::cloud::version_patch());

  EXPECT_THAT(req_attrs, UnorderedElementsAre(
                             Pair("gcp.client.version", expected_version),
                             Pair("gcp.client.artifact", "google-cloud-cpp"),
                             Pair("gcp.client.service", "storage"),
                             Pair("server.address", "private.googleapis.com"),
                             Pair("rpc.system.name", "http"),
                             Pair("gcp.storage.operation", "WriteObject"),
                             Pair("gcp.client.method", "Client::WriteObject")));

  EXPECT_THAT(resp_attrs, UnorderedElementsAre(
                              Pair("gcp.client.version", expected_version),
                              Pair("gcp.client.artifact", "google-cloud-cpp"),
                              Pair("gcp.client.service", "storage"),
                              Pair("server.address", "private.googleapis.com"),
                              Pair("rpc.system.name", "http"),
                              Pair("gcp.storage.operation", "ReadObject"),
                              Pair("gcp.client.method", "Client::ReadObject")));
}

TEST(PayloadMetricsTest, NullProviderAndNullMeterAreNoOps) {
  PayloadMetrics null_provider(nullptr, "storage.googleapis.com", "http");
  opentelemetry::context::Context const ctx;
  null_provider.RecordRequestBodySize({"WriteObject", "Client::WriteObject"},
                                      1024, ctx);
  null_provider.RecordResponseBodySize({"ReadObject", "Client::ReadObject"},
                                       1024, ctx);

  auto provider = std::make_shared<MockMeterProvider>();
  EXPECT_CALL(*provider, GetMeter)
      .WillOnce(Return(
          opentelemetry::nostd::shared_ptr<opentelemetry::metrics::Meter>{}));
  PayloadMetrics null_meter(
      opentelemetry::nostd::shared_ptr<opentelemetry::metrics::MeterProvider>(
          provider),
      "storage.googleapis.com", "http");
  null_meter.RecordRequestBodySize({"WriteObject", "Client::WriteObject"}, 1024,
                                   ctx);
  null_meter.RecordResponseBodySize({"ReadObject", "Client::ReadObject"}, 1024,
                                    ctx);
}

TEST(PayloadMetricsTest, NullAndPartialInstrumentsAreSafe) {
  auto make_provider =
      [](opentelemetry::nostd::shared_ptr<MockMeter> const& meter) {
        auto provider = std::make_shared<MockMeterProvider>();
        EXPECT_CALL(*provider, GetMeter)
            .WillOnce(Return(
                opentelemetry::nostd::shared_ptr<opentelemetry::metrics::Meter>(
                    meter)));
        return opentelemetry::nostd::shared_ptr<
            opentelemetry::metrics::MeterProvider>(provider);
      };

  opentelemetry::context::Context const ctx;

  // Both instruments null.
  opentelemetry::nostd::shared_ptr<MockMeter> empty_meter =
      std::make_shared<MockMeter>();
  EXPECT_CALL(*empty_meter, CreateUInt64Histogram)
      .WillOnce(Return(ByMove(HistPtr{})))
      .WillOnce(Return(ByMove(HistPtr{})));
  PayloadMetrics both_null(make_provider(empty_meter), "storage.googleapis.com",
                           "grpc");
  both_null.RecordRequestBodySize({"WriteObject", "Client::WriteObject"}, 64,
                                  ctx);
  both_null.RecordResponseBodySize({"ReadObject", "Client::ReadObject"}, 64,
                                   ctx);

  // Only request_body_size_ non-null.
  auto req_hist = std::make_unique<MockHistogram<std::uint64_t>>();
  EXPECT_CALL(*req_hist, Record(std::uint64_t{64}, _, _)).Times(1);
  opentelemetry::nostd::shared_ptr<MockMeter> req_only_meter =
      std::make_shared<MockMeter>();
  EXPECT_CALL(
      *req_only_meter,
      CreateUInt64Histogram(Eq("gcp.storage.client.request.body.size"), _, _))
      .WillOnce(Return(ByMove(HistPtr(req_hist.release()))));
  EXPECT_CALL(
      *req_only_meter,
      CreateUInt64Histogram(Eq("gcp.storage.client.response.body.size"), _, _))
      .WillOnce(Return(ByMove(HistPtr{})));
  PayloadMetrics req_only(make_provider(req_only_meter),
                          "storage.googleapis.com", "grpc");
  req_only.RecordRequestBodySize({"WriteObject", "Client::WriteObject"}, 64,
                                 ctx);
  req_only.RecordResponseBodySize({"ReadObject", "Client::ReadObject"}, 64,
                                  ctx);

  // Only response_body_size_ non-null.
  auto resp_hist = std::make_unique<MockHistogram<std::uint64_t>>();
  EXPECT_CALL(*resp_hist, Record(std::uint64_t{128}, _, _)).Times(1);
  opentelemetry::nostd::shared_ptr<MockMeter> resp_only_meter =
      std::make_shared<MockMeter>();
  EXPECT_CALL(
      *resp_only_meter,
      CreateUInt64Histogram(Eq("gcp.storage.client.request.body.size"), _, _))
      .WillOnce(Return(ByMove(HistPtr{})));
  EXPECT_CALL(
      *resp_only_meter,
      CreateUInt64Histogram(Eq("gcp.storage.client.response.body.size"), _, _))
      .WillOnce(Return(ByMove(HistPtr(resp_hist.release()))));
  PayloadMetrics resp_only(make_provider(resp_only_meter),
                           "storage.googleapis.com", "grpc");
  resp_only.RecordRequestBodySize({"WriteObject", "Client::WriteObject"}, 128,
                                  ctx);
  resp_only.RecordResponseBodySize({"ReadObject", "Client::ReadObject"}, 128,
                                   ctx);
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_internal
}  // namespace cloud
}  // namespace google
