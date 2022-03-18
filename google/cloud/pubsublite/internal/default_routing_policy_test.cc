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
#include <unordered_map>

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace pubsublite_internal {
namespace {

using google::cloud::pubsublite_internal::DefaultRoutingPolicy;

TEST(DefaultRoutingPolicyTest, RouteWithKey) {
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
  for (auto const& it : mods) {
    EXPECT_EQ(DefaultRoutingPolicy::Route(it.first, 29), it.second);
  }
}

}  // namespace
}  // namespace pubsublite_internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google
