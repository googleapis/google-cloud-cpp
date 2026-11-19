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
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <memory>

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
using ::testing::Return;

using CounterPtr = opentelemetry::nostd::unique_ptr<
    opentelemetry::metrics::Counter<std::uint64_t>>;
using HistPtr =
    opentelemetry::nostd::unique_ptr<opentelemetry::metrics::Histogram<double>>;

TEST(OperationMetricsTest, RecordsOperationsAndDuration) {
  auto ops = std::make_unique<MockCounter<std::uint64_t>>();
  EXPECT_CALL(*ops, Add(std::uint64_t{1}, _, _)).Times(1);
  auto dur = std::make_unique<MockHistogram<double>>();
  EXPECT_CALL(*dur, Record(DoubleEq(0.25), _, _)).Times(1);

  opentelemetry::nostd::shared_ptr<MockMeter> meter =
      std::make_shared<MockMeter>();
  EXPECT_CALL(*meter, CreateUInt64Counter(Eq("gcp.storage.client.operations"),
                                          _, Eq("{operation}")))
      .WillOnce(Return(ByMove(CounterPtr(ops.release()))));
  EXPECT_CALL(*meter, CreateDoubleHistogram(Eq("gcp.client.request.duration"),
                                            _, Eq("s")))
      .WillOnce(Return(ByMove(HistPtr(dur.release()))));

  auto provider = std::make_shared<MockMeterProvider>();
  EXPECT_CALL(*provider, GetMeter).WillOnce(Return(meter));
  opentelemetry::nostd::shared_ptr<opentelemetry::metrics::MeterProvider> mp(
      std::static_pointer_cast<opentelemetry::metrics::MeterProvider>(
          provider));

  OperationMetrics metrics(mp, "storage.googleapis.com", "grpc");
  metrics.RecordOperation("ReadObject", "Client::ReadObject",
                          "google.storage.v2.Storage/ReadObject", "OK", 0.25);
}

TEST(OperationMetricsTest, NullProviderIsNoOp) {
  OperationMetrics metrics(nullptr, "storage.googleapis.com", "grpc");
  metrics.RecordOperation("ReadObject", "Client::ReadObject",
                          "google.storage.v2.Storage/ReadObject", "OK", 0.1);
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_internal
}  // namespace cloud
}  // namespace google
