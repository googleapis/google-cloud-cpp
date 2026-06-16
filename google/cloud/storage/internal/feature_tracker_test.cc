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

#include "google/cloud/storage/internal/feature_tracker.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace storage {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {
namespace {

using ::testing::Eq;

TEST(FeatureTrackerTest, EncodeBitmask) {
  EXPECT_THAT(EncodeFeatureTrackerBitmask(0), Eq(""));
  EXPECT_THAT(EncodeFeatureTrackerBitmask(1), Eq("AQ=="));
  EXPECT_THAT(EncodeFeatureTrackerBitmask(256), Eq("AQA="));
  EXPECT_THAT(EncodeFeatureTrackerBitmask(65536), Eq("AQAA"));
}

TEST(FeatureTrackerTest, RegisterAndRetrieve) {
  FeatureTracker tracker;
  EXPECT_THAT(tracker.GetMask(), Eq(0));
  EXPECT_THAT(tracker.HeaderValue(), Eq(""));

  tracker.RegisterFeature(TrackedFeature::kMultiStreamInMRD);
  EXPECT_THAT(tracker.GetMask(), Eq(1));
  EXPECT_THAT(tracker.HeaderValue(), Eq("AQ=="));

  tracker.RegisterFeature(TrackedFeature::kPCU);
  EXPECT_THAT(tracker.GetMask(), Eq(3));
}

TEST(FeatureTrackerTest, InitialMask) {
  FeatureTracker tracker(1U << static_cast<std::uint32_t>(
                             TrackedFeature::kGrpcDirectPathEnforced));
  EXPECT_THAT(tracker.GetMask(), Eq(4));

  tracker.RegisterFeature(TrackedFeature::kJsonReads);
  EXPECT_THAT(tracker.GetMask(), Eq(12));
}

TEST(FeatureTrackerTest, SetupFeatureTracker) {
  Options opts;
  auto configured = SetupFeatureTracker(std::move(opts));
  ASSERT_TRUE(configured.has<FeatureTrackerOption>());
  EXPECT_NE(configured.get<FeatureTrackerOption>(), nullptr);

  // Calling it again should preserve the exact same tracker instance.
  auto reconfigured = SetupFeatureTracker(configured);
  EXPECT_EQ(reconfigured.get<FeatureTrackerOption>(),
            configured.get<FeatureTrackerOption>());
}

}  // namespace
}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google
