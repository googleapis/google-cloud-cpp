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

class TimerQueue {
 public:
  TimerQueue() = default;
  TimerQueue(TimerQueue const&) = delete;
  TimerQueue& operator=(TimerQueue const&) = delete;

  // Adds a timer to the queue. If the `Shutdown` has already been called,
  // calls to `Schedule` return immediately with a `StatusCode::kCancelled`.
  future<StatusOr<std::chrono::system_clock::time_point>> Schedule(
      std::chrono::system_clock::time_point tp);

  // Signals all threads that have called `Service` to return. Additionally, all
  // outstanding timers are cancelled.
  void Shutdown();

  // Timers added via `Schedule` should be managed by one or more threads that
  // call `Service`. Calls to `Service` only return after `Shutdown` has been
  // called.
  void Service();

  // Cancels all timers.
  void CancelAll();

 private:
  // Requires a lock to be held before calling.
  bool HasExpiredTimer(std::unique_lock<std::mutex> const&,
                       std::chrono::system_clock::time_point tp);
  void ExpireTimer(std::unique_lock<std::mutex> lk,
                   std::chrono::system_clock::time_point tp);
  void NotifyShutdown(std::unique_lock<std::mutex> lk);
  // Requires a lock to be held before calling.
  std::chrono::system_clock::time_point NextExpiration(
      std::unique_lock<std::mutex> const&);

  using PromiseType = promise<StatusOr<std::chrono::system_clock::time_point>>;
  std::mutex mu_;
  std::condition_variable cv_;
  std::multimap<std::chrono::system_clock::time_point, PromiseType> timers_;
  bool shutdown_ = false;
};

}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_TIMER_QUEUE_H
