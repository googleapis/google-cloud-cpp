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
 private:
  using FutureType = future<StatusOr<std::chrono::system_clock::time_point>>;

 public:
  static std::shared_ptr<TimerQueue> Create();

  TimerQueue(TimerQueue const&) = delete;
  TimerQueue& operator=(TimerQueue const&) = delete;
  TimerQueue(TimerQueue&&) = delete;
  TimerQueue& operator=(TimerQueue&&) = delete;

  /**
   * Adds a timer to the queue.
   *
   * The future returned by this function is satisfied when either:
   * - The timer expires, in which case the future is satisfied with a
   *   successful `StatusOr`. The value in the `StatusOr` will be @p tp.
   * - The timer queue is shutdown (or was shutdown), in which case the future
   *   is satisfied with an error status.
   */
  future<StatusOr<std::chrono::system_clock::time_point>> Schedule(
      std::chrono::system_clock::time_point tp);

  /**
   * Adds a timer to the queue and atomically attaches a callback to the timer.
   *
   * This creates a new timer that expires at @p tp, and atomically attaches
   * @p functor to be invoked when the timer expires. The functor must be able
   * to consume `future<StatusOr<std::chrono::system_clock::time_point>>`.
   *
   * Unless the timer queue is shutdown, the provided functor is always invoked
   * by one of the threads blocked in `Service()`.  In contrast, something like
   * `Schedule(tp).then(std::move(functor))` may result in the functor being
   * invoked by the thread calling `.Schedule()`, as the future may be already
   * satisfied when `.then()` is invoked.
   *
   * When the functor is called its future will be already satisfied. The
   * `StatusOr<>` value in may contain an error.  This can be used to detect
   * if the timer queue is shutdown.  For example, periodic timers may be
   * implemented as:
   *
   * @code {.cpp}
   * void StartPeriodicTimer(
   *     std::shared_ptr<TimerQueue> tq, std::chrono::milliseconds period,
   *     std::function<void()> action) {
   *   auto functor = [
   *           weak = std::weak_ptr<TimerQueue>(tq),
   *           action = std::move(action),
   *           period](
   *       future<StatusOr<std::chrono::system_clock::time_point>> f) mutable {
   *     auto tq = weak.lock();
   *     if (!tq) return;            // deleted timer queue
   *     if (!f.get().ok()) return;  // shutdown timer queue
   *     action();
   *     StartPeriodicTimer(std::move(tq), period, std::move(action));
   *   };
   *   future<void> unused = tq.Schedule(
   *     std::chrono::system_clock::now() + period, std::move(functor));
   * }
   * @endcode
   *
   * @returns a future wrapping the result of `functor(future<StatusOr<...>>)`.
   *     In other words, if the `functor` returns `T` this returns `future<T>`,
   *     unless `T == future<U>` in which case the function automatically
   *     unwraps the future and this returns `future<U>`.
   */
  template <typename Functor>
  auto Schedule(std::chrono::system_clock::time_point tp, Functor&& functor)
      -> decltype(std::declval<FutureType>().then(
          std::forward<Functor>(functor))) {
    auto weak = std::weak_ptr<TimerQueue>(shared_from_this());
    std::unique_lock<std::mutex> lk(mu_);
    if (shutdown_) {
      lk.unlock();
      return make_ready_future(MakeCancelled(__func__))
          .then(std::forward<Functor>(functor));
    }
    auto const key = MakeKey(tp);
    auto p = MakePromise(std::move(weak), key);
    auto f = p.get_future().then(std::forward<Functor>(functor));
    Insert(std::move(lk), key, std::move(p));
    return f;
  }

  /**
   * Schedule an immediately expiring timer and atomically run @p functor on it.
   *
   * See `Schedule(std::chrono::system_clock::time_point,Functor)` for details.
   */
  template <typename Functor>
  auto Schedule(Functor&& functor)
      -> decltype(std::declval<TimerQueue>().Schedule(
          std::chrono::system_clock::time_point{},
          std::forward<Functor>(functor))) {
    return Schedule(std::chrono::system_clock::time_point::min(),
                    std::forward<Functor>(functor));
  }

  /**
   * Signals all threads that have called Service() to return.
   *
   * Once this function returns no more timers can be scheduled successfully.
   * All calls to `Schedule()` will return an immediately satisfied timer with
   * a `StatusCode::kCancelled` status.
   *
   * While all outstanding timers are cancelled, applications should not assume
   * any particular ordering. Timers that are close to their expiration may
   * complete successfully even after `Shutdown()` returns.
   */
  void Shutdown();

  /**
   * Blocks the current thread to service the timer queue.
   *
   * The thread calling `Service()` blocks until `Shutdown()` is called. While
   * blocked in the `Service()` call, the thread is used to expire timers.
   *
   * Any continuations for timers that complete successfully (i.e. are satisfied
   * with a `std::chrono::system_clock::time_point`) run in one of the threads
   * that have called `Service()`.
   */
  void Service();

 private:
  using PromiseType = promise<StatusOr<std::chrono::system_clock::time_point>>;
  using KeyType =
      std::pair<std::chrono::system_clock::time_point, std::uint64_t>;

  TimerQueue() = default;

  // Cancels a timer.
  void Cancel(KeyType key);

  /**
   * Helper function to satisfy futures and promises on shutdown.
   *
   * Creates a value suitable to satisfy the future / promises created by this
   * class.
   *
   * @param where the name of the calling function
   * @returns  a `StatusOr<>` holding a `kCancelled` status.
   */
  static StatusOr<std::chrono::system_clock::time_point> MakeCancelled(
      char const* where);

  KeyType MakeKey(std::chrono::system_clock::time_point tp) {
    return std::make_pair(tp, ++id_generator_);
  }

  static PromiseType MakePromise(std::weak_ptr<TimerQueue> w, KeyType key) {
    return PromiseType([weak = std::move(w), key]() {
      if (auto self = weak.lock()) self->Cancel(key);
    });
  }

  void Insert(std::unique_lock<std::mutex> lk, KeyType const& key,
              PromiseType p);

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
