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
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <memory>

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
using ::testing::Return;

using CounterPtr = opentelemetry::nostd::unique_ptr<
    opentelemetry::metrics::Counter<std::uint64_t>>;

TEST(AttemptMetricsTest, RecordsAttemptsErrorsAndRetries) {
  auto attempts = std::make_unique<MockCounter<std::uint64_t>>();
  EXPECT_CALL(*attempts, Add(std::uint64_t{1}, _, _)).Times(2);
  auto errors = std::make_unique<MockCounter<std::uint64_t>>();
  EXPECT_CALL(*errors, Add(std::uint64_t{1}, _, _)).Times(1);
  auto retries = std::make_unique<MockCounter<std::uint64_t>>();
  EXPECT_CALL(*retries, Add(std::uint64_t{1}, _, _)).Times(1);

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
  metrics.RecordAttempt("ReadObject", "OK", "OK", "directpath");
  metrics.RecordAttempt("ReadObject", "UNAVAILABLE", "UNAVAILABLE",
                        "cloudpath");
  metrics.RecordRetry("ReadObject", "UNAVAILABLE", "cloudpath");
}

TEST(AttemptMetricsTest, NullProviderIsNoOp) {
  AttemptMetrics metrics(nullptr, "storage.googleapis.com", "http");
  metrics.RecordAttempt("ReadObject", "UNAVAILABLE", "503", "cloudpath");
  metrics.RecordRetry("ReadObject", "UNAVAILABLE", "cloudpath");
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_internal
}  // namespace cloud
}  // namespace google
