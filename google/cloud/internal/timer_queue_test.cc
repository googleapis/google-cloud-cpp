// Copyright 2023 Google LLC
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

#include "google/cloud/internal/timer_queue.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>
#include <chrono>
#include <thread>

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {
namespace {

using ::google::cloud::testing_util::IsOk;
using ::google::cloud::testing_util::StatusIs;
using testing::Each;

TEST(TimerQueueTest, ScheduleSingleRunner) {
  TimerQueue tq;
  std::thread t([&tq] { tq.Service(); });
  auto const duration = std::chrono::milliseconds(1);
  auto now = std::chrono::system_clock::now();
  auto f = tq.Schedule(now + duration);
  auto expire_time = f.get();
  tq.Shutdown();
  t.join();

  ASSERT_THAT(expire_time, IsOk());
  EXPECT_GE(*expire_time - now, duration);
}

TEST(TimerQueueTest, ScheduleAndCancelAllSingleRunner) {
  TimerQueue tq;
  auto const duration = std::chrono::seconds(60);
  auto now = std::chrono::system_clock::now();
  auto f = tq.Schedule(now + duration);
  auto f2 = tq.Schedule(now + duration);
  tq.CancelAll();
  std::thread t([&tq] { tq.Service(); });
  tq.Shutdown();
  t.join();

  auto expire_time = f.get();
  EXPECT_THAT(expire_time, StatusIs(StatusCode::kCancelled));
  auto expire_time2 = f2.get();
  EXPECT_THAT(expire_time2, StatusIs(StatusCode::kCancelled));
}

TEST(TimerQueueTest, ScheduleEarlierTimerSingleRunner) {
  TimerQueue tq;
  std::thread t([&tq] { tq.Service(); });
  auto const duration = std::chrono::milliseconds(50);
  auto now = std::chrono::system_clock::now();
  auto later = tq.Schedule(now + 2 * duration);
  auto earlier = tq.Schedule(now + duration);
  auto earlier_expire_time = earlier
                                 .then([&](auto f) {
                                   EXPECT_FALSE(later.is_ready());
                                   return f.get();
                                 })
                                 .get();
  auto later_expire_time = later.get();
  tq.Shutdown();
  t.join();

  ASSERT_THAT(earlier_expire_time, IsOk());
  ASSERT_THAT(later_expire_time, IsOk());
  EXPECT_GT(*later_expire_time, *earlier_expire_time);
}

TEST(TimerQueueTest, ScheduleMultipleRunners) {
  TimerQueue tq;
  // Schedule the timers first, so they are all ready to expire
  auto constexpr kTimers = 100;
  std::vector<future<std::thread::id>> futures(kTimers);
  auto const now = std::chrono::system_clock::now();
  std::generate(futures.begin(), futures.end(), [&, now] {
    return tq.Schedule(now).then([](auto) {
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
      return std::this_thread::get_id();
    });
  });

  auto constexpr kRunners = 16;
  std::vector<std::thread> runners(kRunners);
  std::generate(runners.begin(), runners.end(),
                [&] { return std::thread([&] { tq.Service(); }); });

  std::set<std::thread::id> ids;
  std::transform(futures.begin(), futures.end(),
                 std::inserter(ids, ids.begin()),
                 [&](auto& f) { return f.get(); });

  tq.Shutdown();
  for (auto& t : runners) t.join();

  EXPECT_GT(ids.size(), 1);
}

TEST(TimerQueueTest, ScheduleAndCancelAllMultipleRunners) {
  TimerQueue tq;
  auto const duration = std::chrono::seconds(1);
  auto now = std::chrono::system_clock::now();
  std::vector<future<StatusOr<std::chrono::system_clock::time_point>>> futures;
  for (int i = 0; i != 100; ++i) {
    futures.push_back(tq.Schedule(now + duration));
  }
  tq.CancelAll();
  auto constexpr kRunners = 8;
  std::vector<std::thread> runners;
  for (auto i = 0; i != kRunners; ++i) {
    runners.emplace_back([&] { tq.Service(); });
  }
  std::vector<StatusOr<std::chrono::system_clock::time_point>> timepoints;
  timepoints.reserve(100);
  for (auto& f : futures) {
    timepoints.push_back(f.get());
  }
  tq.Shutdown();
  for (auto& t : runners) t.join();

  EXPECT_THAT(timepoints, Each(StatusIs(StatusCode::kCancelled)));
}

TEST(TimerQueueTest, ScheduleEarlierTimerMultipleRunner) {
  TimerQueue tq;
  auto constexpr kRunners = 8;
  std::vector<std::thread> runners;
  for (auto i = 0; i != kRunners; ++i) {
    runners.emplace_back([&] { tq.Service(); });
  }
  auto const duration = std::chrono::milliseconds(50);
  auto now = std::chrono::system_clock::now();
  auto later = tq.Schedule(now + 2 * duration);
  auto earlier = tq.Schedule(now + duration);
  auto earlier_expire_time = earlier
                                 .then([&](auto f) {
                                   EXPECT_FALSE(later.is_ready());
                                   return f.get();
                                 })
                                 .get();
  auto later_expire_time = later.get();
  tq.Shutdown();
  for (auto& t : runners) t.join();

  ASSERT_THAT(earlier_expire_time, IsOk());
  ASSERT_THAT(later_expire_time, IsOk());
  EXPECT_GT(*later_expire_time, *earlier_expire_time);
}

}  // namespace
}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google
