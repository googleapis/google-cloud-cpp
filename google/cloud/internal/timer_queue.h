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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_TIMER_QUEUE_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_TIMER_QUEUE_H

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

template <typename Clock>
class TimerQueue {
 public:
  using TimePoint = typename Clock::time_point;

  TimerQueue() = default;
  TimerQueue(TimerQueue const&) = delete;
  TimerQueue& operator=(TimerQueue const&) = delete;

  future<StatusOr<TimePoint>> Schedule(TimePoint tp) {
    auto p = PromiseType();
    auto f = p.get_future();
    bool should_notify;
    {
      std::lock_guard<std::mutex> lk(mu_);
      if (shutdown_)
        return make_ready_future(StatusOr<TimePoint>(
            Status(StatusCode::kCancelled, "Queue already shutdown.")));
      auto iter = timers_.emplace(tp, std::move(p));
      should_notify = iter == timers_.begin();
    }
    if (should_notify) cv_.notify_all();
    return f;
  }

  void Shutdown() {
    std::unique_lock<std::mutex> const lk(mu_);
    shutdown_ = true;
    cv_.notify_all();
  }

  // Threads monitoring the queue and expiring timers should call Service.
  void Service() {
    while (true) {
      std::unique_lock<std::mutex> lk(mu_);
      auto const until = NextExpiration(lk);
      auto predicate = [this, until, &lk] {
        auto const ne = NextExpiration(lk);
        return shutdown_ || ne < until || HasExpiredTimers(lk, Clock::now());
      };
      // TODO(#10512): Only wake one thread to service the next timer instead
      // of all of them.
      cv_.wait_until(lk, until, predicate);
      if (shutdown_) return NotifyShutdown(std::move(lk));
      if (HasExpiredTimers(lk, Clock::now())) {
        ExpireTimers(std::move(lk), Clock::now());
      }
    }
  }

  void CancelAll() {
    std::multimap<TimePoint, PromiseType> timers;
    {
      std::lock_guard<std::mutex> lk(mu_);
      std::swap(timers, timers_);
    }
    for (auto& timer : timers) {
      timer.second.set_value(
          Status(StatusCode::kCancelled, "Timer cancelled."));
    }
  }

 private:
  bool HasExpiredTimers(std::unique_lock<std::mutex> const&, TimePoint tp) {
    return !timers_.empty() && timers_.begin()->first < tp;
  }
  void ExpireTimers(std::unique_lock<std::mutex> lk, TimePoint tp) {
    auto p = std::move(timers_.begin()->second);
    timers_.erase(timers_.begin());
    auto const has_expired_timers =
        !timers_.empty() && timers_.begin()->first < tp;
    lk.unlock();
    // We do not want to expire all the timers in this thread, as this would
    // serialize the expiration.  Instead, we pick another thread to continue
    // expiring timers.
    if (has_expired_timers) cv_.notify_one();
    p.set_value(tp);
  }

  void NotifyShutdown(std::unique_lock<std::mutex> lk) {
    auto timers = std::move(timers_);
    lk.unlock();
    for (auto& kv : timers) {
      kv.second.set_value(
          Status(StatusCode::kCancelled, "Timer queue shutdown."));
    }
  }

  TimePoint NextExpiration(std::unique_lock<std::mutex> const&) {
    auto constexpr kMaxSleep = std::chrono::seconds(3600);
    if (timers_.empty()) return Clock::now() + kMaxSleep;
    return timers_.begin()->first;
  }

  using PromiseType = promise<StatusOr<TimePoint>>;
  std::mutex mu_;
  std::condition_variable cv_;
  std::multimap<TimePoint, PromiseType> timers_;
  bool shutdown_ = false;
};

}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_TIMER_QUEUE_H
