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
using ::testing::Eq;

TEST(TimerQueueTest, ScheduleSingleRunner) {
  TimerQueue<std::chrono::system_clock> tq;
  std::thread t([&tq] { tq.Service(); });
  auto const duration = std::chrono::milliseconds(1);
  auto now = std::chrono::system_clock::now();
  auto f = tq.Schedule(now + duration);
  auto expire_time = f.get();
  tq.Shutdown();
  t.join();

  EXPECT_THAT(expire_time, IsOk());
  EXPECT_GE(*expire_time - now, duration);
  EXPECT_THAT(tq.expired_timer_counter(), Eq(1));
}

TEST(TimerQueueTest, ScheduleAndCancelAll) {
  TimerQueue<std::chrono::system_clock> tq;
  std::thread t([&tq] { tq.Service(); });
  auto const duration = std::chrono::seconds(60);
  auto now = std::chrono::system_clock::now();
  auto f = tq.Schedule(now + duration);
  auto f2 = tq.Schedule(now + duration);
  tq.CancelAll();
  tq.Shutdown();
  t.join();

  auto expire_time = f.get();
  EXPECT_THAT(expire_time, StatusIs(StatusCode::kCancelled));
  auto expire_time2 = f2.get();
  EXPECT_THAT(expire_time2, StatusIs(StatusCode::kCancelled));
  EXPECT_THAT(tq.expired_timer_counter(), Eq(0));
}

TEST(TimerQueueTest, ScheduleMultipleRunners) {
  TimerQueue<std::chrono::system_clock> tq;
  auto constexpr kRunners = 8;
  std::vector<std::thread> runners;
  // When a runner expires a timer, we want to add it to this set.
  std::mutex mu;
  std::set<std::thread::id> expiring_runner_ids;  // GUARDED_BY mu
  for (auto i = 0; i != kRunners; ++i) {
    runners.emplace_back([&] {
      tq.Service([&] {
        std::lock_guard<std::mutex> lk(mu);
        expiring_runner_ids.insert(std::this_thread::get_id());
      });
    });
  }

  auto const duration = std::chrono::seconds(1);
  auto now = std::chrono::system_clock::now();
  std::vector<future<StatusOr<std::chrono::system_clock::time_point>>> futures;
  futures.reserve(100);
  for (int i = 0; i != 100; ++i) {
    futures.push_back(tq.Schedule(now + duration));
  }
  std::vector<StatusOr<std::chrono::system_clock::time_point>> timepoints;
  timepoints.reserve(100);
  for (auto& f : futures) {
    timepoints.push_back(f.get());
  }
  tq.Shutdown();
  for (auto& t : runners) t.join();

  EXPECT_THAT(timepoints, Each(IsOk()));
  // Verify expiring timers are not handled serially by a single runner.
  EXPECT_GT(expiring_runner_ids.size(), 1);
  EXPECT_THAT(tq.expired_timer_counter(), Eq(100));
}

TEST(TimerQueueTest, ScheduleAndCancelAllMultipleRunners) {
  TimerQueue<std::chrono::system_clock> tq;
  auto constexpr kRunners = 8;
  std::vector<std::thread> runners;
  for (auto i = 0; i != kRunners; ++i) {
    runners.emplace_back([&] { tq.Service(); });
  }

  auto const duration = std::chrono::seconds(1);
  auto now = std::chrono::system_clock::now();
  std::vector<future<StatusOr<std::chrono::system_clock::time_point>>> futures;
  futures.reserve(100);
  for (int i = 0; i != 100; ++i) {
    futures.push_back(tq.Schedule(now + duration));
  }
  tq.CancelAll();
  std::vector<StatusOr<std::chrono::system_clock::time_point>> timepoints;
  timepoints.reserve(100);
  for (auto& f : futures) {
    timepoints.push_back(f.get());
  }
  tq.Shutdown();
  for (auto& t : runners) t.join();

  EXPECT_THAT(timepoints, Each(StatusIs(StatusCode::kCancelled)));
  EXPECT_THAT(tq.expired_timer_counter(), Eq(0));
}

}  // namespace
}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google
