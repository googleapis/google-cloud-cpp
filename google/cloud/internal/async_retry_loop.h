// Copyright 2020 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_ASYNC_RETRY_LOOP_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_ASYNC_RETRY_LOOP_H

#include "google/cloud/backoff_policy.h"
#include "google/cloud/completion_queue.h"
#include "google/cloud/future.h"
#include "google/cloud/grpc_options.h"
#include "google/cloud/internal/invoke_result.h"
#include "google/cloud/internal/retry_loop_helpers.h"
#include "google/cloud/internal/retry_policy.h"
#include "google/cloud/internal/setup_context.h"
#include "google/cloud/version.h"
#include "absl/meta/type_traits.h"
#include <grpcpp/grpcpp.h>
#include <chrono>

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {

template <typename T>
struct FutureValueType;

template <typename T>
struct FutureValueType<future<T>> {
  using value_type = T;
};

/**
 * Implement an asynchronous retry loop for wrapped gRPC requests.
 *
 * In newer libraries the stubs wrap asynchronous RPCs to match this signature:
 *
 * @code
 * class MyStub { public:
 *   virtual future<StatusOr<ResponseProto>> AsyncRpcName(
 *      google::cloud::CompletionQueue& cq,
 *      std::unique_ptr<grpc::ClientContext> context,
 *      RequestProto const& request) = 0;
 * };
 * @endcode
 *
 * stubs with such a signature are easier to mock and test. In most mocks all
 * we need to do is return a future satisfied immediately. And writing the
 * implementation of these stubs is very easy too.
 *
 * This class implements the retry loop for such an RPC.
 *
 * @par Cancellation and Thread Safety
 *
 * This class supports cancelling a retry loop (best-effort as most cancels
 * with gRPC). Without cancels this class would require no synchronization, as
 * each request, backoff timer, and their callbacks can only occur in sequence.
 * Cancel requests, however, may be invoked by any thread and require some form
 * of synchronization. The basic idea is that the class maintains a future
 * (called `pending_operation_`) that represents the current pending operation,
 * i.e., the current gRPC request or backoff timer. Cancelling the loop may
 * require cancelling the pending operation.  Using such a `pending_operation_`
 * future is prone to subtle race conditions. This section explains how we
 * ensure safety.
 *
 * @par The Race Condition
 *
 * The most common approach to solve race conditions is to use some kind of
 * lock, unfortunately this does not work in this case. Consider this code:
 *
 * @code
 * 1: void AsyncRetryLoopImpl::F() {
 * 2:  auto self = shared_from_this();
 * 3:  future<...> f = create_future();
 * 4:  std::unique_lock lk(mu_);
 * 5:  if (cancelled_) return HandleCancel();
 * 6:  auto pending = f.then([self](auto g) { self->Callback(); }
 * 7:  this->pending_operation_ = pending;
 * 8: }
 * 9:
 * 10: void AsyncRetryLoopImpl::Cancel() {
 * ....
 * 11:   this->pending_operation_.cancel();
 * ....
 * }
 * 12:
 * 13: void AsyncRetryLoopImpl::Callback() {
 * ....
 * 14:   F();
 * ...
 * }
 * @endcode
 *
 * Because futures are immediately active, setting up the callback on line (6)
 * can result in an immediate call to `Callback()`.  Since the mutex is held in
 * line (4) that can result in a deadlock as the callback may invoke `F()`
 * in line (14) which would want to acquire the mutex.
 *
 * One could solve this deadlock using a recursive mutex, but that still leaves
 * a second problem: as the stack unwinds the `pending_operation_` member
 * variable is set, on line (7), to the **first** operation, and we want it to
 * remain set to the **last** operation.
 *
 * @par Solution
 *
 * We keep a counter reflecting the number of operations performed by the retry
 * loop. This counter is incremented before starting a request and before
 * starting a backoff timer.
 *
 * The `pending_operation_` variable is updated **only** if the current
 * operation matches the operation counter. This means the `pending_operation_`
 * variable always reflects the last pending operation, it can never be set to
 * an older operation.
 *
 * @par Observations
 *
 * - The initial value of `cancelled_` is false.
 * - `Cancel()` is the only operation that changes `cancelled_`, and it holds
 *   the `mu_` mutex while doing so.
 * - Once `cancelled_` is set to `true` it is never set to `false`
 *
 * While `cancelled_` is false the loop is (basically) single threaded:
 *   - Each gRPC request or backoff timer is sequenced-after a call to
 *     `StartOperation()`, see `StartAttempt()` and `StartBackoff()`.
 *   - Each gRPC request or backoff timer must complete before the next one
 *     starts, as it is their callbacks (`OnAttempt()` and `OnBackoff()`) that
 *     start the next step.
 *   - `StartOperation()` is always sequenced-before calls to `SetPending()`.
 *   - `SetPending()` never sets `pending_operation_` to the `future<void>`
 *     representing an operation that has already completed.
 *
 * As to when the `cancelled_` flag changes to `true`:
 *   - `StartOperation()` and `SetPending()` both lock the same mutex `mu_` as
 *     `Cancel()`.
 *   - It follows that if `Cancel()` is invoked, then the `true` value will be
 *     visible to any future calls to `StartOperation()` or `SetPending()`.
 *   - If the next call is `StartOperation()` then no new operation is issued,
 *     as both `StartAttempt()` and `StartBackoff()` return immediately in this
 *     case.
 *   - Note that if `cancelled_` is true, `StartOperation()` terminates the
 *     retry loop by calling `SetDoneWithCancel()`.
 *   - If the next call is `SetPending()` the pending operation is immediately
 *     cancelled.
 *
 * @par Safety and Progress
 *
 * While the `cancelled_` flag is `false` the loop remains "safe" because it is
 * fundamentally a sequence of calls without concurrency or parallelism.
 * Multiple threads may be involved (i.e., each callback can happen in a
 * different thread) but the completion queue provides enough synchronization.
 *
 * Once the `cancelled_` flag is set to `true` the new valuable will become
 * visible to the threads running the `StartAttempt()` and/or `StartBackoff()`
 * functions.  If the value is visible, the retry loop will stop on the next
 * callback and/or before the next request or timer is issued.
 */
template <typename Functor, typename Request, typename RetryPolicyType>
class AsyncRetryLoopImpl
    : public std::enable_shared_from_this<
          AsyncRetryLoopImpl<Functor, Request, RetryPolicyType>> {
 public:
  AsyncRetryLoopImpl(std::unique_ptr<RetryPolicyType> retry_policy,
                     std::unique_ptr<BackoffPolicy> backoff_policy,
                     Idempotency idempotency, google::cloud::CompletionQueue cq,
                     Functor&& functor, Request request, char const* location)
      : retry_policy_(std::move(retry_policy)),
        backoff_policy_(std::move(backoff_policy)),
        idempotency_(idempotency),
        cq_(std::move(cq)),
        functor_(std::forward<Functor>(functor)),
        request_(std::move(request)),
        location_(location) {}

  using ReturnType = ::google::cloud::internal::invoke_result_t<
      Functor, google::cloud::CompletionQueue&,
      std::unique_ptr<grpc::ClientContext>, Request const&>;
  using T = typename FutureValueType<ReturnType>::value_type;

  future<T> Start() {
    auto weak = std::weak_ptr<AsyncRetryLoopImpl>(this->shared_from_this());
    result_ = promise<T>([weak]() mutable {
      if (auto self = weak.lock()) {
        OptionsSpan span(self->options_);
        self->Cancel();
      }
    });

    StartAttempt();
    return result_.get_future();
  }

 private:
  using TimerArgType = StatusOr<std::chrono::system_clock::time_point>;

  struct State {
    bool cancelled;
    std::uint_fast32_t operation;
  };

  State StartOperation() {
    std::unique_lock<std::mutex> lk(mu_);
    if (!cancelled_) return State{false, ++operation_};
    return SetDoneWithCancel(std::move(lk));
  }

  State OnOperation() {
    std::unique_lock<std::mutex> lk(mu_);
    if (!cancelled_) return State{false, operation_};
    return SetDoneWithCancel(std::move(lk));
  }

  void StartAttempt() {
    auto self = this->shared_from_this();
    if (retry_policy_->IsExhausted()) {
      return SetDone(
          RetryLoopError("Retry policy exhausted in", location_, last_status_));
    }
    auto state = StartOperation();
    if (state.cancelled) return;
    auto context = absl::make_unique<grpc::ClientContext>();
    ConfigureContext(*context, options_);
    SetupContext<RetryPolicyType>::Setup(*retry_policy_, *context);
    SetPending(
        state.operation,
        functor_(cq_, std::move(context), request_).then([self](future<T> f) {
          self->OnAttempt(f.get());
        }));
  }

  void StartBackoff() {
    auto self = this->shared_from_this();
    auto state = StartOperation();
    if (state.cancelled) return;
    SetPending(state.operation,
               cq_.MakeRelativeTimer(backoff_policy_->OnCompletion())
                   .then([self](future<TimerArgType> f) {
                     self->OnBackoff(f.get());
                   }));
  }

  void OnAttempt(T result) {
    // A successful attempt, set the value and finish the loop.
    if (result.ok()) return SetDone(std::move(result));
    // Some kind of failure, first verify that it is retryable.
    last_status_ = GetResultStatus(std::move(result));
    if (idempotency_ == Idempotency::kNonIdempotent) {
      return SetDone(RetryLoopError("Error in non-idempotent operation",
                                    location_, last_status_));
    }
    if (!retry_policy_->OnFailure(last_status_)) {
      if (retry_policy_->IsPermanentFailure(last_status_)) {
        return SetDone(
            RetryLoopError("Permanent error in", location_, last_status_));
      }
      return SetDone(
          RetryLoopError("Retry policy exhausted in", location_, last_status_));
    }
    StartBackoff();
  }

  void OnBackoff(TimerArgType tp) {
    auto state = OnOperation();
    // Check for the retry loop cancellation first. We want to report that
    // status instead of the timer failure in that case.
    if (state.cancelled) return;
    if (!tp) {
      // Some kind of error in the CompletionQueue, probably shutting down.
      return SetDone(RetryLoopError("Timer failure in", location_,
                                    std::move(tp).status()));
    }
    StartAttempt();
  }

  void SetPending(std::uint_fast32_t operation, future<void> op) {
    std::unique_lock<std::mutex> lk(mu_);
    if (operation_ == operation) pending_operation_ = std::move(op);
    if (cancelled_) return Cancel(std::move(lk));
  }

  // Handle the case where the retry loop finishes due to a successful request
  // or the retry policies getting exhausted.
  void SetDone(T value) {
    std::unique_lock<std::mutex> lk(mu_);
    if (done_) return;
    done_ = true;
    lk.unlock();
    result_.set_value(std::move(value));
  }

  // Handle the case where the retry loop finishes due to a successful cancel
  // request.
  State SetDoneWithCancel(std::unique_lock<std::mutex> lk) {
    if (done_) return State{true, 0};
    done_ = true;
    lk.unlock();
    result_.set_value(
        RetryLoopError("Retry loop cancelled", location_, last_status_));
    return State{true, 0};
  }

  void Cancel() { return Cancel(std::unique_lock<std::mutex>{mu_}); }

  void Cancel(std::unique_lock<std::mutex> lk) {
    cancelled_ = true;
    future<void> f = std::move(pending_operation_);
    lk.unlock();
    f.cancel();
  }

  std::unique_ptr<RetryPolicyType> retry_policy_;
  std::unique_ptr<BackoffPolicy> backoff_policy_;
  Idempotency idempotency_ = Idempotency::kNonIdempotent;
  google::cloud::CompletionQueue cq_;
  absl::decay_t<Functor> functor_;
  Request request_;
  char const* location_ = "unknown";
  Options options_ = CurrentOptions();
  Status last_status_ = Status(StatusCode::kUnknown, "Retry policy exhausted");
  promise<T> result_;

  // Only the following variables require synchronization, as they coordinate
  // the work between the retry loop (which would be lock-free) and the cancel
  // requests (which need locks).
  std::mutex mu_;
  bool cancelled_ = false;
  bool done_ = false;
  std::uint_fast32_t operation_ = 0;
  future<void> pending_operation_;
};

/**
 * Create the right AsyncRetryLoopImpl object and start the retry loop on it.
 */
template <typename Functor, typename Request, typename RetryPolicyType,
          typename std::enable_if<
              google::cloud::internal::is_invocable<
                  Functor, google::cloud::CompletionQueue&,
                  std::unique_ptr<grpc::ClientContext>, Request const&>::value,
              int>::type = 0>
auto AsyncRetryLoop(std::unique_ptr<RetryPolicyType> retry_policy,
                    std::unique_ptr<BackoffPolicy> backoff_policy,
                    Idempotency idempotency, google::cloud::CompletionQueue cq,
                    Functor&& functor, Request request, char const* location)
    -> google::cloud::internal::invoke_result_t<
        Functor, google::cloud::CompletionQueue&,
        std::unique_ptr<grpc::ClientContext>, Request const&> {
  auto loop =
      std::make_shared<AsyncRetryLoopImpl<Functor, Request, RetryPolicyType>>(
          std::move(retry_policy), std::move(backoff_policy), idempotency,
          std::move(cq), std::forward<Functor>(functor), std::move(request),
          location);
  return loop->Start();
}

}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_ASYNC_RETRY_LOOP_H
