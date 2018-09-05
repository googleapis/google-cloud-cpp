// Copyright 2018 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_COMPLETION_QUEUE_H_
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_COMPLETION_QUEUE_H_

#include "google/cloud/bigtable/internal/completion_queue_impl.h"

namespace google {
namespace cloud {
namespace bigtable {
inline namespace BIGTABLE_CLIENT_NS {
/**
 * Call the functor associated with asynchronous operations when they complete.
 */
class CompletionQueue {
 public:
  CompletionQueue();
  explicit CompletionQueue(std::shared_ptr<internal::CompletionQueueImpl> impl)
      : impl_(std::move(impl)) {}

  /**
   * Run the completion queue event loop.
   *
   * Note that more than one thread can call this member function, to create a
   * pool of threads completing asynchronous operations.
   */
  void Run();

  /// Terminate the completion queue event loop.
  void Shutdown();

  /**
   * Create a timer that fires at @p deadline.
   *
   * @tparam Functor the functor to call when the timer expires and/or it is
   *   canceled.  It must satisfy the `void(AsyncOperation&,bool)` signature.
   * @param deadline when should the timer expire.
   * @param functor the value of the functor.
   * @return an asynchronous operation wrapping the functor and timer, can be
   *   used to cancel the pending timer.
   */
  template <typename Functor>
  std::shared_ptr<AsyncOperation> MakeDeadlineTimer(
      std::chrono::system_clock::time_point deadline, Functor&& functor) {
    auto op = std::make_shared<internal::AsyncTimerFunctor<Functor>>(
        std::forward<Functor>(functor));
    void* tag = impl_->RegisterOperation(op);
    op->Set(impl_->cq(), deadline, tag);
    return op;
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
   * @tparam Functor the functor to call when the timer expires and/or it is
   *   canceled.  It must satisfy the `void(AsyncOperation&,bool)` signature.
   * @param duration when should the timer expire relative to the current time.
   * @param functor the value of the functor.
   * @return an asynchronous operation wrapping the functor and timer, can be
   *   used to cancel the pending timer.
   */
  template <typename Rep, typename Period, typename Functor>
  std::shared_ptr<AsyncOperation> MakeRelativeTimer(
      std::chrono::duration<Rep, Period> duration, Functor&& functor) {
    auto deadline = std::chrono::system_clock::now() + duration;
    return MakeDeadlineTimer(deadline, std::forward<Functor>(functor));
  }

 private:
  std::shared_ptr<internal::CompletionQueueImpl> impl_;
};

}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_COMPLETION_QUEUE_H_
