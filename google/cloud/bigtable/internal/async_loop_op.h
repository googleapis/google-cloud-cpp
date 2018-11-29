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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INTERNAL_ASYNC_LOOP_OP_H_
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INTERNAL_ASYNC_LOOP_OP_H_

#include "google/cloud/bigtable/completion_queue.h"
#include "google/cloud/bigtable/internal/async_op_traits.h"
#include "google/cloud/internal/make_unique.h"
#include "google/cloud/internal/throw_delegate.h"

namespace google {
namespace cloud {
namespace bigtable {
inline namespace BIGTABLE_CLIENT_NS {
namespace internal {

/**
 * A dummy function object only to ease specification of loopable operations.
 *
 * It is an example type which could be passed to `Start()` member function of
 * the operation to be executed in a loop.
 */
struct PrototypeLoopOperationStartCallback {
  void operator()(CompletionQueue&, bool finished) const {}
};

template <typename Operation>
struct MeetsLoopOperationRequirements {
  // Looks like MSVC 15.7.3 has a bug and evaluates the Has.* detectors to
  // false. 15.9.2 seems to have worked, but I'm not adding specific version
  // conditionals to avoid future surprises.
#ifndef _MSC_VER
  static_assert(
      HasStart<Operation, PrototypeLoopOperationStartCallback>::value,
      "Operation has to have a templated Start() member function "
      "instantiatable with functors like PrototypeLoopOperationStartCallback");
  static_assert(HasCancel<Operation>::value,
                "Operation has to have an Cancel() member function.");
  static_assert(HasWaitPeriod<Operation>::value,
                "Operation has to have an WaitPeriod() member function.");
#endif
  static_assert(
      google::cloud::internal::is_invocable<
          decltype(
              &Operation::template Start<PrototypeLoopOperationStartCallback>),
          Operation&, CompletionQueue&,
          PrototypeLoopOperationStartCallback&&>::value,
      "Operation::Start<PrototypeLoopOperationStartCallback> has to be "
      "non-static and invocable with CompletionQueue&, "
      "PrototypeLoopOperationStartCallback&&.");
  static_assert(
      google::cloud::internal::is_invocable<
          decltype(&Operation::Cancel), Operation&, CompletionQueue&>::value,
      "Operation::Cancel has to be non-static and invokable with "
      "CompletionQueue&");
  static_assert(google::cloud::internal::is_invocable<
                    decltype(&Operation::WaitPeriod), Operation&>::value,
                "Operation::WaitPeriod() has to be non-static and "
                "invokable with no arguments.&");
  static_assert(std::is_same<google::cloud::internal::invoke_result_t<
                                 decltype(&Operation::template Start<
                                          PrototypeLoopOperationStartCallback>),
                                 Operation&, CompletionQueue&,
                                 PrototypeLoopOperationStartCallback&&>,
                             std::shared_ptr<AsyncOperation>>::value,
                "Operation::Start<>(...) has to return a "
                "std::shared_ptr<AsyncOperation>");
  static_assert(std::is_same<google::cloud::internal::invoke_result_t<
                                 decltype(&Operation::WaitPeriod), Operation&>,
                             std::chrono::milliseconds>::value,
                "Operation::WaitPeriod() has to return "
                "std::chrono::milliseconds");
};

/**
 * Loop an asynchronous operation while allowing for cancellation.
 *
 * Conceptually, this class implements an asynchronous counterpart of the
 * following logic:
 * ```
 * for(;;) {
 *   bool const finished = op.Start();
 *   if (finished) {
 *     break;
 *   }
 *   if (* someone cancelled the loop *) {
 *     op.Cancel();
 *     break;
 *   }
 *   sleep(op.GetDelay());
 *   if (* someone cancelled the loop *) {
 *     op.Cancel();
 *     break;
 *   }
 * }
 * ```
 * It is used for implementing `AsyncRetryOp` (retrying an asynchronous
 * operation), `AsyncPollOp` (asynchronously polling) and alike.
 *
 * The `Operation` is responsible for firing the user callback if it completes.
 *
 * The operation should implement 3 member functions (has to meet
 * `MeetsLoopOperationRequirements`):
 *  * Start() - starts a new attempt and indicates in the provided callback
 *      whether the whole operation is finished (i.e. whether we should break
 *      the loop).
 *  * WaitPeriod() - indicates for how long we should pause the loop.
 *  * Cancel() - this should immediately abort and call the user-provided
 *      callback.
 *
 *  The `Operation` object is guaranteed to not be destroyed until it fires the
 *  callback provided to it via `Start()` with finished==true or until
 *  `Cancel()` function is called on it.
 *
 *  `Operation` doesn't need to be thread-safe, `AsyncLoopOp` guarantees serial
 *  accesses. `AsyncLoopOp` also guarantees that neither
 */
template <typename Operation>
class AsyncLoopOp : public std::enable_shared_from_this<AsyncLoopOp<Operation>>,
                    public AsyncOperation {
 public:
  explicit AsyncLoopOp(Operation&& operation)
      : cancelled_(), operation_(std::move(operation)) {}

  void Cancel() override {
    std::lock_guard<std::mutex> lk(mu_);
    cancelled_ = true;
    if (current_op_) {
      current_op_->Cancel();
      current_op_.reset();
    }
  }

  std::shared_ptr<AsyncOperation> Start(CompletionQueue& cq) {
    auto res =
        std::static_pointer_cast<AsyncOperation>(this->shared_from_this());
    std::unique_lock<std::mutex> lk(mu_);
    if (cancelled_) {
      lk.unlock();
      // We could fire the callback right here, but we'd be risking a deadlock
      // if the user held a lock while submitting this request. Instead, let's
      // schedule the callback to fire on the thread running the completion
      // queue by submitting an expired timer.
      // There is no reason to store this timer in current_op_.
      auto self = this->shared_from_this();
      cq.RunAsync([self](CompletionQueue& cq) { self->OnTimer(cq, false); });
      return res;
    }
    StartUnlocked(cq);
    return res;
  }

 private:
  /**
   * Kick off the asynchronous request.
   *
   * @param cq the completion queue to run the asynchronous operations.
   */
  void StartUnlocked(CompletionQueue& cq) {
    auto self = this->shared_from_this();
    current_op_ =
        operation_.Start(cq, [self](CompletionQueue& cq, bool finished) {
          self->OnCompletion(cq, finished);
        });
  }

  /// The callback to handle one asynchronous request completing.
  void OnCompletion(CompletionQueue& cq, bool finished) {
    std::unique_lock<std::mutex> lk(mu_);
    // If we don't schedule a timer, we don't want this object to
    // hold the operation.
    current_op_.reset();
    if (finished) {
      // The operation signalled that it's finished - it must have already done
      // fired the callback.
      return;
    }
    if (cancelled_) {
      // The operation didn't notice the cancellation, let's make it clear.
      lk.unlock();
      operation_.Cancel(cq);
      return;
    }
    auto delay = operation_.WaitPeriod();
    if (delay == std::chrono::milliseconds(0)) {
      StartUnlocked(cq);
      return;
    }
    auto self = this->shared_from_this();
    current_op_ = cq.MakeRelativeTimer(
        delay, [self](CompletionQueue& cq, AsyncTimerResult result) {
          self->OnTimer(cq, result.cancelled);
        });
    return;
  }

  /// The callback to handle the timer completing.
  void OnTimer(CompletionQueue& cq, bool cancelled) {
    std::unique_lock<std::mutex> lk(mu_);
    current_op_.reset();
    if (cancelled or cancelled_) {
      // Cancelled, no more action to take.
      // The operation couldn't have noticed this cancellation, because it came
      // while we were waiting.
      lk.unlock();
      operation_.Cancel(cq);
      return;
    }
    StartUnlocked(cq);
  }

  MeetsLoopOperationRequirements<Operation> requirements_check_;
  std::mutex mu_;
  // Because of the racy nature of cancellation, a cancelled timer or operation
  // might occasionally return a non-cancelled status (e.g. when cancellation
  // occurs right before firing the callback). In order to not schedule a next
  // retry in such a scenario, we indicate cancellation by using this flag.
  bool cancelled_;
  // A handle to a currently ongoing async operation - either a timer or one
  // created through `Operation::Start`.
  std::shared_ptr<AsyncOperation> current_op_;
  Operation operation_;
};

}  // namespace internal
}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INTERNAL_ASYNC_LOOP_OP_H_
