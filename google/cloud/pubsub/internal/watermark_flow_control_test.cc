// Copyright 2020 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "google/cloud/pubsub/internal/watermark_flow_control.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace pubsub_internal {
inline namespace GOOGLE_CLOUD_CPP_PUBSUB_NS {
namespace {

TEST(WatermarkFlowControlCountOnlyTest, Single) {
  WatermarkFlowControlCountOnly control(0, 1);
  EXPECT_TRUE(control.MaybeAdmit());
  EXPECT_FALSE(control.MaybeAdmit());
  EXPECT_FALSE(control.MaybeAdmit());
  EXPECT_FALSE(control.MaybeAdmit());
  EXPECT_TRUE(control.Release());
  EXPECT_TRUE(control.MaybeAdmit());
  EXPECT_TRUE(control.Release());
  EXPECT_TRUE(control.MaybeAdmit());
  EXPECT_TRUE(control.Release());
  EXPECT_TRUE(control.MaybeAdmit());
}

TEST(WatermarkFlowControlCountOnlyTest, Basic) {
  WatermarkFlowControlCountOnly control(2, 4);
  EXPECT_TRUE(control.MaybeAdmit());
  EXPECT_TRUE(control.MaybeAdmit());
  EXPECT_TRUE(control.MaybeAdmit());
  EXPECT_TRUE(control.MaybeAdmit());
  EXPECT_FALSE(control.MaybeAdmit());
  EXPECT_FALSE(control.MaybeAdmit());
  EXPECT_FALSE(control.MaybeAdmit());

  EXPECT_FALSE(control.Release());
  EXPECT_TRUE(control.Release());
  EXPECT_TRUE(control.Release());
  EXPECT_TRUE(control.MaybeAdmit());
  EXPECT_TRUE(control.MaybeAdmit());
  EXPECT_TRUE(control.MaybeAdmit());
  EXPECT_FALSE(control.MaybeAdmit());
}

TEST(WatermarkFlowControlTest, CountLimited) {
  WatermarkFlowControl control(2, 4, 200, 400);
  EXPECT_TRUE(control.MaybeAdmit(1));
  EXPECT_TRUE(control.MaybeAdmit(1));
  EXPECT_TRUE(control.MaybeAdmit(1));
  EXPECT_TRUE(control.MaybeAdmit(1));
  EXPECT_FALSE(control.MaybeAdmit(1));
  EXPECT_FALSE(control.MaybeAdmit(1));
  EXPECT_FALSE(control.MaybeAdmit(1));

  EXPECT_FALSE(control.Release(1));
  EXPECT_TRUE(control.Release(1));
  EXPECT_TRUE(control.Release(1));
  EXPECT_TRUE(control.MaybeAdmit(1));
  EXPECT_TRUE(control.MaybeAdmit(1));
  EXPECT_TRUE(control.MaybeAdmit(1));
  EXPECT_FALSE(control.MaybeAdmit(1));
}

TEST(WatermarkFlowControlTest, SizeLimited) {
  WatermarkFlowControl control(2, 4, 200, 400);
  EXPECT_TRUE(control.MaybeAdmit(100));
  EXPECT_TRUE(control.MaybeAdmit(100));
  EXPECT_TRUE(control.MaybeAdmit(100));
  EXPECT_TRUE(control.MaybeAdmit(100));
  EXPECT_FALSE(control.MaybeAdmit(1));
  EXPECT_FALSE(control.MaybeAdmit(1));
  EXPECT_FALSE(control.MaybeAdmit(1));

  EXPECT_FALSE(control.Release(50));
  EXPECT_FALSE(control.Release(50));
  EXPECT_FALSE(control.MaybeAdmit(100));
  EXPECT_FALSE(control.Release(50));
  EXPECT_TRUE(control.Release(50));
  EXPECT_TRUE(control.Release(50));
  EXPECT_TRUE(control.MaybeAdmit(300));
}

}  // namespace
}  // namespace GOOGLE_CLOUD_CPP_PUBSUB_NS
}  // namespace pubsub_internal
}  // namespace cloud
}  // namespace google
