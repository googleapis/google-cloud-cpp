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
#include <array>
#include <cstdint>
#include <string>

namespace google {
namespace cloud {
namespace storage_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::testing::IsEmpty;
using ::testing::Not;

// Every tracked feature. Add new enumerators here so the tests below cover
// them.
constexpr std::array<TrackedFeature, 2> kAllFeatures = {
    TrackedFeature::kMultistreamInMrd,
    TrackedFeature::kReadHedging,
};

// The enumerators are bit positions in a 32-bit mask, so each must fit and no
// two may collide. Both are checked at compile time; a bad enumerator fails
// the build rather than the test.
constexpr bool AllBitPositionsFit() {
  for (auto feature : kAllFeatures) {
    if (static_cast<std::uint32_t>(feature) >= 32) return false;
  }
  return true;
}

constexpr bool AllBitPositionsDistinct() {
  for (std::size_t i = 0; i != kAllFeatures.size(); ++i) {
    for (std::size_t j = i + 1; j != kAllFeatures.size(); ++j) {
      if (kAllFeatures[i] == kAllFeatures[j]) return false;
    }
  }
  return true;
}

static_assert(AllBitPositionsFit(),
              "TrackedFeature values must be bit positions below 32");
static_assert(AllBitPositionsDistinct(),
              "TrackedFeature values must not be reused");

TEST(TrackedFeatures, HeaderName) {
  EXPECT_EQ(std::string(kFeaturesHeader), "x-goog-storage-cpp-features");
}

// The bit assignment is a contract with the service: moving a feature to a
// different bit would silently change what the service records.
TEST(TrackedFeatures, FeatureBit) {
  EXPECT_EQ(FeatureBit(TrackedFeature::kMultistreamInMrd), std::uint32_t{1});
  EXPECT_EQ(FeatureBit(TrackedFeature::kReadHedging), std::uint32_t{2});
}

TEST(TrackedFeatures, FeatureBitsAreSingleDistinctBits) {
  std::uint32_t seen = 0;
  for (auto feature : kAllFeatures) {
    auto const bit = FeatureBit(feature);
    // Exactly one bit set.
    EXPECT_NE(bit, 0U);
    EXPECT_EQ(bit & (bit - 1), 0U) << "feature bit " << bit;
    // Not shared with any other feature.
    EXPECT_EQ(seen & bit, 0U) << "feature bit " << bit;
    seen |= bit;
  }
}

// A zero mask means "no header": `EncodeFeatureMask()` returns an empty string
// so callers can distinguish it from any real mask, all of which encode to 8
// characters.
TEST(TrackedFeatures, EncodeZeroIsEmpty) {
  EXPECT_THAT(EncodeFeatureMask(0), IsEmpty());
}

// The exact strings are part of the contract with the service and with the
// other client libraries: 4-byte big-endian, URL-safe alphabet, with padding.
TEST(TrackedFeatures, EncodeKnownValues) {
  EXPECT_EQ(EncodeFeatureMask(1), "AAAAAQ==");
  EXPECT_EQ(EncodeFeatureMask(2), "AAAAAg==");
  EXPECT_EQ(EncodeFeatureMask(3), "AAAAAw==");
  EXPECT_EQ(EncodeFeatureMask(4), "AAAABA==");
  // 0xF8 and 0xFC put 62 and 63 in a sextet: the two characters that differ
  // between the standard and the URL-safe alphabets.
  EXPECT_EQ(EncodeFeatureMask(0xF8), "AAAA-A==");
  EXPECT_EQ(EncodeFeatureMask(0xFC), "AAAA_A==");
  EXPECT_EQ(EncodeFeatureMask(0xFFFFFFFF), "_____w==");
}

TEST(TrackedFeatures, EncodeTrackedFeatures) {
  EXPECT_EQ(EncodeFeatureMask(FeatureBit(TrackedFeature::kMultistreamInMrd)),
            "AAAAAQ==");
  EXPECT_EQ(EncodeFeatureMask(FeatureBit(TrackedFeature::kReadHedging)),
            "AAAAAg==");
  EXPECT_EQ(EncodeFeatureMask(FeatureBit(TrackedFeature::kMultistreamInMrd) |
                              FeatureBit(TrackedFeature::kReadHedging)),
            "AAAAAw==");
}

// The value is big-endian: the low byte of the mask is the last byte encoded.
TEST(TrackedFeatures, EncodeIsBigEndian) {
  EXPECT_EQ(EncodeFeatureMask(0x01000000), "AQAAAA==");
  EXPECT_EQ(EncodeFeatureMask(0x00000001), "AAAAAQ==");
  EXPECT_NE(EncodeFeatureMask(0x01000000), EncodeFeatureMask(0x00000001));
}

TEST(TrackedFeatures, EncodeUsesUrlSafeAlphabet) {
  // Exercise every 6-bit group value; none of the outputs may contain the
  // standard alphabet's `+` or `/`.
  for (std::uint32_t mask = 1; mask != 0; mask <<= 1) {
    auto const encoded = EncodeFeatureMask(mask | (mask >> 1) | (mask >> 2));
    EXPECT_EQ(encoded.find_first_of("+/"), std::string::npos) << encoded;
    EXPECT_EQ(encoded.size(), 8U) << encoded;
  }
  // A 32-bit value spans five full sextets, starting at bits 26, 20, 14, 8,
  // and 2. Put 62 and 63, the two sextets that differ between the alphabets,
  // in each of them.
  for (int shift : {26, 20, 14, 8, 2}) {
    for (std::uint32_t sextet : {0x3EU, 0x3FU}) {
      auto const encoded = EncodeFeatureMask(sextet << shift);
      EXPECT_THAT(encoded, Not(IsEmpty()));
      EXPECT_EQ(encoded.find_first_of("+/"), std::string::npos) << encoded;
    }
  }
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_internal
}  // namespace cloud
}  // namespace google
