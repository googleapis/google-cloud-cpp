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

#include "google/cloud/storage/internal/server_unreached_metrics.h"
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

using ::google::cloud::testing_util::MockCounter;
using ::google::cloud::testing_util::MockMeter;
using ::google::cloud::testing_util::MockMeterProvider;
using ::testing::_;
using ::testing::ByMove;
using ::testing::Eq;
using ::testing::Pair;
using ::testing::Return;
using ::testing::UnorderedElementsAre;

using CounterPtr = opentelemetry::nostd::unique_ptr<
    opentelemetry::metrics::Counter<std::uint64_t>>;

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

TEST(ServerUnreachedMetricsTest, RecordsServerUnreachedWithAttributes) {
  std::map<std::string, std::string> recorded_attrs;
  auto counter = std::make_unique<MockCounter<std::uint64_t>>();
  EXPECT_CALL(*counter, Add(std::uint64_t{1}, _, _))
      .WillOnce([&](std::uint64_t,
                    opentelemetry::common::KeyValueIterable const& kv,
                    opentelemetry::context::Context const& ctx) {
        opentelemetry::context::ContextValue const val =
            ctx.GetValue("trace_marker");
        auto const* marker = opentelemetry::nostd::get_if<std::int64_t>(&val);
        ASSERT_NE(marker, nullptr);
        EXPECT_EQ(*marker, 42);
        recorded_attrs = ExtractStringAttrs(kv);
      });

  opentelemetry::nostd::shared_ptr<MockMeter> meter =
      std::make_shared<MockMeter>();
  EXPECT_CALL(*meter,
              CreateUInt64Counter(Eq("gcp.storage.client.server.unreached"), _,
                                  Eq("{request}")))
      .WillOnce(Return(ByMove(CounterPtr(counter.release()))));

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

  ServerUnreachedMetrics metrics(
      opentelemetry::nostd::shared_ptr<opentelemetry::metrics::MeterProvider>(
          provider),
      "private.googleapis.com");

  opentelemetry::context::Context const ctx("trace_marker", std::int64_t{42});
  metrics.RecordServerUnreached(
      {"http", "ReadObject", "cloudpath", "DNS_FAILURE"}, ctx);

  std::string const expected_version =
      std::to_string(google::cloud::version_major()) + "." +
      std::to_string(google::cloud::version_minor()) + "." +
      std::to_string(google::cloud::version_patch());

  EXPECT_THAT(
      recorded_attrs,
      UnorderedElementsAre(Pair("gcp.client.version", expected_version),
                           Pair("gcp.client.artifact", "google-cloud-cpp"),
                           Pair("gcp.client.service", "storage"),
                           Pair("server.address", "private.googleapis.com"),
                           Pair("rpc.system.name", "http"),
                           Pair("gcp.storage.operation", "ReadObject"),
                           Pair("network.path", "cloudpath"),
                           Pair("error.type", "DNS_FAILURE")));
}

TEST(ServerUnreachedMetricsTest,
     NullProviderNullMeterAndNullInstrumentAreNoOps) {
  opentelemetry::context::Context const ctx;

  ServerUnreachedMetrics null_provider(nullptr, "storage.googleapis.com");
  null_provider.RecordServerUnreached(
      {"grpc", "WriteObject", "directpath", "CONNECTION_ERROR"}, ctx);

  auto null_meter_provider = std::make_shared<MockMeterProvider>();
  EXPECT_CALL(*null_meter_provider, GetMeter)
      .WillOnce(Return(
          opentelemetry::nostd::shared_ptr<opentelemetry::metrics::Meter>{}));
  ServerUnreachedMetrics null_meter(
      opentelemetry::nostd::shared_ptr<opentelemetry::metrics::MeterProvider>(
          null_meter_provider),
      "storage.googleapis.com");
  null_meter.RecordServerUnreached(
      {"grpc", "WriteObject", "directpath", "CONNECTION_ERROR"}, ctx);

  opentelemetry::nostd::shared_ptr<MockMeter> empty_meter =
      std::make_shared<MockMeter>();
  EXPECT_CALL(*empty_meter, CreateUInt64Counter)
      .WillOnce(Return(ByMove(CounterPtr{})));
  auto empty_provider = std::make_shared<MockMeterProvider>();
  EXPECT_CALL(*empty_provider, GetMeter)
      .WillOnce(Return(
          opentelemetry::nostd::shared_ptr<opentelemetry::metrics::Meter>(
              empty_meter)));
  ServerUnreachedMetrics null_instrument(
      opentelemetry::nostd::shared_ptr<opentelemetry::metrics::MeterProvider>(
          empty_provider),
      "storage.googleapis.com");
  null_instrument.RecordServerUnreached(
      {"grpc", "WriteObject", "directpath", "CONNECTION_ERROR"}, ctx);
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_internal
}  // namespace cloud
}  // namespace google
