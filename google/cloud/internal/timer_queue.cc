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
#include "google/cloud/internal/make_status.h"

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {
namespace {

StatusOr<std::chrono::system_clock::time_point> MakeCancelled(
    std::string msg, ErrorInfoBuilder b) {
  return CancelledError(std::move(msg), std::move(b));
}

}  // namespace

future<StatusOr<std::chrono::system_clock::time_point>> TimerQueue::Schedule(
    std::chrono::system_clock::time_point tp) {
  auto p = PromiseType();
  auto f = p.get_future();
  bool should_notify = false;
  {
    std::lock_guard<std::mutex> lk(mu_);
    if (shutdown_) {
      return make_ready_future(
          MakeCancelled("TimerQueue shutdown", GCP_ERROR_INFO()));
    }
    auto iter = timers_.emplace(tp, std::move(p));
    should_notify = iter == timers_.begin();
  }
  if (should_notify) cv_.notify_all();
  return f;
}

void TimerQueue::Shutdown() {
  std::unique_lock<std::mutex> const lk(mu_);
  shutdown_ = true;
  cv_.notify_one();
  cv_follower_.notify_all();
}

// The threads play two different roles:
// - A single thread at a time plays the leader role.
// - All other threads are followers and block on cv_follower_.
//
// Once a timer expires, the leader thread relinquishes its role and wakes up
// one follower thread to become the new leader. Only after doing so it
// runs the code to expire the timer.
//
// This is all complicated by shutdown. Basically all thread needs to wake
// up when the TQ is shutdown and one of them will expire all the timers.
void TimerQueue::Service() {
  // The is_leader flag allows us to restart this loop without worrying about
  // electing new leaders.
  auto is_leader = false;
  std::unique_lock<std::mutex> lk(mu_);
  while (!shutdown_) {
    if (!is_leader && has_leader_) {
      // The current thread becomes a follower while there is a leader.
      cv_follower_.wait(lk, [this] { return shutdown_ || !has_leader_; });
      continue;
    }
    is_leader = true;
    has_leader_ = true;
    if (timers_.empty()) {
      cv_.wait(lk, [this] { return shutdown_ || !timers_.empty(); });
      continue;
    }
    // Should a new timer appear that changes the "first" timer, we need to
    // wake up and recompute the sleep time. But note that the leader thread
    // does not need to relinquish its role to do so.
    auto until = timers_.begin()->first;
    auto predicate = [this, until] {
      return shutdown_ || timers_.empty() || timers_.begin()->first != until;
    };
    if (cv_.wait_until(lk, until, std::move(predicate))) {
      // Timers can be expired only if wait_util() returns due to a timeout.
      continue;
    }
    // If we get here we know that predicate() is false, which implies that
    // `timers_` is not empty and the first timer's key is `until`.
    auto p = std::move(timers_.begin()->second);
    timers_.erase(timers_.begin());
    // Relinquish the leader role, release the mutex and then signal a follower.
    has_leader_ = false;
    lk.unlock();
    is_leader = false;
    // Elect a new leader (if available) to continue expiring timers.
    cv_follower_.notify_one();
    // This may run user code (in the continuations for the future). That may
    // do all kinds of things, including calling back into this class to create
    // new timers. We cannot hold the mutex while it is running.
    p.set_value(until);
    lk.lock();
  }
  CancelAll(std::move(lk), "TimerQueue shutdown");
}

void TimerQueue::CancelAll() {
  CancelAll(std::unique_lock<std::mutex>(mu_), "TimerQueue cancel all");
}

void TimerQueue::CancelAll(std::unique_lock<std::mutex> lk, char const* msg) {
  while (!timers_.empty()) {
    auto p = std::move(timers_.begin()->second);
    timers_.erase(timers_.begin());
    lk.unlock();
    cv_.notify_one();
    p.set_value(MakeCancelled(msg, GCP_ERROR_INFO()));
    lk.lock();
  }
}

}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google
