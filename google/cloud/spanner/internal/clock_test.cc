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

#include "google/cloud/spanner/internal/clock.h"
#include "google/cloud/spanner/testing/fake_clock.h"
#include <gmock/gmock.h>
#include <memory>

namespace google {
namespace cloud {
namespace spanner {
inline namespace SPANNER_CLIENT_NS {
namespace internal {
namespace {

using ::google::cloud::spanner_testing::FakeClock;

TEST(Clock, SteadyClock) {
  auto clock = std::make_shared<SteadyClock>();
  auto now = clock->Now();
  auto now2 = clock->Now();
  // `SteadyClock::Now()` can never decrease as physical time moves forward.
  EXPECT_LE(now, now2);
}

TEST(Clock, SystemClock) {
  auto clock = std::make_shared<SystemClock>();
  // There is no guarantee that `SystemClock::Now()` can never decrease, so
  // we can't test that like we do for `SteadyClock`, so for now just make
  // sure `Now()` is callable.
  (void)clock->Now();
}

TEST(Clock, FakeClock) {
  SteadyClock real_clock;
  FakeClock<SteadyClock> clock;
  SteadyClock::time_point time(real_clock.Now());
  clock.SetTime(time);
  EXPECT_EQ(clock.Now(), time);

  time += std::chrono::minutes(3);
  clock.SetTime(time);
  EXPECT_EQ(clock.Now(), time);

  SteadyClock::duration duration = std::chrono::hours(89);
  time += duration;
  clock.AdvanceTime(duration);
  EXPECT_EQ(clock.Now(), time);
  time += duration;
  clock.AdvanceTime(duration);
  EXPECT_EQ(clock.Now(), time);
}

}  // namespace
}  // namespace internal
}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner
}  // namespace cloud
}  // namespace google
