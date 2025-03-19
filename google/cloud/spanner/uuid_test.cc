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

namespace google {
namespace cloud {
namespace spanner {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::google::cloud::testing_util::IsOkAndHolds;
using ::google::cloud::testing_util::StatusIs;
using ::testing::Eq;
using ::testing::HasSubstr;

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

  absl::uint128 expected_value = absl::MakeUint128(high, low);
  Uuid uuid{high, low};
  EXPECT_THAT(static_cast<absl::uint128>(uuid), Eq(expected_value));
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
  Uuid uuid1{absl::MakeUint128(0x0b6ed04ca16dfc46, 0x52817f9978c13738)};
  Uuid uuid2{absl::MakeUint128(0x7bf8a7b819171919, 0x2625f208c5824254)};

  EXPECT_THAT(static_cast<std::string>(uuid0),
              Eq("00000000-0000-0000-0000-000000000000"));
  EXPECT_THAT(static_cast<std::string>(uuid1),
              Eq("0b6ed04c-a16d-fc46-5281-7f9978c13738"));
  EXPECT_THAT(static_cast<std::string>(uuid2),
              Eq("7bf8a7b8-1917-1919-2625-f208c5824254"));
}

TEST(UuidTest, ostream) {
  Uuid uuid1{absl::MakeUint128(0x0b6ed04ca16dfc46, 0x52817f9978c13738)};
  std::stringstream ss;
  ss << uuid1;
  EXPECT_THAT(ss.str(), Eq("0b6ed04c-a16d-fc46-5281-7f9978c13738"));
}

struct MakeUuidTestParam {
  std::string name;
  std::string input;
  uint64_t expected_high_bits;
  uint64_t expected_low_bits;
  absl::optional<Status> error;
};
class MakeUuidTest : public ::testing::TestWithParam<MakeUuidTestParam> {};

TEST_P(MakeUuidTest, MakeUuidFromString) {
  Uuid expected_uuid{absl::MakeUint128(GetParam().expected_high_bits,
                                       GetParam().expected_low_bits)};
  StatusOr<Uuid> actual_uuid = MakeUuid(GetParam().input);
  if (GetParam().error.has_value()) {
    EXPECT_THAT(actual_uuid.status(),
                StatusIs(GetParam().error->code(),
                         HasSubstr(GetParam().error->message())));
  } else {
    EXPECT_THAT(actual_uuid, IsOkAndHolds(expected_uuid));
  }
}

INSTANTIATE_TEST_SUITE_P(
    MakeUuid, MakeUuidTest,
    testing::Values(
        // Error Paths
        MakeUuidTestParam{
            "Empty", "", 0x0, 0x0,
            Status{StatusCode::kInvalidArgument, "UUID cannot be empty"}},
        MakeUuidTestParam{
            "MissingClosingCurlyBrace", "{0b6ed04ca16dfc4652817f9978c13738",
            0x0, 0x0,
            Status{
                StatusCode::kInvalidArgument,
                "UUID missing closing '}': {0b6ed04ca16dfc4652817f9978c13738"}},
        MakeUuidTestParam{
            "MissingOpeningCurlyBrace", "0b6ed04ca16dfc4652817f9978c13738}",
            0x0, 0x0,
            Status{StatusCode::kInvalidArgument,
                   "Extra characters found after parsing UUID: }"}},
        MakeUuidTestParam{"StartsWithInvalidHyphen",
                          "-0b6ed04ca16dfc4652817f9978c13738", 0x0, 0x0,
                          Status{StatusCode::kInvalidArgument,
                                 "UUID cannot begin with '-':"}},
        MakeUuidTestParam{"ContainsInvalidCharacter",
                          "0b6ed04ca16dfc4652817f9978c1373g", 0x0, 0x0,
                          Status{StatusCode::kInvalidArgument,
                                 "UUID contains invalid character (g):"}},
        MakeUuidTestParam{"ContainsConsecutiveHyphens",
                          "0b--6ed04ca16dfc4652817f9978c13738", 0x0, 0x0,
                          Status{StatusCode::kInvalidArgument,
                                 "UUID cannot contain consecutive hyphens: "
                                 "0b--6ed04ca16dfc4652817f9978c13738"}},
        MakeUuidTestParam{"InsufficientDigits",
                          "00-00-00-00-00-00-00-00-00-00-00-00-00-00-00", 0x0,
                          0x0,
                          Status{StatusCode::kInvalidArgument,
                                 "UUID must contain 32 hexadecimal digits:"}},
        // Success Paths
        MakeUuidTestParam{
            "Zero", "00000000000000000000000000000000", 0x0, 0x0, {}},
        MakeUuidTestParam{"CurlyBraced",
                          "{0b6ed04ca16dfc4652817f9978c13738}",
                          0x0b6ed04ca16dfc46,
                          0x52817f9978c13738,
                          {}},
        MakeUuidTestParam{
            "CurlyBracedFullyHyphenated",
            "{0-b-6-e-d-0-4-c-a-1-6-d-f-c-4-6-5-2-8-1-7-f-9-9-7-8-c-1-3-7-3-8}",
            0x0b6ed04ca16dfc46,
            0x52817f9978c13738,
            {}},
        MakeUuidTestParam{"CurlyBracedConventionallyHyphenated",
                          "{0b6ed04c-a16d-fc46-5281-7f9978c13738}",
                          0x0b6ed04ca16dfc46,
                          0x52817f9978c13738,
                          {}},
        MakeUuidTestParam{"MixedCase16BitHyphenSeparated",
                          "7Bf8-A7b8-1917-1919-2625-F208-c582-4254",
                          0x7Bf8A7b819171919,
                          0x2625F208c5824254,
                          {}},
        MakeUuidTestParam{"AllUpperCase",
                          "{DECAFBAD-DEAD-FADE-CAFE-FEEDFACEBEEF}",
                          0xdecafbaddeadfade,
                          0xcafefeedfacebeef,
                          {}}),
    [](testing::TestParamInfo<MakeUuidTest::ParamType> const& info) {
      return info.param.name;
    });

TEST(UuidTest, RoundTripStringRepresentation) {
  Uuid expected_uuid{absl::MakeUint128(0x7bf8a7b819171919, 0x2625f208c5824254)};
  auto str = static_cast<std::string>(expected_uuid);
  auto actual_uuid = MakeUuid(str);
  EXPECT_THAT(actual_uuid, IsOkAndHolds(expected_uuid));
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace spanner
}  // namespace cloud
}  // namespace google
