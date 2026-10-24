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

#include "google/cloud/storage/internal/tracked_features.h"
#include <gmock/gmock.h>
#include <cstdint>
#include <string>

namespace google {
namespace cloud {
namespace storage_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::testing::IsEmpty;

TEST(TrackedFeatures, HeaderName) {
  EXPECT_EQ(std::string(kFeaturesHeader), "x-goog-storage-cpp-features");
}

TEST(TrackedFeatures, FeatureBit) {
  EXPECT_EQ(FeatureBit(TrackedFeature::kMultistreamInMrd), std::uint32_t{1});
  EXPECT_EQ(FeatureBit(TrackedFeature::kReadHedging), std::uint32_t{2});
}

TEST(TrackedFeatures, EncodeZeroIsEmpty) {
  EXPECT_THAT(EncodeFeatureMask(0), IsEmpty());
}

// The exact strings are part of the contract with the service and with the
// other client libraries: 4-byte big-endian, URL-safe alphabet, with padding.
TEST(TrackedFeatures, EncodeKnownValues) {
  EXPECT_EQ(EncodeFeatureMask(1), "AAAAAQ==");
  EXPECT_EQ(EncodeFeatureMask(2), "AAAAAg==");
  EXPECT_EQ(EncodeFeatureMask(4), "AAAABA==");
  EXPECT_EQ(EncodeFeatureMask(0xF8), "AAAA-A==");
  EXPECT_EQ(EncodeFeatureMask(0xFFFFFFFF), "_____w==");
  EXPECT_EQ(EncodeFeatureMask(FeatureBit(TrackedFeature::kMultistreamInMrd)),
            "AAAAAQ==");
  EXPECT_EQ(EncodeFeatureMask(FeatureBit(TrackedFeature::kReadHedging)),
            "AAAAAg==");
}

TEST(TrackedFeatures, EncodeUsesUrlSafeAlphabet) {
  // Exercise every 6-bit group value; none of the outputs may contain the
  // standard alphabet's `+` or `/`.
  for (std::uint32_t mask = 1; mask != 0; mask <<= 1) {
    auto const encoded = EncodeFeatureMask(mask | (mask >> 1) | (mask >> 2));
    EXPECT_EQ(encoded.find_first_of("+/"), std::string::npos) << encoded;
    EXPECT_EQ(encoded.size(), 8U) << encoded;
  }
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_internal
}  // namespace cloud
}  // namespace google
