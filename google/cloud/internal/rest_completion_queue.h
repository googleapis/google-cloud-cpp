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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_REST_COMPLETION_QUEUE_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_REST_COMPLETION_QUEUE_H

#include "google/cloud/future.h"
#include "google/cloud/status_or.h"
#include "google/cloud/version.h"
#include "absl/memory/memory.h"
#include <chrono>
#include <deque>
#include <map>
#include <mutex>

namespace google {
namespace cloud {
namespace rest_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

// This class is a placeholder that exists in the google_cloud_cpp_rest_internal
// library. This is an important distinction as the other completion queue
// components reside in libraries that are dependent on protobuf and grpc. In
// the future, this class would leverage one or more curl_multi_handles in order
// to handle executing multiple HTTP requests on a single thread, and could be
// used with libraries that actively avoid having dependencies on protobuf or
// grpc.
class RestCompletionQueue {
 public:
  enum class QueueStatus {
    kShutdown,
    kGotEvent,
    kTimeout,
  };

  // Prevents further retrieval from or mutation of the RestCompletionQueue.
  void Shutdown();

  void CancelAll();

  future<StatusOr<std::chrono::system_clock::time_point>> ScheduleTimer(
      std::chrono::system_clock::time_point tp);

  void Service();

  std::int64_t timer_counter() const { return tq_.timer_counter(); }
#if 0
  // Attempts to get the next tag from the queue before the deadline is reached.
  // If a tag is retrieved, tag is set to the retrieved value, ok is set true,
  //   and kGotEvent is returned.
  // If the deadline is reached and no tag is available, kTimeout is returned.
  // If Shutdown has been called, tag is set to nullptr, ok is set to false,
  //    and kShutdown is returned.
  QueueStatus GetNext(void** tag, bool* ok,
                      std::chrono::system_clock::time_point deadline);
  void AddTag(void* tag);
  void RemoveTag(void* tag);
  std::size_t size() const;
#endif

 private:
  /// This class is an implementation detail to manage multiple timers on a
  /// single thread.
  template <typename Clock>
  class TimerQueue {
   public:
    using TimePoint = typename Clock::time_point;
    using FutureType = StatusOr<TimePoint>;

    TimerQueue() = default;
    TimerQueue(TimerQueue const&) = delete;
    TimerQueue& operator=(TimerQueue const&) = delete;

    future<FutureType> Schedule(TimePoint tp) {
      std::cout << __func__ << " thread=" << std::this_thread::get_id()
                << std::endl;
      auto p = PromiseType();
      auto f = p.get_future();
      {
        std::lock_guard<std::mutex> lk(mu_);
        if (shutdown_)
          return make_ready_future(FutureType(
              Status(StatusCode::kCancelled, "queue already shutdown")));
        (void)timers_.emplace(tp, std::move(p));
      }
      cv_.notify_all();
      return f;
    }

    void Shutdown() {
      std::unique_lock<std::mutex> const lk(mu_);
      shutdown_ = true;
      cv_.notify_all();
    }

    void Service() {
      while (true) {
        std::unique_lock<std::mutex> lk(mu_);
        auto const ne = NextExpiration();
        cv_.wait_until(
            lk, ne, [this, ne] { return shutdown_ || ne >= NextExpiration(); });
        if (shutdown_) return NotifyShutdown(std::move(lk));
        ExpireTimers(std::move(lk), Clock::now());
      }
    }

    void CancelAll() {
      std::multimap<TimePoint, PromiseType> timers;
      {
        std::lock_guard<std::mutex> lk(mu_);
        timers = std::move(timers_);
      }
      for (auto& kv : timers) {
        kv.second.set_value(Status(StatusCode::kCancelled, "Timer cancelled."));
      }
    }

    std::int64_t timer_counter() const { return timer_counter_.load(); }

   private:
    void ExpireTimers(std::unique_lock<std::mutex> lk, TimePoint tp) {
      std::cout << __func__ << " thread=" << std::this_thread::get_id()
                << std::endl;
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
      ++timer_counter_;
      //      std::cout << __func__ << "thread=" << std::this_thread::get_id()
      //                << " timer_counter_=" << timer_counter_ << std::endl;
    }

    void NotifyShutdown(std::unique_lock<std::mutex> lk) {
      auto timers = std::move(timers_);
      lk.unlock();
      for (auto& kv : timers) {
        kv.second.set_value(
            Status(StatusCode::kCancelled, "Timer queue shutdown."));
      }
    }

    TimePoint NextExpiration() {
      auto constexpr kMaxSleep = std::chrono::hours(1);
      if (timers_.empty()) return Clock::now() + kMaxSleep;
      return timers_.begin()->first;
    }

    using PromiseType = promise<StatusOr<TimePoint>>;
    std::mutex mu_;
    std::condition_variable cv_;
    std::atomic<std::int64_t> timer_counter_{0};
    std::multimap<TimePoint, PromiseType> timers_;
    bool shutdown_ = false;
  };

  mutable std::mutex mu_;
  bool shutdown_{false};  // GUARDED_BY(mu_)
  //  std::deque<void*> pending_tags_;  // GUARDED_BY(mu_)
  TimerQueue<std::chrono::system_clock> tq_;
};

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace rest_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_REST_COMPLETION_QUEUE_H
