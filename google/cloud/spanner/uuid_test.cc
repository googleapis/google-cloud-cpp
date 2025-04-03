// Copyright 2025 Google LLC
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

#include "google/cloud/spanner/uuid.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gtest/gtest.h>
#include <sstream>

namespace google {
namespace cloud {
namespace spanner {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::google::cloud::testing_util::IsOkAndHolds;
using ::google::cloud::testing_util::StatusIs;
using ::testing::Eq;
using ::testing::Pair;
using ::testing::StartsWith;

TEST(UuidTest, DefaultConstruction) {
  Uuid uuid1;
  EXPECT_THAT(static_cast<absl::uint128>(uuid1), Eq(0));
  Uuid uuid2{};
  EXPECT_THAT(static_cast<absl::uint128>(uuid2), Eq(0));
}

TEST(UuidTest, ConstructedFromUint128) {
  absl::uint128 expected_value =
      absl::MakeUint128(0x0b6ed04ca16dfc46, 0x52817f9978c13738);
  Uuid uuid{expected_value};
  EXPECT_THAT(static_cast<absl::uint128>(uuid), Eq(expected_value));
}

TEST(UuidTest, ConstructedFromUint64Pieces) {
  std::uint64_t high = 0x7bf8a7b819171919;
  std::uint64_t low = 0x2625f208c5824254;
  Uuid uuid{high, low};
  EXPECT_THAT(uuid.As64BitPair(), Pair(high, low));
}

TEST(UuidTest, RelationalOperations) {
  Uuid uuid1{absl::MakeUint128(0x0b6ed04ca16dfc46, 0x52817f9978c13738)};
  Uuid uuid2{absl::MakeUint128(0x7bf8a7b819171919, 0x2625f208c5824254)};

  EXPECT_TRUE(uuid1 == uuid1);
  EXPECT_FALSE(uuid1 != uuid1);
  EXPECT_TRUE(uuid1 != uuid2);

  EXPECT_TRUE(uuid1 < uuid2);
  EXPECT_FALSE(uuid2 < uuid2);
  EXPECT_FALSE(uuid2 < uuid1);
  EXPECT_TRUE(uuid2 >= uuid1);
  EXPECT_TRUE(uuid2 > uuid1);
  EXPECT_TRUE(uuid1 <= uuid2);
}

TEST(UuidTest, ConversionToString) {
  Uuid uuid0;
  EXPECT_THAT(static_cast<std::string>(uuid0),
              Eq("00000000-0000-0000-0000-000000000000"));

  Uuid uuid1{absl::MakeUint128(0x0b6ed04ca16dfc46, 0x52817f9978c13738)};
  EXPECT_THAT(static_cast<std::string>(uuid1),
              Eq("0b6ed04c-a16d-fc46-5281-7f9978c13738"));

  Uuid uuid2{absl::MakeUint128(0x7bf8a7b819171919, 0x2625f208c5824254)};
  EXPECT_THAT(static_cast<std::string>(uuid2),
              Eq("7bf8a7b8-1917-1919-2625-f208c5824254"));
}

TEST(UuidTest, RoundTripStringRepresentation) {
  Uuid expected_uuid{absl::MakeUint128(0x7bf8a7b819171919, 0x2625f208c5824254)};
  auto actual_uuid = MakeUuid(static_cast<std::string>(expected_uuid));
  EXPECT_THAT(actual_uuid, IsOkAndHolds(expected_uuid));
}

TEST(UuidTest, ostream) {
  std::stringstream ss;
  ss << Uuid{absl::MakeUint128(0x0b6ed04ca16dfc46, 0x52817f9978c13738)};
  EXPECT_THAT(ss.str(), Eq("0b6ed04c-a16d-fc46-5281-7f9978c13738"));
}

TEST(UuidTest, MakeUuid) {
  // Error Paths
  EXPECT_THAT(  //
      MakeUuid(""),
      StatusIs(  //
          StatusCode::kInvalidArgument,
          StartsWith("UUID must contain 32 hexadecimal digits:")));
  EXPECT_THAT(
      MakeUuid("{0b6ed04ca16dfc4652817f9978c13738"),
      StatusIs(
          StatusCode::kInvalidArgument,
          StartsWith("UUID contains invalid character '{' at position 0:")));
  EXPECT_THAT(
      MakeUuid("0b6ed04ca16dfc4652817f9978c13738}"),
      StatusIs(  //
          StatusCode::kInvalidArgument,
          StartsWith("Extra characters \"}\" found after parsing UUID:")));
  EXPECT_THAT(
      MakeUuid("-0b6ed04ca16dfc4652817f9978c13738"),
      StatusIs(
          StatusCode::kInvalidArgument,
          StartsWith("UUID contains invalid character '-' at position 0:")));
  EXPECT_THAT(
      MakeUuid("0b6ed04ca16dfc4652817f9978c1373g"),
      StatusIs(
          StatusCode::kInvalidArgument,
          StartsWith("UUID contains invalid character 'g' at position 31:")));
  EXPECT_THAT(
      MakeUuid("0b--6ed04ca16dfc4652817f9978c13738"),
      StatusIs(
          StatusCode::kInvalidArgument,
          StartsWith("UUID contains invalid character '-' at position 3:")));
  EXPECT_THAT(  //
      MakeUuid("00-00-00-00-00-00-00-00-00-00-00-00-00-00-00"),
      StatusIs(  //
          StatusCode::kInvalidArgument,
          StartsWith("UUID must contain 32 hexadecimal digits:")));
  EXPECT_THAT(
      MakeUuid("{00000000-0000-0000-0000-000000000000-aa}"),
      StatusIs(
          StatusCode::kInvalidArgument,
          StartsWith("Extra characters \"-aa\" found after parsing UUID:")));

  // Success Paths
  EXPECT_THAT(MakeUuid("00000000000000000000000000000000"),
              IsOkAndHolds(Uuid{}));
  EXPECT_THAT(MakeUuid("0x0b6ed04ca16dfc4652817f9978c13738"),
              IsOkAndHolds(Uuid{0x0b6ed04ca16dfc46, 0x52817f9978c13738}));
  EXPECT_THAT(MakeUuid("{0b6ed04ca16dfc4652817f9978c13738}"),
              IsOkAndHolds(Uuid{0x0b6ed04ca16dfc46, 0x52817f9978c13738}));
  EXPECT_THAT(MakeUuid("{0b6ed04c-a16d-fc46-5281-7f9978c13738}"),
              IsOkAndHolds(Uuid{0x0b6ed04ca16dfc46, 0x52817f9978c13738}));
  EXPECT_THAT(MakeUuid("7Bf8-A7b8-1917-1919-2625-F208-c582-4254"),
              IsOkAndHolds(Uuid{0x7Bf8A7b819171919, 0x2625F208c5824254}));
  EXPECT_THAT(MakeUuid("{DECAFBAD-DEAD-FADE-CAFE-FEEDFACEBEEF}"),
              IsOkAndHolds(Uuid{0xdecafbaddeadfade, 0xcafefeedfacebeef}));
  EXPECT_THAT(
      MakeUuid(
          "{0-b-6-e-d-0-4-c-a-1-6-d-f-c-4-6-5-2-8-1-7-f-9-9-7-8-c-1-3-7-3-8}"),
      IsOkAndHolds(Uuid{0x0b6ed04ca16dfc46, 0x52817f9978c13738}));
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace spanner
}  // namespace cloud
}  // namespace google
