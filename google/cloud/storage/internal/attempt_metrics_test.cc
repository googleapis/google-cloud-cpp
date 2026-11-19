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

#include "google/cloud/storage/internal/attempt_metrics.h"
#include "google/cloud/testing_util/mock_opentelemetry_metrics.h"
#include "google/cloud/version.h"
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>

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

struct CapturedAttrs {
  std::map<std::string, std::string> strings;
  std::map<std::string, std::int64_t> ints;
};

CapturedAttrs ExtractAttrs(
    opentelemetry::common::KeyValueIterable const& iterable) {
  CapturedAttrs out;
  iterable.ForEachKeyValue(
      [&](opentelemetry::nostd::string_view key,
          opentelemetry::common::AttributeValue value) noexcept {
        std::string k(key.data(), key.size());
        if (auto const* sv =
                opentelemetry::nostd::get_if<opentelemetry::nostd::string_view>(
                    &value)) {
          out.strings.emplace(std::move(k),
                              std::string(sv->data(), sv->size()));
        } else if (auto const* i =
                       opentelemetry::nostd::get_if<std::int64_t>(&value)) {
          out.ints.emplace(std::move(k), *i);
        }
        return true;
      });
  return out;
}

TEST(AttemptMetricsTest, RecordsGrpcAndHttpAttributes) {
  std::vector<CapturedAttrs> attempt_attrs;
  auto attempts = std::make_unique<MockCounter<std::uint64_t>>();
  EXPECT_CALL(*attempts, Add(std::uint64_t{1}, _, _))
      .Times(3)
      .WillRepeatedly([&](std::uint64_t,
                          opentelemetry::common::KeyValueIterable const& kv,
                          opentelemetry::context::Context const&) {
        attempt_attrs.push_back(ExtractAttrs(kv));
      });

  std::vector<CapturedAttrs> error_attrs;
  auto errors = std::make_unique<MockCounter<std::uint64_t>>();
  EXPECT_CALL(*errors, Add(std::uint64_t{1}, _, _))
      .Times(2)
      .WillRepeatedly([&](std::uint64_t,
                          opentelemetry::common::KeyValueIterable const& kv,
                          opentelemetry::context::Context const&) {
        error_attrs.push_back(ExtractAttrs(kv));
      });

  CapturedAttrs retry_attrs;
  auto retries = std::make_unique<MockCounter<std::uint64_t>>();
  EXPECT_CALL(*retries, Add(std::uint64_t{1}, _, _))
      .WillOnce([&](std::uint64_t,
                    opentelemetry::common::KeyValueIterable const& kv,
                    opentelemetry::context::Context const&) {
        retry_attrs = ExtractAttrs(kv);
      });

  opentelemetry::nostd::shared_ptr<MockMeter> meter =
      std::make_shared<MockMeter>();
  EXPECT_CALL(*meter, CreateUInt64Counter(Eq("gcp.storage.client.attempts"), _,
                                          Eq("{attempt}")))
      .WillOnce(Return(ByMove(CounterPtr(attempts.release()))));
  EXPECT_CALL(*meter, CreateUInt64Counter(Eq("gcp.storage.client.errors"), _,
                                          Eq("{error}")))
      .WillOnce(Return(ByMove(CounterPtr(errors.release()))));
  EXPECT_CALL(*meter, CreateUInt64Counter(Eq("gcp.storage.client.retries"), _,
                                          Eq("{retry}")))
      .WillOnce(Return(ByMove(CounterPtr(retries.release()))));

  auto provider = std::make_shared<MockMeterProvider>();
  EXPECT_CALL(*provider, GetMeter).WillOnce(Return(meter));
  opentelemetry::nostd::shared_ptr<opentelemetry::metrics::MeterProvider> mp(
      std::static_pointer_cast<opentelemetry::metrics::MeterProvider>(
          provider));

  AttemptMetrics metrics(mp, "storage.googleapis.com", "grpc");
  metrics.RecordGrpcAttempt("ReadObject", "OK", "OK", "directpath");
  metrics.RecordGrpcAttempt("ReadObject", "UNAVAILABLE", "UNAVAILABLE",
                            "cloudpath");
  metrics.RecordHttpAttempt("ReadObject", "UNAVAILABLE", 503, "cloudpath");
  metrics.RecordRetry("ReadObject", "UNAVAILABLE", "cloudpath");

  ASSERT_EQ(attempt_attrs.size(), 3U);
  EXPECT_EQ(attempt_attrs[0].strings["rpc.response.status_code"], "OK");
  EXPECT_EQ(attempt_attrs[1].strings["rpc.response.status_code"],
            "UNAVAILABLE");
  EXPECT_THAT(attempt_attrs[2].ints,
              UnorderedElementsAre(Pair("http.response.status_code", 503)));
  ASSERT_EQ(error_attrs.size(), 2U);
  EXPECT_EQ(error_attrs[0].strings.count("rpc.response.status_code"), 0U);
  EXPECT_EQ(error_attrs[1].ints.count("http.response.status_code"), 0U);
  EXPECT_THAT(retry_attrs.strings,
              UnorderedElementsAre(
                  Pair("gcp.client.version", google::cloud::version_string()),
                  Pair("gcp.client.artifact", "google-cloud-cpp"),
                  Pair("gcp.client.service", "storage"),
                  Pair("server.address", "storage.googleapis.com"),
                  Pair("rpc.system.name", "grpc"),
                  Pair("gcp.storage.operation", "ReadObject"),
                  Pair("error.type", "UNAVAILABLE"),
                  Pair("network.path", "cloudpath")));
}

TEST(AttemptMetricsTest, NullProviderIsNoOp) {
  AttemptMetrics metrics(nullptr, "storage.googleapis.com", "http");
  metrics.RecordHttpAttempt("ReadObject", "UNAVAILABLE", 503, "cloudpath");
  metrics.RecordRetry("ReadObject", "UNAVAILABLE", "cloudpath");
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_internal
}  // namespace cloud
}  // namespace google
