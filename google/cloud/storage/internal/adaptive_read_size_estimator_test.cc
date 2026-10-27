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

#include "google/cloud/storage/internal/adaptive_read_size_estimator.h"
#include "google/cloud/storage/options.h"
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <cstdint>
#include <string>
#include <thread>
#include <vector>

namespace google {
namespace cloud {
namespace storage {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {
namespace {

using ::testing::DoubleEq;
using ::testing::Eq;
using ::testing::NotNull;

std::int64_t constexpr kKiB = 1024;
std::int64_t constexpr kMiB = 1024 * kKiB;

AdaptiveReadSettings DefaultSettings() {
  return MakeAdaptiveReadSettings(
      storage_experimental::AdaptiveReadRangeConfig{});
}

void RecordMany(AdaptiveReadSizeEstimator& estimator, std::string const& bucket,
                std::int64_t bytes, bool reached_eof, std::size_t count) {
  for (std::size_t i = 0; i != count; ++i) {
    estimator.Record(bucket, bytes, reached_eof);
  }
}

TEST(AdaptiveReadSizeEstimator, SimulatedCostEarlyClose) {
  auto const settings = DefaultSettings();
  // 400 KiB from a 512 KiB window: one request, 112 KiB drained.
  EXPECT_THAT(AdaptiveReadSizeEstimator::SimulatedCost(400 * kKiB, false,
                                                       512 * kKiB, settings),
              DoubleEq(112.0 * kKiB));
  // From a 256 KiB window: two requests (256 KiB + 512 KiB), 368 KiB drained.
  EXPECT_THAT(AdaptiveReadSizeEstimator::SimulatedCost(400 * kKiB, false,
                                                       256 * kKiB, settings),
              DoubleEq(368.0 * kKiB + kMiB));
  // From a 4 MiB window: the remainder is abandoned.
  EXPECT_THAT(AdaptiveReadSizeEstimator::SimulatedCost(400 * kKiB, false,
                                                       4 * kMiB, settings),
              DoubleEq(2.0 * kMiB));
}

TEST(AdaptiveReadSizeEstimator, SimulatedCostEndOfData) {
  auto const settings = DefaultSettings();
  // The last window is clamped to the end of the data, only the requests
  // count.
  EXPECT_THAT(AdaptiveReadSizeEstimator::SimulatedCost(400 * kKiB, true,
                                                       4 * kMiB, settings),
              DoubleEq(0.0));
  // 3 MiB from 1 MiB: [0, 1), [1, 3) MiB.
  EXPECT_THAT(
      AdaptiveReadSizeEstimator::SimulatedCost(3 * kMiB, true, kMiB, settings),
      DoubleEq(1.0 * kMiB));
  // 1 GiB from 1 MiB: 1, 2, 4, ..., 32 (63 MiB), then 16 windows of 64 MiB, 22
  // requests in total.
  EXPECT_THAT(AdaptiveReadSizeEstimator::SimulatedCost(1024 * kMiB, true, kMiB,
                                                       settings),
              DoubleEq(21.0 * kMiB));
}

TEST(AdaptiveReadSizeEstimator, SimulatedCostWithoutGrowth) {
  auto settings = DefaultSettings();
  settings.growth_factor = 1.0;
  // 10 windows of 1 MiB, the last one has 512 KiB left.
  EXPECT_THAT(AdaptiveReadSizeEstimator::SimulatedCost(9 * kMiB + 512 * kKiB,
                                                       false, kMiB, settings),
              DoubleEq(512.0 * kKiB + 9.0 * kMiB));
}

TEST(AdaptiveReadSizeEstimator, EmptyUsesInitialWindow) {
  AdaptiveReadSizeEstimator estimator;
  auto settings = DefaultSettings();
  EXPECT_THAT(estimator.Recommend("bucket", settings), Eq(kMiB));
  settings.initial_window = 300 * kKiB;
  EXPECT_THAT(estimator.Recommend("bucket", settings), Eq(300 * kKiB));
  EXPECT_THAT(estimator.bucket_count(), Eq(0U));
}

TEST(AdaptiveReadSizeEstimator, FewSamplesUseInitialWindow) {
  AdaptiveReadSizeEstimator estimator;
  auto const settings = DefaultSettings();
  RecordMany(estimator, "bucket", 400 * kKiB, false,
             AdaptiveReadSizeEstimator::kMinSamples - 1);
  EXPECT_THAT(estimator.Recommend("bucket", settings), Eq(kMiB));
  estimator.Record("bucket", 400 * kKiB, false);
  EXPECT_THAT(estimator.Recommend("bucket", settings), Eq(512 * kKiB));
}

TEST(AdaptiveReadSizeEstimator, SmallReads) {
  AdaptiveReadSizeEstimator estimator;
  RecordMany(estimator, "bucket", 400 * kKiB, false, 100);
  EXPECT_THAT(estimator.Recommend("bucket", DefaultSettings()), Eq(512 * kKiB));
  // Other buckets are not affected.
  EXPECT_THAT(estimator.Recommend("other", DefaultSettings()), Eq(kMiB));
}

TEST(AdaptiveReadSizeEstimator, BimodalReadsDependOnRequestCost) {
  // Five reads of 400 KiB for each read of 2 MiB.
  auto record = [](AdaptiveReadSizeEstimator& estimator) {
    for (int i = 0; i != 16; ++i) {
      RecordMany(estimator, "bucket", 400 * kKiB, false, 5);
      estimator.Record("bucket", 2 * kMiB, false);
    }
  };
  // With expensive requests a 768 KiB window serves the 2 MiB reads in two
  // requests (768 KiB + 1.5 MiB) without abandoning a window, at the cost of
  // draining more data from the small reads.
  AdaptiveReadSizeEstimator expensive;
  record(expensive);
  auto settings = DefaultSettings();
  settings.request_cost_bytes = kMiB;
  EXPECT_THAT(expensive.Recommend("bucket", settings), Eq(768 * kKiB));

  // With cheap requests a 512 KiB window, which fits the small reads, wins.
  AdaptiveReadSizeEstimator cheap;
  record(cheap);
  settings.request_cost_bytes = 64 * kKiB;
  EXPECT_THAT(cheap.Recommend("bucket", settings), Eq(512 * kKiB));
}

TEST(AdaptiveReadSizeEstimator, RecommendationFollowsSettings) {
  AdaptiveReadSizeEstimator estimator;
  for (int i = 0; i != 16; ++i) {
    RecordMany(estimator, "bucket", 400 * kKiB, false, 5);
    estimator.Record("bucket", 2 * kMiB, false);
  }
  auto settings = DefaultSettings();
  settings.request_cost_bytes = kMiB;
  EXPECT_THAT(estimator.Recommend("bucket", settings), Eq(768 * kKiB));
  // A change in the settings recomputes the recommendation.
  settings.request_cost_bytes = 64 * kKiB;
  EXPECT_THAT(estimator.Recommend("bucket", settings), Eq(512 * kKiB));
}

TEST(AdaptiveReadSizeEstimator, HalfStepCandidates) {
  // Reads of 1.2 MiB fit in a 1.5 MiB half-step window, avoiding the extra
  // 512 KiB of drain that a 2 MiB power-of-two window would incur.
  AdaptiveReadSizeEstimator estimator;
  RecordMany(estimator, "bucket", 1200 * kKiB, false, 64);
  EXPECT_THAT(estimator.Recommend("bucket", DefaultSettings()),
              Eq(1536 * kKiB));
}

TEST(AdaptiveReadSizeEstimator, SeparatesStartAndOffsetHistories) {
  // Small header reads from the start of the object and larger chunk reads from
  // a positive offset do not skew each other's window recommendation.
  AdaptiveReadSizeEstimator estimator;
  for (std::size_t i = 0; i != AdaptiveReadSizeEstimator::kMinSamples; ++i) {
    estimator.Record("bucket", 64 * kKiB, /*reached_eof=*/false,
                     /*from_offset=*/false);
    estimator.Record("bucket", 1800 * kKiB, /*reached_eof=*/false,
                     /*from_offset=*/true);
  }
  auto const settings = DefaultSettings();
  EXPECT_THAT(estimator.Recommend("bucket", settings, /*from_offset=*/false),
              Eq(AdaptiveReadSizeEstimator::kMinCandidate));
  EXPECT_THAT(estimator.Recommend("bucket", settings, /*from_offset=*/true),
              Eq(2 * kMiB));
  EXPECT_THAT(estimator.bucket_count(), Eq(1U));
}

TEST(AdaptiveReadSizeEstimator, TailReadsToEndOfData) {
  // Reads of the last 64 KiB of an object (e.g. a file footer) need a single
  // small window.
  AdaptiveReadSizeEstimator estimator;
  RecordMany(estimator, "bucket", 64 * kKiB, true, 64);
  EXPECT_THAT(estimator.Recommend("bucket", DefaultSettings()),
              Eq(AdaptiveReadSizeEstimator::kMinCandidate));
}

TEST(AdaptiveReadSizeEstimator, FullReadsToEndOfData) {
  // Full reads of 3.5 MiB objects fit a single 4 MiB window, and the window is
  // clamped to the end of the object.
  AdaptiveReadSizeEstimator estimator;
  RecordMany(estimator, "bucket", 3500 * kKiB, true, 64);
  EXPECT_THAT(estimator.Recommend("bucket", DefaultSettings()), Eq(4 * kMiB));
}

TEST(AdaptiveReadSizeEstimator, KeepsRecentHistory) {
  AdaptiveReadSizeEstimator estimator;
  auto const settings = DefaultSettings();
  RecordMany(estimator, "bucket", 3500 * kKiB, true,
             AdaptiveReadSizeEstimator::kHistorySize);
  EXPECT_THAT(estimator.Recommend("bucket", settings), Eq(4 * kMiB));
  // The new reads replace the old ones.
  RecordMany(estimator, "bucket", 400 * kKiB, false,
             AdaptiveReadSizeEstimator::kHistorySize);
  EXPECT_THAT(estimator.Recommend("bucket", settings), Eq(512 * kKiB));
}

TEST(AdaptiveReadSizeEstimator, SmallMaxWindowUsesInitialWindow) {
  AdaptiveReadSizeEstimator estimator;
  RecordMany(estimator, "bucket", 400 * kKiB, false, 64);
  auto settings = DefaultSettings();
  settings.initial_window = 64 * kKiB;
  settings.max_window = 128 * kKiB;
  EXPECT_THAT(estimator.Recommend("bucket", settings), Eq(64 * kKiB));
}

TEST(AdaptiveReadSizeEstimator, RecommendationIsBoundedByMaxWindow) {
  AdaptiveReadSizeEstimator estimator;
  RecordMany(estimator, "bucket", 100 * kMiB, true, 64);
  auto settings = DefaultSettings();
  settings.max_window = 2 * kMiB;
  EXPECT_THAT(estimator.Recommend("bucket", settings), Eq(2 * kMiB));
}

TEST(AdaptiveReadSizeEstimator, EvictsLeastRecentlyUsedBucket) {
  AdaptiveReadSizeEstimator estimator(/*max_buckets=*/2);
  auto const settings = DefaultSettings();
  RecordMany(estimator, "b1", 400 * kKiB, false, 64);
  RecordMany(estimator, "b2", 400 * kKiB, false, 64);
  // Using `b1` makes `b2` the least recently used bucket.
  EXPECT_THAT(estimator.Recommend("b1", settings), Eq(512 * kKiB));
  RecordMany(estimator, "b3", 400 * kKiB, false, 64);
  EXPECT_THAT(estimator.bucket_count(), Eq(2U));
  EXPECT_THAT(estimator.Recommend("b1", settings), Eq(512 * kKiB));
  EXPECT_THAT(estimator.Recommend("b2", settings), Eq(kMiB));
  EXPECT_THAT(estimator.Recommend("b3", settings), Eq(512 * kKiB));
}

TEST(AdaptiveReadSizeEstimator, NegativeBytesAreZero) {
  AdaptiveReadSizeEstimator estimator;
  RecordMany(estimator, "bucket", -1, true, 64);
  EXPECT_THAT(estimator.Recommend("bucket", DefaultSettings()),
              Eq(AdaptiveReadSizeEstimator::kMinCandidate));
}

TEST(AdaptiveReadSizeEstimator, Global) {
  auto global = AdaptiveReadSizeEstimator::Global();
  ASSERT_THAT(global, NotNull());
  EXPECT_THAT(AdaptiveReadSizeEstimator::Global().get(), Eq(global.get()));
}

TEST(AdaptiveReadSizeEstimator, ConcurrentRecordAndRecommend) {
  AdaptiveReadSizeEstimator estimator(/*max_buckets=*/4);
  auto const settings = DefaultSettings();
  auto work = [&](int id) {
    for (int i = 0; i != 2000; ++i) {
      auto const bucket = "bucket-" + std::to_string((id + i) % 6);
      estimator.Record(bucket, (i % 2 == 0 ? 400 : 3000) * kKiB, i % 3 == 0);
      auto const w = estimator.Recommend(bucket, settings);
      EXPECT_GE(w, AdaptiveReadSizeEstimator::kMinCandidate);
      EXPECT_LE(w, settings.max_window);
    }
  };
  std::vector<std::thread> threads;
  for (int id = 0; id != 8; ++id) threads.emplace_back(work, id);
  for (auto& t : threads) t.join();
  EXPECT_THAT(estimator.bucket_count(), Eq(4U));
}

}  // namespace
}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google
