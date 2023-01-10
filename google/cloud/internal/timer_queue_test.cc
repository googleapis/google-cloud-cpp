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
  auto tq = TimerQueue::Create();
  std::thread t([&tq] { tq->Service(); });
  auto const duration = std::chrono::milliseconds(1);
  auto now = std::chrono::system_clock::now();
  auto f = tq->Schedule(now + duration);
  auto expire_time = f.get();
  tq->Shutdown();
  t.join();

  ASSERT_THAT(expire_time, IsOk());
  EXPECT_GE(*expire_time - now, duration);
}

/// @test Verify timers can be cancelled.
TEST(TimerQueueTest, CancelTimers) {
  auto tq = TimerQueue::Create();

  // Track all the observed thread ids.
  auto constexpr kRunners = 16U;

  // Schedule the timers first, but make them so long that they should not
  // expire before we get to cancel them. Note that the test will be terminated
  // by the default test timeout before the scheduled time.
  std::vector<future<Status>> futures(kRunners);
  auto const now = std::chrono::system_clock::now();
  auto const tp = now + std::chrono::hours(1);
  std::generate(futures.begin(), futures.end(), [&] {
    return tq->Schedule(tp).then([](auto f) { return f.get().status(); });
  });

  std::vector<std::thread> runners(kRunners);
  std::generate(runners.begin(), runners.end(),
                [&] { return std::thread([&] { tq->Service(); }); });

  // Cancel all the timers. Leave the servicing threads running.
  for (auto& f : futures) f.cancel();
  // Wait for all timers and capture the status.
  std::vector<Status> status(futures.size());
  std::transform(futures.begin(), futures.end(), status.begin(),
                 [](auto& f) { return f.get(); });
  EXPECT_THAT(status, Each(StatusIs(StatusCode::kCancelled)));

  // Only shutdown the timer queue once the timers are successfully cancelled.
  tq->Shutdown();
  for (auto& t : runners) t.join();
}

TEST(TimerQueueTest, ScheduleEarlierTimerSingleRunner) {
  auto tq = TimerQueue::Create();
  std::thread t([&tq] { tq->Service(); });
  auto const duration = std::chrono::milliseconds(50);
  auto now = std::chrono::system_clock::now();
  auto later = tq->Schedule(now + 2 * duration);
  auto earlier = tq->Schedule(now + duration);
  auto earlier_expire_time = earlier
                                 .then([&](auto f) {
                                   EXPECT_FALSE(later.is_ready());
                                   return f.get();
                                 })
                                 .get();
  auto later_expire_time = later.get();
  tq->Shutdown();
  t.join();

  ASSERT_THAT(earlier_expire_time, IsOk());
  ASSERT_THAT(later_expire_time, IsOk());
  EXPECT_GT(*later_expire_time, *earlier_expire_time);
}

TEST(TimerQueueTest, ScheduleMultipleRunners) {
  auto tq = TimerQueue::Create();

  // Track all the observed thread ids.
  auto constexpr kRunners = 16U;
  std::mutex mu;
  std::condition_variable cv;
  std::set<std::thread::id> ids;
  auto insert_thread_id = [&](auto) {
    std::unique_lock<std::mutex> lk(mu);
    ids.insert(std::this_thread::get_id());
    if (ids.size() == kRunners) {
      lk.unlock();
      cv.notify_all();
      return;
    }
    // Block after inserting each thread id, to force all threads to
    // participate.
    cv.wait(lk, [&] { return ids.size() >= kRunners; });
  };

  // Schedule the timers first, so they are all ready to expire
  std::vector<future<void>> futures(kRunners);
  auto const now = std::chrono::system_clock::now();
  std::generate(futures.begin(), futures.end(),
                [&, now] { return tq->Schedule(now).then(insert_thread_id); });

  std::vector<std::thread> runners(kRunners);
  std::generate(runners.begin(), runners.end(),
                [&] { return std::thread([&] { tq->Service(); }); });

  for (auto& f : futures) f.get();

  tq->Shutdown();
  for (auto& t : runners) t.join();

  EXPECT_EQ(ids.size(), kRunners);
}

TEST(TimerQueueTest, ShutdownMultipleRunners) {
  auto tq = TimerQueue::Create();

  // Track all the observed thread ids.
  auto constexpr kRunners = 16U;
  std::mutex mu;
  std::condition_variable cv;
  std::set<std::thread::id> ids;
  auto insert_thread_id = [&](auto f) {
    std::unique_lock<std::mutex> lk(mu);
    ids.insert(std::this_thread::get_id());
    if (ids.size() == kRunners) {
      lk.unlock();
      cv.notify_all();
      return f.get().status();
    }
    // Block after inserting each thread id, to force all threads to
    // participate.
    cv.wait(lk, [&] { return ids.size() >= kRunners; });
    return f.get().status();
  };

  // Schedule the timers first, but make them so long that they should not
  // expire before we call Shutdown(). Note that the test will be terminated by
  // the default test timeout before this expiration, and the test fails if the
  // timers expires successfully.
  std::vector<future<Status>> futures(kRunners);
  auto const now = std::chrono::system_clock::now();
  auto const tp = now + std::chrono::hours(1);
  std::generate(futures.begin(), futures.end(),
                [&] { return tq->Schedule(tp).then(insert_thread_id); });

  std::vector<std::thread> runners(kRunners);
  std::generate(runners.begin(), runners.end(),
                [&] { return std::thread([&] { tq->Service(); }); });

  tq->Shutdown();
  for (auto& t : runners) t.join();
  std::vector<Status> status(futures.size());
  std::transform(futures.begin(), futures.end(), status.begin(),
                 [](auto& f) { return f.get(); });

  EXPECT_EQ(ids.size(), kRunners);
  EXPECT_THAT(status, Each(StatusIs(StatusCode::kCancelled)));
}

TEST(TimerQueueTest, ScheduleEarlierTimerMultipleRunner) {
  auto tq = TimerQueue::Create();
  auto constexpr kRunners = 8;
  std::vector<std::thread> runners;
  for (auto i = 0; i != kRunners; ++i) {
    runners.emplace_back([&] { tq->Service(); });
  }
  auto const duration = std::chrono::milliseconds(50);
  auto now = std::chrono::system_clock::now();
  auto later = tq->Schedule(now + 2 * duration);
  auto earlier = tq->Schedule(now + duration);
  auto earlier_expire_time = earlier
                                 .then([&](auto f) {
                                   EXPECT_FALSE(later.is_ready());
                                   return f.get();
                                 })
                                 .get();
  auto later_expire_time = later.get();
  tq->Shutdown();
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
