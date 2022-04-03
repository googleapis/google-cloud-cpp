// Copyright 2022 Google LLC
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

#include "google/cloud/pubsublite/internal/default_routing_policy.h"
#include <gmock/gmock.h>
#include <array>
#include <unordered_map>

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace pubsublite_internal {

using google::cloud::pubsublite_internal::DefaultRoutingPolicy;

TEST(DefaultRoutingPolicyTest, RouteWithKey) {
  // same list of test values as in other client libraries
  DefaultRoutingPolicy rp;
  std::unordered_map<std::string, std::uint64_t> mods = {
      {"oaisdhfoiahsd", 18},
      {"P(#*YNPOIUDF", 9},
      {"LCIUNDFPOASIUN", 8},
      {";odsfiupoius", 9},
      {"OPISUDfpoiu", 2},
      {"dokjwO:IDf", 21},
      {"%^&*", 19},
      {"XXXXXXXXX", 15},
      {"dpcollins", 28},
      {"#()&$IJHLOIURF", 2},
      {"dfasiduyf", 6},
      {"983u2poer", 3},
      {"8888888", 6},
      {"OPUIPOUYPOIOPUIOIPUOUIPJOP", 2},
      {"x", 16}};
  for (auto const& kv : mods) {
    EXPECT_EQ(rp.Route(kv.first, 29), kv.second);
  }
}

TEST(DefaultRoutingPolicyTest, RouteWithoutKey) {
  unsigned int num_partitions = 29;
  DefaultRoutingPolicy rp;
  std::uint32_t initial_partition = rp.Route(num_partitions);
  for (unsigned int i = 0; i < num_partitions; ++i) {
    std::uint32_t next_partition = rp.Route(num_partitions);
    EXPECT_EQ((initial_partition + 1) % num_partitions,
              next_partition % num_partitions);
    initial_partition = next_partition;
  }
}

// expected values obtained from Python3 REPL
TEST(TestGetMod, MaxValue) {
  std::array<std::uint8_t, 32> arr{255, 255, 255, 255, 255, 255, 255, 255,
                                   255, 255, 255, 255, 255, 255, 255, 255,
                                   255, 255, 255, 255, 255, 255, 255, 255,
                                   255, 255, 255, 255, 255, 255, 255, 255};
  EXPECT_EQ(GetMod(arr, 2), 1);
  EXPECT_EQ(GetMod(arr, 18), 15);
  EXPECT_EQ(GetMod(arr, 100), 35);
  EXPECT_EQ(GetMod(arr, 10023), 5397);
  EXPECT_EQ(GetMod(arr, UINT8_MAX), 0);
  EXPECT_EQ(GetMod(arr, UINT32_MAX - 1), 255);
}

TEST(TestGetMod, OneLessThanMaxValue) {
  std::array<std::uint8_t, 32> arr{255, 255, 255, 255, 255, 255, 255, 255,
                                   255, 255, 255, 255, 255, 255, 255, 255,
                                   255, 255, 255, 255, 255, 255, 255, 255,
                                   255, 255, 255, 255, 255, 255, 255, 254};
  EXPECT_EQ(GetMod(arr, 2), 0);
  EXPECT_EQ(GetMod(arr, 18), 14);
  EXPECT_EQ(GetMod(arr, 100), 34);
  EXPECT_EQ(GetMod(arr, 10023), 5396);
  EXPECT_EQ(GetMod(arr, UINT8_MAX), 254);
  EXPECT_EQ(GetMod(arr, UINT32_MAX - 1), 254);
}

TEST(TestGetMod, Zeros) {
  std::array<std::uint8_t, 32> arr{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                                   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                                   0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
  EXPECT_EQ(GetMod(arr, 2), 0);
  EXPECT_EQ(GetMod(arr, 18), 0);
  EXPECT_EQ(GetMod(arr, 100), 0);
  EXPECT_EQ(GetMod(arr, 10023), 0);
  EXPECT_EQ(GetMod(arr, UINT8_MAX), 0);
  EXPECT_EQ(GetMod(arr, UINT32_MAX - 1), 0);
}

TEST(TestGetMod, ArbitraryValue) {
  std::array<std::uint8_t, 32> arr{255, 255, 255, 255, 255, 255, 2,   255,
                                   5,   79,  255, 255, 255, 255, 80,  255,
                                   255, 255, 8,   255, 255, 4,   255, 255,
                                   78,  255, 255, 100, 255, 255, 255, 254};
  EXPECT_EQ(GetMod(arr, 10), 0);
  EXPECT_EQ(GetMod(arr, 109), 4);
  EXPECT_EQ(GetMod(arr, 10023), 3346);
  EXPECT_EQ(GetMod(arr, 109000), 60390);
  EXPECT_EQ(GetMod(arr, UINT8_MAX), 100);
  EXPECT_EQ(GetMod(arr, UINT32_MAX - 1), 1136793478);
}

TEST(TestGetMod, ArbitraryValue1) {
  std::array<std::uint8_t, 32> arr{0, 48, 0, 0,   60, 0,   0, 56, 0,  99, 0,
                                   0, 0,  0, 0,   90, 231, 0, 89, 0,  27, 80,
                                   0, 0,  0, 254, 0,  0,   0, 0,  23, 0};
  EXPECT_EQ(GetMod(arr, 109001), 68945);
  EXPECT_EQ(GetMod(arr, 102301), 93535);
  EXPECT_EQ(GetMod(arr, 23), 13);
  EXPECT_EQ(GetMod(arr, UINT8_MAX), 37);
  EXPECT_EQ(GetMod(arr, UINT32_MAX - 1), 3416191692);
}

}  // namespace pubsublite_internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google
