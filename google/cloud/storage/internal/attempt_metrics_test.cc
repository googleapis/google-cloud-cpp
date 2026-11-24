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
#include <opentelemetry/common/attribute_value.h>
#include <opentelemetry/common/key_value_iterable.h>
#include <opentelemetry/context/context.h>
#include <opentelemetry/nostd/shared_ptr.h>
#include <opentelemetry/nostd/string_view.h>
#include <opentelemetry/nostd/variant.h>
#include <opentelemetry/version.h>
#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <utility>
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
using ::testing::IsEmpty;
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
        } else {
          ADD_FAILURE() << "Unexpected attribute type for key: " << k;
        }
        return true;
      });
  return out;
}

void ExpectTraceMarker(opentelemetry::context::Context const& ctx) {
  opentelemetry::context::ContextValue const val = ctx.GetValue("trace_marker");
  auto const* marker = opentelemetry::nostd::get_if<std::int64_t>(&val);
  ASSERT_NE(marker, nullptr);
  EXPECT_EQ(*marker, 42);
}

TEST(AttemptMetricsTest, RecordsGrpcAndHttpAttributes) {
  std::vector<CapturedAttrs> attempt_attrs;
  auto attempts = std::make_unique<MockCounter<std::uint64_t>>();
  EXPECT_CALL(*attempts, Add(std::uint64_t{1}, _, _))
      .Times(4)
      .WillRepeatedly([&](std::uint64_t,
                          opentelemetry::common::KeyValueIterable const& kv,
                          opentelemetry::context::Context const& ctx) {
        ExpectTraceMarker(ctx);
        attempt_attrs.push_back(ExtractAttrs(kv));
      });

  std::vector<CapturedAttrs> error_attrs;
  auto errors = std::make_unique<MockCounter<std::uint64_t>>();
  EXPECT_CALL(*errors, Add(std::uint64_t{1}, _, _))
      .Times(2)
      .WillRepeatedly([&](std::uint64_t,
                          opentelemetry::common::KeyValueIterable const& kv,
                          opentelemetry::context::Context const& ctx) {
        ExpectTraceMarker(ctx);
        error_attrs.push_back(ExtractAttrs(kv));
      });

  CapturedAttrs retry_attrs;
  auto retries = std::make_unique<MockCounter<std::uint64_t>>();
  EXPECT_CALL(*retries, Add(std::uint64_t{1}, _, _))
      .WillOnce([&](std::uint64_t,
                    opentelemetry::common::KeyValueIterable const& kv,
                    opentelemetry::context::Context const& ctx) {
        ExpectTraceMarker(ctx);
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

  AttemptMetrics metrics(mp, "storage.googleapis.com");
  opentelemetry::context::Context const ctx =
      opentelemetry::context::Context("trace_marker", std::int64_t{42});
  metrics.RecordGrpcAttempt({"ReadObject", "OK", "OK", "directpath"}, ctx);
  metrics.RecordGrpcAttempt(
      {"WriteObject", "DEADLINE_EXCEEDED", "DEADLINE_EXCEEDED", "directpath"},
      ctx);
  metrics.RecordHttpAttempt({"GetObjectMetadata", "OK", 200, "cloudpath"}, ctx);
  metrics.RecordHttpAttempt({"DeleteObject", "UNAVAILABLE", 503, "cloudpath"},
                            ctx);
  metrics.RecordRetry({"http", "DeleteObject", "UNAVAILABLE", "cloudpath"},
                      ctx);

  std::string const ver = std::to_string(google::cloud::version_major()) + "." +
                          std::to_string(google::cloud::version_minor()) + "." +
                          std::to_string(google::cloud::version_patch());
  ASSERT_EQ(attempt_attrs.size(), 4U);
  EXPECT_THAT(attempt_attrs[0].strings,
              UnorderedElementsAre(
                  Pair("gcp.client.version", ver),
                  Pair("gcp.client.artifact", "google-cloud-cpp"),
                  Pair("gcp.client.service", "storage"),
                  Pair("server.address", "storage.googleapis.com"),
                  Pair("rpc.system.name", "grpc"),
                  Pair("gcp.storage.operation", "ReadObject"),
                  Pair("error.type", "OK"), Pair("network.path", "directpath"),
                  Pair("rpc.response.status_code", "OK")));
  EXPECT_THAT(attempt_attrs[0].ints, IsEmpty());
  EXPECT_THAT(attempt_attrs[1].strings,
              UnorderedElementsAre(
                  Pair("gcp.client.version", ver),
                  Pair("gcp.client.artifact", "google-cloud-cpp"),
                  Pair("gcp.client.service", "storage"),
                  Pair("server.address", "storage.googleapis.com"),
                  Pair("rpc.system.name", "grpc"),
                  Pair("gcp.storage.operation", "WriteObject"),
                  Pair("error.type", "DEADLINE_EXCEEDED"),
                  Pair("network.path", "directpath"),
                  Pair("rpc.response.status_code", "DEADLINE_EXCEEDED")));
  EXPECT_THAT(attempt_attrs[1].ints, IsEmpty());
  EXPECT_THAT(attempt_attrs[2].strings,
              UnorderedElementsAre(
                  Pair("gcp.client.version", ver),
                  Pair("gcp.client.artifact", "google-cloud-cpp"),
                  Pair("gcp.client.service", "storage"),
                  Pair("server.address", "storage.googleapis.com"),
                  Pair("rpc.system.name", "http"),
                  Pair("gcp.storage.operation", "GetObjectMetadata"),
                  Pair("error.type", "OK"), Pair("network.path", "cloudpath")));
  EXPECT_THAT(attempt_attrs[2].ints,
              UnorderedElementsAre(Pair("http.response.status_code", 200)));
  EXPECT_THAT(
      attempt_attrs[3].strings,
      UnorderedElementsAre(Pair("gcp.client.version", ver),
                           Pair("gcp.client.artifact", "google-cloud-cpp"),
                           Pair("gcp.client.service", "storage"),
                           Pair("server.address", "storage.googleapis.com"),
                           Pair("rpc.system.name", "http"),
                           Pair("gcp.storage.operation", "DeleteObject"),
                           Pair("error.type", "UNAVAILABLE"),
                           Pair("network.path", "cloudpath")));
  EXPECT_THAT(attempt_attrs[3].ints,
              UnorderedElementsAre(Pair("http.response.status_code", 503)));

  ASSERT_EQ(error_attrs.size(), 2U);
  EXPECT_THAT(
      error_attrs[0].strings,
      UnorderedElementsAre(Pair("gcp.client.version", ver),
                           Pair("gcp.client.artifact", "google-cloud-cpp"),
                           Pair("gcp.client.service", "storage"),
                           Pair("server.address", "storage.googleapis.com"),
                           Pair("rpc.system.name", "grpc"),
                           Pair("gcp.storage.operation", "WriteObject"),
                           Pair("error.type", "DEADLINE_EXCEEDED"),
                           Pair("network.path", "directpath")));
  EXPECT_THAT(error_attrs[0].ints, IsEmpty());
  EXPECT_THAT(
      error_attrs[1].strings,
      UnorderedElementsAre(Pair("gcp.client.version", ver),
                           Pair("gcp.client.artifact", "google-cloud-cpp"),
                           Pair("gcp.client.service", "storage"),
                           Pair("server.address", "storage.googleapis.com"),
                           Pair("rpc.system.name", "http"),
                           Pair("gcp.storage.operation", "DeleteObject"),
                           Pair("error.type", "UNAVAILABLE"),
                           Pair("network.path", "cloudpath")));
  EXPECT_THAT(error_attrs[1].ints, IsEmpty());
  EXPECT_THAT(
      retry_attrs.strings,
      UnorderedElementsAre(Pair("gcp.client.version", ver),
                           Pair("gcp.client.artifact", "google-cloud-cpp"),
                           Pair("gcp.client.service", "storage"),
                           Pair("server.address", "storage.googleapis.com"),
                           Pair("rpc.system.name", "http"),
                           Pair("gcp.storage.operation", "DeleteObject"),
                           Pair("error.type", "UNAVAILABLE"),
                           Pair("network.path", "cloudpath")));
  EXPECT_THAT(retry_attrs.ints, IsEmpty());
}

TEST(AttemptMetricsTest, NullProviderAndNullMeterAreNoOps) {
  opentelemetry::context::Context const ctx{};
  AttemptMetrics no_provider(nullptr, "storage.googleapis.com");
  no_provider.RecordGrpcAttempt({"ReadObject", "OK", "OK", "directpath"}, ctx);
  no_provider.RecordHttpAttempt({"ReadObject", "UNAVAILABLE", 503, "cloudpath"},
                                ctx);
  no_provider.RecordRetry({"http", "ReadObject", "UNAVAILABLE", "cloudpath"},
                          ctx);

  auto provider = std::make_shared<MockMeterProvider>();
  EXPECT_CALL(*provider, GetMeter)
      .WillOnce(Return(
          opentelemetry::nostd::shared_ptr<opentelemetry::metrics::Meter>{
              nullptr}));
  opentelemetry::nostd::shared_ptr<opentelemetry::metrics::MeterProvider> mp(
      std::static_pointer_cast<opentelemetry::metrics::MeterProvider>(
          provider));
  AttemptMetrics no_meter(mp, "storage.googleapis.com");
  no_meter.RecordGrpcAttempt(
      {"ReadObject", "UNAVAILABLE", "UNAVAILABLE", "cloudpath"}, ctx);
  no_meter.RecordHttpAttempt({"ReadObject", "UNAVAILABLE", 503, "cloudpath"},
                             ctx);
  no_meter.RecordRetry({"grpc", "ReadObject", "UNAVAILABLE", "cloudpath"}, ctx);
}

TEST(AttemptMetricsTest, NullAndPartialInstruments) {
  opentelemetry::context::Context const ctx{};
  opentelemetry::nostd::shared_ptr<MockMeter> empty_meter =
      std::make_shared<MockMeter>();
  EXPECT_CALL(*empty_meter, CreateUInt64Counter)
      .WillOnce(Return(ByMove(CounterPtr(nullptr))))
      .WillOnce(Return(ByMove(CounterPtr(nullptr))))
      .WillOnce(Return(ByMove(CounterPtr(nullptr))));

  auto errors = std::make_unique<MockCounter<std::uint64_t>>();
  EXPECT_CALL(*errors, Add(std::uint64_t{1}, _, _)).Times(1);
  auto retries = std::make_unique<MockCounter<std::uint64_t>>();
  EXPECT_CALL(*retries, Add(std::uint64_t{1}, _, _)).Times(1);
  opentelemetry::nostd::shared_ptr<MockMeter> no_attempts_meter =
      std::make_shared<MockMeter>();
  EXPECT_CALL(*no_attempts_meter, CreateUInt64Counter)
      .WillOnce(Return(ByMove(CounterPtr(nullptr))))
      .WillOnce(Return(ByMove(CounterPtr(errors.release()))))
      .WillOnce(Return(ByMove(CounterPtr(retries.release()))));

  auto attempts = std::make_unique<MockCounter<std::uint64_t>>();
  EXPECT_CALL(*attempts, Add(std::uint64_t{1}, _, _)).Times(1);
  opentelemetry::nostd::shared_ptr<MockMeter> no_errors_meter =
      std::make_shared<MockMeter>();
  EXPECT_CALL(*no_errors_meter, CreateUInt64Counter)
      .WillOnce(Return(ByMove(CounterPtr(attempts.release()))))
      .WillOnce(Return(ByMove(CounterPtr(nullptr))))
      .WillOnce(Return(ByMove(CounterPtr(nullptr))));

  auto provider = std::make_shared<MockMeterProvider>();
  EXPECT_CALL(*provider, GetMeter)
      .WillOnce(Return(empty_meter))
      .WillOnce(Return(no_attempts_meter))
      .WillOnce(Return(no_errors_meter));
  opentelemetry::nostd::shared_ptr<opentelemetry::metrics::MeterProvider> mp(
      std::static_pointer_cast<opentelemetry::metrics::MeterProvider>(
          provider));

  AttemptMetrics none(mp, "storage.googleapis.com");
  none.RecordGrpcAttempt(
      {"ReadObject", "UNAVAILABLE", "UNAVAILABLE", "cloudpath"}, ctx);
  none.RecordRetry({"grpc", "ReadObject", "UNAVAILABLE", "cloudpath"}, ctx);

  AttemptMetrics no_attempts(mp, "storage.googleapis.com");
  no_attempts.RecordGrpcAttempt({"ReadObject", "OK", "OK", "directpath"}, ctx);
  no_attempts.RecordGrpcAttempt(
      {"ReadObject", "UNAVAILABLE", "UNAVAILABLE", "cloudpath"}, ctx);
  no_attempts.RecordRetry({"grpc", "ReadObject", "UNAVAILABLE", "cloudpath"},
                          ctx);

  AttemptMetrics no_errors(mp, "storage.googleapis.com");
  no_errors.RecordGrpcAttempt(
      {"ReadObject", "UNAVAILABLE", "UNAVAILABLE", "cloudpath"}, ctx);
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_internal
}  // namespace cloud
}  // namespace google
