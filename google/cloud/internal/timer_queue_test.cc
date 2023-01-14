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
#include "google/cloud/testing_util/chrono_output.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>
#include <sstream>
#include <thread>

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {
namespace {

using ::google::cloud::testing_util::IsOk;
using ::google::cloud::testing_util::StatusIs;
using ::testing::Each;
using ::testing::ElementsAreArray;
using ::testing::Eq;
using ::testing::IsSupersetOf;
using ::testing::Not;
using ::testing::WhenSorted;

TEST(TimerQueueTest, ScheduleSingleRunner) {
  auto tq = TimerQueue::Create();
  std::thread t([tq] { tq->Service(); });
  auto const duration = std::chrono::milliseconds(1);
  auto const tp = std::chrono::system_clock::now() + duration;
  auto f = tq->Schedule(tp);
  auto expire_time = f.get();
  tq->Shutdown();
  t.join();

  ASSERT_THAT(expire_time, IsOk());
  EXPECT_EQ(*expire_time, tp);
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
                [&] { return std::thread([tq] { tq->Service(); }); });

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

/// @test Verify timers are executed in order.
TEST(TimerQueueTest, SingleRunnerOrdering) {
  // Populate some delays with intentionally out-of-order and in-order data.
  using duration = std::chrono::system_clock::duration;
  std::vector<duration> const delays{
      duration(10), duration(9),  duration(8), duration(5), duration(20),
      duration(21), duration(22), duration(4), duration(3),
  };

  // Create the timer queue, but do not run the servicing thread until the
  // timers are scheduled. Otherwise "later" timers may be immediately expired
  // because clocks are hard.
  auto tq = TimerQueue::Create();

  auto const start = std::chrono::system_clock::now();
  std::vector<std::chrono::system_clock::time_point> expected(delays.size());
  std::transform(delays.begin(), delays.end(), expected.begin(),
                 [start](auto d) { return start + d; });
  // We are going to store the expiration of each timer in this vector. Note
  // that first it is used only by the timer servicing thread (`std::thread t`)
  // below, and then by the testing thread.  Synchronization is provided by
  // the thread join() call.
  std::vector<std::chrono::system_clock::time_point> actual;
  std::vector<future<Status>> timers(expected.size());
  std::transform(expected.begin(), expected.end(), timers.begin(), [&](auto d) {
    // As the timers expire we add their time (if successful) to `expirations`,
    // and return the `Status`.
    return tq->Schedule(d).then([&, d](auto f) {
      auto e = f.get();
      if (!e) return std::move(e).status();
      if (*e != d) {
        std::ostringstream os;
        os << "mismatched expiration, expected=" << d << ", got=" << *e;
        return Status(StatusCode::kFailedPrecondition, std::move(os).str());
      }
      actual.push_back(*std::move(e));
      return Status{};
    });
  });

  // Start expiring timers.
  std::thread t([tq] { tq->Service(); });

  // Wait for all the timers to expire.
  std::vector<Status> status(timers.size());
  std::transform(timers.begin(), timers.end(), status.begin(),
                 [](auto& f) { return f.get(); });

  tq->Shutdown();
  t.join();

  EXPECT_THAT(status, Each(IsOk()));

  // At this point we expect the expirations to be in order and match the
  // expiration times set in `expected`.
  EXPECT_THAT(expected, WhenSorted(ElementsAreArray(actual)));
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
                [&] { return std::thread([tq] { tq->Service(); }); });

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
                [&] { return std::thread([tq] { tq->Service(); }); });

  tq->Shutdown();
  for (auto& t : runners) t.join();
  std::vector<Status> status(futures.size());
  std::transform(futures.begin(), futures.end(), status.begin(),
                 [](auto& f) { return f.get(); });

  EXPECT_EQ(ids.size(), kRunners);
  EXPECT_THAT(status, Each(StatusIs(StatusCode::kCancelled)));
}

TEST(TimerQueueTest, ScheduleWithCallbacks) {
  auto tq = TimerQueue::Create();

  // Start a number of runners.
  auto constexpr kRunners = 4;
  std::vector<std::thread> runners(kRunners);
  std::generate(runners.begin(), runners.end(),
                [tq] { return std::thread([tq] { tq->Service(); }); });

  // Now start a number of timers with a callback.  We expect these callbacks
  // to run only in the runner threads.
  auto constexpr kTimers = 16 * 100;
  std::vector<future<std::thread::id>> timers(kTimers);
  std::generate(timers.begin(), timers.end(), [tq]() {
    return tq->Schedule(std::chrono::system_clock::now(),
                        [](auto) { return std::this_thread::get_id(); });
  });

  std::set<std::thread::id> actual;
  std::transform(timers.begin(), timers.end(),
                 std::inserter(actual, actual.end()),
                 [](auto& f) { return f.get(); });

  std::set<std::thread::id> expected;
  std::transform(runners.begin(), runners.end(),
                 std::inserter(expected, expected.end()),
                 [](auto& t) { return t.get_id(); });

  tq->Shutdown();
  for (auto& t : runners) t.join();

  EXPECT_THAT(actual, Each(Not(Eq(std::this_thread::get_id()))));
  EXPECT_THAT(expected, IsSupersetOf(actual));
}

TEST(TimerQueueTest, ScheduleImmediatelyWithCallbacks) {
  auto tq = TimerQueue::Create();

  // Start a number of runners.
  auto constexpr kRunners = 4;
  std::vector<std::thread> runners(kRunners);
  std::generate(runners.begin(), runners.end(),
                [tq] { return std::thread([tq] { tq->Service(); }); });

  // Now start a number of timers with a callback.  We expect these callbacks
  // to run only in the runner threads.
  auto constexpr kTimers = 16 * 100;
  std::vector<future<std::thread::id>> timers(kTimers);
  std::generate(timers.begin(), timers.end(), [tq]() {
    return tq->Schedule([](auto) { return std::this_thread::get_id(); });
  });

  std::set<std::thread::id> actual;
  std::transform(timers.begin(), timers.end(),
                 std::inserter(actual, actual.end()),
                 [](auto& f) { return f.get(); });

  std::set<std::thread::id> expected;
  std::transform(runners.begin(), runners.end(),
                 std::inserter(expected, expected.end()),
                 [](auto& t) { return t.get_id(); });

  tq->Shutdown();
  for (auto& t : runners) t.join();

  EXPECT_THAT(actual, Each(Not(Eq(std::this_thread::get_id()))));
  EXPECT_THAT(expected, IsSupersetOf(actual));
}

}  // namespace
}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google
