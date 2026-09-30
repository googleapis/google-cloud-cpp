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

#include "google/cloud/storage/internal/hedged_read_metrics.h"
#include "google/cloud/testing_util/mock_opentelemetry_metrics.h"
#include "google/cloud/version.h"
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <memory>
#include <string>

namespace google {
namespace cloud {
namespace storage {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {
namespace {

using ::google::cloud::testing_util::MockCounter;
using ::google::cloud::testing_util::MockMeter;
using ::google::cloud::testing_util::MockMeterProvider;
using ::testing::_;
using ::testing::ByMove;
using ::testing::Eq;
using ::testing::Return;

using CounterPtr = opentelemetry::nostd::unique_ptr<
    opentelemetry::metrics::Counter<std::uint64_t>>;

opentelemetry::nostd::shared_ptr<opentelemetry::metrics::MeterProvider>
AsProvider(std::shared_ptr<MockMeterProvider> mock) {
  return opentelemetry::nostd::shared_ptr<
      opentelemetry::metrics::MeterProvider>{
      std::shared_ptr<opentelemetry::metrics::MeterProvider>(std::move(mock))};
}

TEST(HedgedReadMetricsTest, RecordsOnNamedCounters) {
  auto dispatched = std::make_unique<MockCounter<std::uint64_t>>();
  EXPECT_CALL(*dispatched, Add(std::uint64_t{1})).Times(2);
  auto won = std::make_unique<MockCounter<std::uint64_t>>();
  EXPECT_CALL(*won, Add(std::uint64_t{1})).Times(1);

  opentelemetry::nostd::shared_ptr<MockMeter> meter =
      std::make_shared<MockMeter>();
  EXPECT_CALL(*meter,
              CreateUInt64Counter(Eq("storage.read_hedging.hedges_dispatched"),
                                  _, Eq("{hedge}")))
      .WillOnce(Return(ByMove(CounterPtr(dispatched.release()))));
  EXPECT_CALL(*meter, CreateUInt64Counter(Eq("storage.read_hedging.hedge_won"),
                                          _, Eq("{read}")))
      .WillOnce(Return(ByMove(CounterPtr(won.release()))));

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
        EXPECT_THAT(std::string(scope.data(), scope.size()), Eq("gl-cpp"));
        EXPECT_THAT(std::string(scope_version.data(), scope_version.size()),
                    Eq(google::cloud::version_string()));
        return meter;
      });

  HedgedReadMetrics metrics(AsProvider(provider));
  metrics.OnHedgeDispatched();
  metrics.OnHedgeDispatched();
  metrics.OnHedgeWon();
}

TEST(HedgedReadMetricsTest, NoMeterRecordsNothing) {
  auto provider = std::make_shared<MockMeterProvider>();
  EXPECT_CALL(*provider, GetMeter)
      .WillOnce(Return(
          opentelemetry::nostd::shared_ptr<opentelemetry::metrics::Meter>{
              nullptr}));

  HedgedReadMetrics metrics(AsProvider(provider));
  metrics.OnHedgeDispatched();
  metrics.OnHedgeWon();
}

TEST(HedgedReadMetricsTest, NoProviderRecordsNothing) {
  HedgedReadMetrics metrics(
      opentelemetry::nostd::shared_ptr<opentelemetry::metrics::MeterProvider>{
          nullptr});
  metrics.OnHedgeDispatched();
  metrics.OnHedgeWon();
}

}  // namespace
}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google
