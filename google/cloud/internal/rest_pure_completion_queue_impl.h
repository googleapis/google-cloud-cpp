// Copyright 2026 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_REST_PURE_COMPLETION_QUEUE_IMPL_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_REST_PURE_COMPLETION_QUEUE_IMPL_H

#include "google/cloud/future.h"
#include "google/cloud/internal/run_async_base.h"
#include "google/cloud/internal/timer_queue.h"
#include "google/cloud/log.h"
#include "google/cloud/status_or.h"
#include "google/cloud/version.h"
#include <atomic>
#include <chrono>
#include <memory>

namespace google {
namespace cloud {
namespace rest_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

/**
 * Interface for CompletionQueue that does NOT use a grpc::CompletionQueue.
 *
 * Due to the lack of a completion queue that can manage multiple, simultaneous
 * REST requests, asynchronous calls should be launched on a thread of their own
 * and RunAsync should only be called with a function to join that thread after
 * it completes its work.
 *
 * This class differs from RestCompletionQueueImpl as it does not contain a
 * method that returns a `grpc::CompletionQueue*` nor a `StartOperation` method
 * as that method takes an `internal::AsyncGrpcOperation` as a parameter.
 */
class RestPureCompletionQueueInterface {
 public:
  virtual ~RestPureCompletionQueueInterface() = default;

  /// Run the event loop until Shutdown() is called.
  virtual void Run() = 0;

  /// Terminate the event loop.
  virtual void Shutdown() = 0;

  /// Cancel all existing operations.
  virtual void CancelAll() = 0;

  /// Create a new timer.
  virtual future<StatusOr<std::chrono::system_clock::time_point>>
  MakeDeadlineTimer(std::chrono::system_clock::time_point deadline) = 0;

  /// Create a new timer.
  virtual future<StatusOr<std::chrono::system_clock::time_point>>
  MakeRelativeTimer(std::chrono::nanoseconds duration) = 0;

  /// Enqueue a new asynchronous function.
  virtual void RunAsync(std::unique_ptr<internal::RunAsyncBase> function) = 0;
};

class RestPureCompletionQueue;

std::shared_ptr<RestPureCompletionQueueInterface>
GetRestPureCompletionQueueImpl(RestPureCompletionQueue const& cq);
std::shared_ptr<RestPureCompletionQueueInterface>
GetRestPureCompletionQueueImpl(RestPureCompletionQueue&& cq);

template <typename Functor>
using CheckRunAsyncCallback =
    google::cloud::internal::is_invocable<Functor, RestPureCompletionQueue&>;

/// A pure REST version of `google::cloud::CompletionQueue` that wraps an
/// implementation of `RestPureCompletionQueueInterface`.
class RestPureCompletionQueue {
 public:
  RestPureCompletionQueue();
  explicit RestPureCompletionQueue(
      std::shared_ptr<RestPureCompletionQueueInterface> impl)
      : impl_(std::move(impl)) {}

  /**
   * Run the completion queue event loop.
   *
   * Note that more than one thread can call this member function, to create a
   * pool of threads completing asynchronous operations.
   */
  void Run() { impl_->Run(); }

  /// Terminate the completion queue event loop.
  void Shutdown() { impl_->Shutdown(); }

  /// Cancel all pending operations.
  void CancelAll() { impl_->CancelAll(); }

  /**
   * Create a timer that fires at @p deadline.
   *
   * @param deadline when should the timer expire.
   *
   * @return a future that becomes satisfied after @p deadline.
   *     The result of the future is the time at which it expired, or an error
   *     Status if the timer did not run to expiration (e.g. it was cancelled).
   */
  google::cloud::future<StatusOr<std::chrono::system_clock::time_point>>
  MakeDeadlineTimer(std::chrono::system_clock::time_point deadline) {
    return impl_->MakeDeadlineTimer(deadline);
  }

  /**
   * Create a timer that fires after the @p duration.
   *
   * @tparam Rep a placeholder to match the Rep tparam for @p duration type,
   *     the semantics of this template parameter are documented in
   *     `std::chrono::duration<>` (in brief, the underlying arithmetic type
   *     used to store the number of ticks), for our purposes it is simply a
   *     formal parameter.
   * @tparam Period a placeholder to match the Period tparam for @p duration
   *     type, the semantics of this template parameter are documented in
   *     `std::chrono::duration<>` (in brief, the length of the tick in seconds,
   *     expressed as a `std::ratio<>`), for our purposes it is simply a formal
   *     parameter.
   *
   * @param duration when should the timer expire relative to the current time.
   *
   * @return a future that becomes satisfied after @p duration time has elapsed.
   *     The result of the future is the time at which it expired, or an error
   *     Status if the timer did not run to expiration (e.g. it was cancelled).
   */
  template <typename Rep, typename Period>
  future<StatusOr<std::chrono::system_clock::time_point>> MakeRelativeTimer(
      std::chrono::duration<Rep, Period> duration) {
    return impl_->MakeRelativeTimer(
        std::chrono::duration_cast<std::chrono::nanoseconds>(duration));
  }

