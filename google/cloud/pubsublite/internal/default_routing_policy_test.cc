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

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace pubsublite_internal {
namespace {

using google::cloud::pubsublite_internal::DefaultRoutingPolicy;

TEST(DefaultRoutingPolicyTest, RouteWithKey) {
  EXPECT_EQ(DefaultRoutingPolicy::Route("oaisdhfoiahsd", 29), 18);
  EXPECT_EQ(DefaultRoutingPolicy::Route("P(#*YNPOIUDF", 29), 9);
  EXPECT_EQ(DefaultRoutingPolicy::Route("LCIUNDFPOASIUN", 29), 8);
  EXPECT_EQ(DefaultRoutingPolicy::Route(";odsfiupoius", 29), 9);
  EXPECT_EQ(DefaultRoutingPolicy::Route("OPISUDfpoiu", 29), 2);
  EXPECT_EQ(DefaultRoutingPolicy::Route("dokjwO:IDf", 29), 21);
  EXPECT_EQ(DefaultRoutingPolicy::Route("%^&*", 29), 19);
  EXPECT_EQ(DefaultRoutingPolicy::Route("XXXXXXXXX", 29), 15);
  EXPECT_EQ(DefaultRoutingPolicy::Route("dpcollins", 29), 28);
  EXPECT_EQ(DefaultRoutingPolicy::Route("#()&$IJHLOIURF", 29), 2);
  EXPECT_EQ(DefaultRoutingPolicy::Route("dfasiduyf", 29), 6);
  EXPECT_EQ(DefaultRoutingPolicy::Route("983u2poer", 29), 3);
  EXPECT_EQ(DefaultRoutingPolicy::Route("8888888", 29), 6);
  EXPECT_EQ(DefaultRoutingPolicy::Route("OPUIPOUYPOIOPUIOIPUOUIPJOP", 29), 2);
  EXPECT_EQ(DefaultRoutingPolicy::Route("x", 29), 16);
}

}  // namespace
}  // namespace pubsublite_internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google
