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

#include "google/cloud/storage/internal/operation_metrics.h"
#include "google/cloud/testing_util/mock_opentelemetry_metrics.h"
#include "google/cloud/version.h"
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <map>
#include <memory>
#include <string>

namespace google {
namespace cloud {
namespace storage_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::google::cloud::testing_util::MockCounter;
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

using CounterPtr = opentelemetry::nostd::unique_ptr<
    opentelemetry::metrics::Counter<std::uint64_t>>;
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
        }
        return true;
      });
  return result;
}

TEST(OperationMetricsTest, RecordsOperationsAndDurationWithAttributes) {
  std::map<std::string, std::string> op_attrs;
  auto ops = std::make_unique<MockCounter<std::uint64_t>>();
  EXPECT_CALL(*ops, Add(std::uint64_t{1}, _, _))
      .WillOnce([&](std::uint64_t,
                    opentelemetry::common::KeyValueIterable const& kv,
                    opentelemetry::context::Context const&) {
        op_attrs = ExtractStringAttrs(kv);
      });

  std::map<std::string, std::string> dur_attrs;
  auto dur = std::make_unique<MockHistogram<double>>();
  EXPECT_CALL(*dur, Record(DoubleEq(0.25), _, _))
      .WillOnce([&](double, opentelemetry::common::KeyValueIterable const& kv,
                    opentelemetry::context::Context const&) {
        dur_attrs = ExtractStringAttrs(kv);
      });

  opentelemetry::nostd::shared_ptr<MockMeter> meter =
      std::make_shared<MockMeter>();
  EXPECT_CALL(*meter, CreateUInt64Counter(Eq("gcp.storage.client.operations"),
                                          _, Eq("{operation}")))
      .WillOnce(Return(ByMove(CounterPtr(ops.release()))));
  EXPECT_CALL(*meter, CreateDoubleHistogram(Eq("gcp.client.request.duration"),
                                            _, Eq("s")))
      .WillOnce(Return(ByMove(HistPtr(dur.release()))));

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
        EXPECT_EQ(std::string(scope.data(), scope.size()), "gl-cpp");
        EXPECT_EQ(std::string(scope_version.data(), scope_version.size()),
                  google::cloud::version_string());
        return meter;
      });
  opentelemetry::nostd::shared_ptr<opentelemetry::metrics::MeterProvider> mp(
      std::static_pointer_cast<opentelemetry::metrics::MeterProvider>(
          provider));

  OperationMetrics metrics(mp, "storage.googleapis.com", "grpc");
  metrics.RecordOperation("ReadObject", "Client::ReadObject",
                          "google.storage.v2.Storage/ReadObject", "OK", 0.25);

  EXPECT_THAT(op_attrs,
              UnorderedElementsAre(
                  Pair("gcp.client.version", google::cloud::version_string()),
                  Pair("gcp.client.artifact", "google-cloud-cpp"),
                  Pair("gcp.client.service", "storage"),
                  Pair("server.address", "storage.googleapis.com"),
                  Pair("rpc.system.name", "grpc"),
                  Pair("gcp.storage.operation", "ReadObject"),
                  Pair("gcp.client.method", "Client::ReadObject"),
                  Pair("error.type", "OK")));
  EXPECT_THAT(dur_attrs,
              UnorderedElementsAre(
                  Pair("gcp.client.version", google::cloud::version_string()),
                  Pair("gcp.client.artifact", "google-cloud-cpp"),
                  Pair("gcp.client.service", "storage"),
                  Pair("server.address", "storage.googleapis.com"),
                  Pair("rpc.system.name", "grpc"),
                  Pair("gcp.storage.operation", "ReadObject"),
                  Pair("gcp.client.method", "Client::ReadObject"),
                  Pair("error.type", "OK"),
                  Pair("rpc.method", "google.storage.v2.Storage/ReadObject")));
}

TEST(OperationMetricsTest, NullProviderAndNullMeterAreNoOps) {
  OperationMetrics no_provider(nullptr, "storage.googleapis.com", "grpc");
  no_provider.RecordOperation("ReadObject", "Client::ReadObject",
                              "google.storage.v2.Storage/ReadObject", "OK",
                              0.1);

  auto provider = std::make_shared<MockMeterProvider>();
  EXPECT_CALL(*provider, GetMeter)
      .WillOnce(Return(
          opentelemetry::nostd::shared_ptr<opentelemetry::metrics::Meter>{
              nullptr}));
  opentelemetry::nostd::shared_ptr<opentelemetry::metrics::MeterProvider> mp(
      std::static_pointer_cast<opentelemetry::metrics::MeterProvider>(
          provider));
  OperationMetrics no_meter(mp, "storage.googleapis.com", "grpc");
  no_meter.RecordOperation("ReadObject", "Client::ReadObject",
                           "google.storage.v2.Storage/ReadObject", "OK", 0.1);
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_internal
}  // namespace cloud
}  // namespace google
