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
#include "google/cloud/future.h"
#include "google/cloud/status_or.h"
#include "google/cloud/version.h"
#include <chrono>
#include <map>
#include <mutex>

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {

future<StatusOr<std::chrono::system_clock::time_point>> TimerQueue::Schedule(
    std::chrono::system_clock::time_point tp) {
  auto p = PromiseType();
  auto f = p.get_future();
  bool should_notify = false;
  {
    std::lock_guard<std::mutex> lk(mu_);
    if (shutdown_)
      return make_ready_future(StatusOr<std::chrono::system_clock::time_point>(
          Status(StatusCode::kCancelled, "TimerQueue already shutdown")));
    auto iter = timers_.emplace(tp, std::move(p));
    should_notify = iter == timers_.begin();
  }
  if (should_notify) cv_.notify_all();
  return f;
}

void TimerQueue::Shutdown() {
  std::unique_lock<std::mutex> const lk(mu_);
  shutdown_ = true;
  cv_.notify_all();
}

// Timers added via Schedule should be managed by one or more threads that
// call Service. Calls to Service only return after Shutdown has been called.
void TimerQueue::Service() {
  while (true) {
    std::unique_lock<std::mutex> lk(mu_);
    auto const until = NextExpiration(lk);
    // TODO(#10521): Predicate condition ne < until may need to be reworked.
    auto predicate = [this, until, &lk] {
      auto const ne = NextExpiration(lk);
      return shutdown_ || ne < until ||
             HasExpiredTimer(lk, std::chrono::system_clock::now());
    };
    // TODO(#10512): Only wake one thread to service the next timer instead
    // of all of them.
    cv_.wait_until(lk, until, predicate);
    if (shutdown_) return NotifyShutdown(std::move(lk));
    auto now = std::chrono::system_clock::now();
    if (HasExpiredTimer(lk, now)) {
      ExpireTimer(std::move(lk), now);
    }
  }
}

void TimerQueue::CancelAll() {
  std::multimap<std::chrono::system_clock::time_point, PromiseType> timers;
  {
    std::lock_guard<std::mutex> lk(mu_);
    std::swap(timers, timers_);
  }
  for (auto& timer : timers) {
    timer.second.set_value(Status(StatusCode::kCancelled, "Timer cancelled"));
  }
}

// Requires a lock to be held before calling.
bool TimerQueue::HasExpiredTimer(std::unique_lock<std::mutex> const&,
                                 std::chrono::system_clock::time_point tp) {
  return !timers_.empty() && timers_.begin()->first < tp;
}

void TimerQueue::ExpireTimer(std::unique_lock<std::mutex> lk,
                             std::chrono::system_clock::time_point tp) {
  auto p = std::move(timers_.begin()->second);
  timers_.erase(timers_.begin());
  auto const has_expired_timers = HasExpiredTimer(lk, tp);
  lk.unlock();
  // We do not want to expire all the timers in this thread, as this would
  // serialize the expiration.  Instead, we pick another thread to continue
  // expiring timers.
  if (has_expired_timers) cv_.notify_one();
  p.set_value(tp);
}

void TimerQueue::NotifyShutdown(std::unique_lock<std::mutex> lk) {
  auto timers = std::move(timers_);
  lk.unlock();
  for (auto& timer : timers) {
    timer.second.set_value(
        Status(StatusCode::kCancelled, "TimerQueue shutdown"));
  }
}

// Requires a lock to be held before calling.
std::chrono::system_clock::time_point TimerQueue::NextExpiration(
    std::unique_lock<std::mutex> const&) {
  // #TODO(#10522): Determine whether or not to use a better value for kMaxSleep
  //  or remove it entirely in favor of a call to wait on the cv.
  auto constexpr kMaxSleep = std::chrono::seconds(3600);
  if (timers_.empty()) return std::chrono::system_clock::now() + kMaxSleep;
  return timers_.begin()->first;
}

}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google
