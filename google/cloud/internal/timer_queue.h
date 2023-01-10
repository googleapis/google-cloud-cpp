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
#include <condition_variable>
#include <map>
#include <memory>
#include <mutex>

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {

/**
 * A timer queue supporting multiple servicing threads.
 *
 * @par Example
 *
 * First create a pool of threads to service the TimerQueue:
 * @code
 * TimerQueue tq;
 * std::vector<std::thread> svc(8);
 * std::generate(svc.begin(), svc.end(),
 *               std::thread{[&] { return tq.Service(); }});
 * @endcode
 *
 * The thread pool can be as small as one thread. You can schedule timers using
 * the `Schedule()` function:
 * @code
 * using std::chrono_literals;
 * auto const now = std::chrono::system_clock::now();
 * tq.Schedule(now + 100ms).then([](auto f) { std::out << "timer 1\n"; });
 * tq.Schedule(now + 200ms).then([](auto f) { std::out << "timer 2\n"; });
 * tq.Schedule(now + 200ms).then([](auto f) { std::out << "timer 3\n"; });
 * @code
 *
 * To shutdown the timer queue you need to call `Shutdown()`:
 * @code
 * tq.Shutdown();
 * @endcode
 *
 * Don't forget to join your thread pool.  Remember that these threads will not
 * terminate until `Shutdown()` is called:
 * @code
 * for (auto& t : svc) t.join();
 * @endcode
 */
class TimerQueue : public std::enable_shared_from_this<TimerQueue> {
 public:
  static std::shared_ptr<TimerQueue> Create();

  TimerQueue(TimerQueue const&) = delete;
  TimerQueue& operator=(TimerQueue const&) = delete;
  TimerQueue(TimerQueue&&) = delete;
  TimerQueue& operator=(TimerQueue&&) = delete;

  // Adds a timer to the queue.
  future<StatusOr<std::chrono::system_clock::time_point>> Schedule(
      std::chrono::system_clock::time_point tp);

  // Signals all threads that have called Service to return. Does not modify
  // remaining timers. If desired, CancelAll can be called.
  void Shutdown();

  // Timers added via Schedule should be managed by one or more threads that
  // call Service. Calls to Service only return after Shutdown has been called.
  void Service();

 private:
  using PromiseType = promise<StatusOr<std::chrono::system_clock::time_point>>;
  using KeyType = std::pair<std::chrono::system_clock::time_point, std::uint64_t>;

  TimerQueue() = default;

  // Cancels an specific timer
  void Cancel(KeyType key);

  std::mutex mu_;
  std::condition_variable cv_;
  std::condition_variable cv_follower_;
  std::multimap<KeyType, PromiseType> timers_;
  std::uint64_t id_generator_ = 0;
  bool shutdown_ = false;
  bool has_leader_ = false;
};

}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_TIMER_QUEUE_H