  /**
   * Asynchronously run a functor on a thread `Run()`ning the `CompletionQueue`.
   *
   * @param functor the functor to invoke in one of the CompletionQueue's
   *     threads.
   *
   * @tparam Functor the type of @p functor. It must satisfy
   *     `std::is_invocable<Functor, #CompletionQueue&>`
   */
  template <typename Functor,
            /// @cond implementation_details
            std::enable_if_t<CheckRunAsyncCallback<Functor>::value, int> = 0
            /// @endcond
            >
  void RunAsync(Functor&& functor) {
    class Wrapper : public internal::RunAsyncBase {
     public:
      Wrapper(std::weak_ptr<RestPureCompletionQueueInterface> impl, Functor&& f)
          : impl_(std::move(impl)), fun_(std::forward<Functor>(f)) {}
      ~Wrapper() override = default;
      void exec() override {
        auto impl = impl_.lock();
        if (!impl) return;
        RestPureCompletionQueue cq(std::move(impl));
        fun_(cq);
      }

     private:
      std::weak_ptr<RestPureCompletionQueueInterface> impl_;
      std::decay_t<Functor> fun_;
    };
    impl_->RunAsync(
        std::make_unique<Wrapper>(impl_, std::forward<Functor>(functor)));
  }

  /**
   * Asynchronously run a functor on a thread `Run()`ning the `CompletionQueue`.
   *
   * @param functor the functor to call in one of the CompletionQueue's threads.
   * @tparam Functor the type of @p functor. It must satisfy
   *     `std::is_invocable<Functor>`.
   */
  template <typename Functor,
            /// @cond implementation_details
            std::enable_if_t<internal::is_invocable<Functor>::value, int> = 0
            /// @endcond
            >
  void RunAsync(Functor&& functor) {
    class Wrapper : public internal::RunAsyncBase {
     public:
      explicit Wrapper(Functor&& f) : fun_(std::forward<Functor>(f)) {}
      ~Wrapper() override = default;
      void exec() override { fun_(); }

     private:
      std::decay_t<Functor> fun_;
    };
    impl_->RunAsync(std::make_unique<Wrapper>(std::forward<Functor>(functor)));
  }

 private:
  friend std::shared_ptr<RestPureCompletionQueueInterface>
  GetRestPureCompletionQueueImpl(RestPureCompletionQueue const& cq);
  friend std::shared_ptr<RestPureCompletionQueueInterface>
  GetRestPureCompletionQueueImpl(RestPureCompletionQueue&& cq);
  std::shared_ptr<RestPureCompletionQueueInterface> impl_;
};

inline std::shared_ptr<RestPureCompletionQueueInterface>
GetRestPureCompletionQueueImpl(RestPureCompletionQueue const& cq) {
  return cq.impl_;
}

inline std::shared_ptr<RestPureCompletionQueueInterface>
GetRestPureCompletionQueueImpl(RestPureCompletionQueue&& cq) {
  return std::move(cq.impl_);
}

/// Default implementation of `RestPureCompletionQueueInterface` that uses a
/// `TimerQueue` under the hood.
class RestPureCompletionQueueImpl final
    : public RestPureCompletionQueueInterface,
      public std::enable_shared_from_this<RestPureCompletionQueueInterface> {
 public:
  ~RestPureCompletionQueueImpl() override = default;
  RestPureCompletionQueueImpl();

  /// Run the event loop until Shutdown() is called.
  void Run() override;

  /// Terminate the event loop.
  void Shutdown() override;

  /// Cancel all existing operations.
  void CancelAll() override;

  /// Create a new timer.
  future<StatusOr<std::chrono::system_clock::time_point>> MakeDeadlineTimer(
      std::chrono::system_clock::time_point deadline) override;

  /// Create a new timer.
  future<StatusOr<std::chrono::system_clock::time_point>> MakeRelativeTimer(
      std::chrono::nanoseconds duration) override;

  /// Enqueue a new asynchronous function.
  void RunAsync(std::unique_ptr<internal::RunAsyncBase> function) override;

  /// Some counters for testing and debugging.
  std::int64_t run_async_counter() const { return run_async_counter_.load(); }

 private:
  std::shared_ptr<internal::TimerQueue> tq_;
  // These are metrics used in testing.
  std::atomic<std::int64_t> run_async_counter_{0};
};

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace rest_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_REST_PURE_COMPLETION_QUEUE_IMPL_H
