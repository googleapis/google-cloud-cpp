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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INTERNAL_ASYNC_POLL_OP_H_
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INTERNAL_ASYNC_POLL_OP_H_

#include "google/cloud/bigtable/completion_queue.h"
#include "google/cloud/bigtable/internal/async_op_traits.h"
#include "google/cloud/bigtable/internal/conjunction.h"
#include "google/cloud/bigtable/metadata_update_policy.h"
#include "google/cloud/bigtable/polling_policy.h"
#include "google/cloud/internal/make_unique.h"

namespace google {
namespace cloud {
namespace bigtable {
inline namespace BIGTABLE_CLIENT_NS {
namespace internal {

struct PrototypePollOpStartCallback {
  void operator()(CompletionQueue&, bool finished, grpc::Status&) const {}
};

/**
 * A check if the template parameter meets criteria for `AsyncPollOp`.
 *
 * This struct inherits from `std::true_type` or `std::false_type` depending on
 * whether it meets the criteria for an `Operation` parameter to `AsyncPollOp`.
 *
 * These criteria are:
 *  - has a `Start` member function,
 *  - has a `AccumulatedResult` member function,
 *  - the `Start` function is invokable with `CompletionQueue&`,
 *    `std::unique_ptr<grpc::ClientContext>&&` and `Functor&&`, where `Functor`
 *    is invokable with `CompletionQueue&`, bool and `grpc::Status&`,
 *  - the `AccumulatedResult` is invocable with no arguments,
 *  - the `Start` function returns a std::shared_ptr<AsyncOperation>
 *  - the `AccumulatedResult` function has the same return type as
 *    `Operation::Response`.
 */
template <typename Operation>
struct MeetsAsyncPollOperationRequirements
    : public conjunction<
          HasStart<Operation, PrototypePollOpStartCallback>,
          HasAccumulatedResult<Operation>,
          google::cloud::internal::is_invocable<
              decltype(
                  &Operation::template Start<PrototypePollOpStartCallback>),
              Operation&, CompletionQueue&,
              std::unique_ptr<grpc::ClientContext>&&,
              PrototypePollOpStartCallback&&>,
          google::cloud::internal::is_invocable<
              decltype(&Operation::AccumulatedResult), Operation&>,
          std::is_same<google::cloud::internal::invoke_result_t<
                           decltype(&Operation::AccumulatedResult), Operation&>,
                       typename Operation::Response>,
          std::is_same<google::cloud::internal::invoke_result_t<
                           decltype(&Operation::template Start<
                                    PrototypePollOpStartCallback>),
                           Operation&, CompletionQueue&,
                           std::unique_ptr<grpc::ClientContext>&&,
                           PrototypePollOpStartCallback&&>,
                       std::shared_ptr<AsyncOperation>>> {};

/**
 * Perform asynchronous polling.
 *
 * @tparam Functor the type of the function-like object that will receive the
 *     results.
 *
 * @tparam Operation a class responsible for submitting requests. Its `Start()`
 *     member function will be used for sending the retries and the original
 *     request. It follows the same scheme as AsyncRetryOp.
 *     `AsyncCheckConsistency` and `MeetsAsyncPollOperationRequirements`.
 *
 * @tparam valid_callback_type a format parameter, uses `std::enable_if<>` to
 *     disable this template if the functor does not match the expected
 *     signature.
 */
template <typename Functor, typename Operation,
          typename std::enable_if<
              google::cloud::internal::is_invocable<
                  Functor, CompletionQueue&, typename Operation::Response&,
                  grpc::Status&>::value,
              int>::type valid_callback_type = 0,
          typename std::enable_if<
              MeetsAsyncPollOperationRequirements<Operation>::value, int>::type
              operation_meets_requirements = 0>
class AsyncPollOp
    : public std::enable_shared_from_this<AsyncPollOp<Functor, Operation>>,
      public AsyncOperation {
 public:
  explicit AsyncPollOp(char const* error_message,
                       std::unique_ptr<PollingPolicy> polling_policy,
                       MetadataUpdatePolicy metadata_update_policy,
                       Functor&& callback, Operation&& operation)
      : cancelled_(),
        error_message_(error_message),
        polling_policy_(std::move(polling_policy)),
        metadata_update_policy_(std::move(metadata_update_policy)),
        callback_(std::forward<Functor>(callback)),
        operation_(std::move(operation)) {}

  void Cancel() override {
    std::lock_guard<std::mutex> lk(mu_);
    cancelled_ = true;
    if (current_op_) {
      current_op_->Cancel();
      current_op_.reset();
    }
  }

  bool Notify(CompletionQueue& cq, bool ok) override {
    // TODO(#1389) Notify should be moved from AsyncOperation to some more
    // specific derived class.
    google::cloud::internal::RaiseLogicError(
        "This member function doesn't make sense in "
        "AsyncRetryOp");
  }

  std::shared_ptr<AsyncOperation> Start(CompletionQueue& cq) {
    auto self = this->shared_from_this();
    std::unique_lock<std::mutex> lk(mu_);
    if (cancelled_) {
      lk.unlock();
      // We could fire the callback right here, but we'd be risking a deadlock
      // if the user held a lock while submitting this request. Instead, let's
      // schedule the callback to fire on the thread running the completion
      // queue.
      // There is no reason to store this timer in current_op_.
      cq.RunAsync([self](CompletionQueue& cq) { self->OnTimer(cq, false); });
      return self;
    }
    StartUnlocked(cq);
    return self;
  }

 private:
  /**
   * Kick off the asynchronous request.
   *
   * @param cq the completion queue to run the asynchronous operations.
   */
  void StartUnlocked(CompletionQueue& cq) {
    auto self = this->shared_from_this();
    auto context = google::cloud::internal::make_unique<grpc::ClientContext>();
    // TODO(1431): add polling_policy_->Setup();
    metadata_update_policy_.Setup(*context);

    current_op_ = operation_.Start(
        cq, std::move(context),
        [self](CompletionQueue& cq, bool finished, grpc::Status& status) {
          self->OnCompletion(cq, finished, status);
        });
  }

  std::string FullErrorMessageUnlocked(char const* where) {
    std::string full_message = error_message_;
    full_message += "(" + metadata_update_policy_.value() + ") ";
    full_message += where;
    return full_message;
  }

  std::string FullErrorMessageUnlocked(char const* where,
                                       grpc::Status const& status) {
    std::string full_message = FullErrorMessageUnlocked(where);
    full_message += ", last error=";
    full_message += status.error_message();
    return full_message;
  }

  /// The callback to handle one asynchronous request completing.
  void OnCompletion(CompletionQueue& cq, bool finished, grpc::Status& status) {
    std::unique_lock<std::mutex> lk(mu_);
    // If we don't schedule a timer, we don't want this object to
    // hold the operation.
    current_op_.reset();
    // If the underlying operation didn't notice a cancel request and reported
    // a different error or success, we should report the error or success
    // unless we would continue trying. This is because it is our best knowledge
    // about the status of the retried request.
    if (status.error_code() == grpc::StatusCode::CANCELLED) {
      // Cancelled, no retry necessary.
      auto res = operation_.AccumulatedResult();
      grpc::Status res_status(
          grpc::StatusCode::CANCELLED,
          FullErrorMessageUnlocked("pending operation cancelled", status),
          status.error_details());
      lk.unlock();
      callback_(cq, res, res_status);
      return;
    }
    if (finished) {
      // Finished, just report the result.
      auto res = operation_.AccumulatedResult();
      lk.unlock();
      callback_(cq, res, status);
      return;
    }
    // At this point we know the operation is not finished and not cancelled.
    if (not status.ok() and not polling_policy_->OnFailure(status)) {
      std::string full_message =
          FullErrorMessageUnlocked(polling_policy_->IsPermanentError(status)
                                       ? "permanent error"
                                       : "too many transient errors",
                                   status);
      auto res = operation_.AccumulatedResult();
      grpc::Status res_status(status.error_code(), full_message,
                              status.error_details());
      lk.unlock();
      callback_(cq, res, res_status);
      return;
    }
    if (polling_policy_->Exhausted()) {
      auto res = operation_.AccumulatedResult();
      grpc::Status res_status(
          grpc::StatusCode::UNKNOWN,
          FullErrorMessageUnlocked("polling policy exhausted"));
      lk.unlock();
      callback_(cq, res, res_status);
      return;
    }
    if (cancelled_) {
      // At this point we know that the user intended to Cancel and we'd retry,
      // so let's report the cancellation status to them.
      auto res = operation_.AccumulatedResult();
      grpc::Status res_status(
          grpc::StatusCode::CANCELLED,
          FullErrorMessageUnlocked("pending operation cancelled", status),
          status.error_details());
      lk.unlock();
      callback_(cq, res, res_status);
      return;
    }

    auto delay = polling_policy_->WaitPeriod();
    auto self = this->shared_from_this();
    current_op_ = cq.MakeRelativeTimer(
        delay, [self](CompletionQueue& cq, AsyncTimerResult result) {
          self->OnTimer(cq, result.cancelled);
        });
  }

  void OnTimer(CompletionQueue& cq, bool cancelled) {
    std::unique_lock<std::mutex> lk(mu_);
    if (cancelled or cancelled_) {
      // Cancelled, no more action to take.
      current_op_.reset();
      auto res = operation_.AccumulatedResult();
      grpc::Status res_status(
          grpc::StatusCode::CANCELLED,
          FullErrorMessageUnlocked("pending timer cancelled"));
      lk.unlock();
      callback_(cq, res, res_status);
      return;
    }
    StartUnlocked(cq);
  }

  std::mutex mu_;
  // Because of the racy nature of cancellation, a cancelled timer or operation
  // might occasionally return a non-cancelled status (e.g. when cancellation
  // occurs right before firing the callback). In order to not schedule a next
  // retry in such a scenario, we indicate cancellation by using this flag.
  bool cancelled_;
  char const* error_message_;
  std::unique_ptr<PollingPolicy> polling_policy_;
  MetadataUpdatePolicy metadata_update_policy_;
  Functor callback_;
  // A handle to a currently ongoing async operation - either a timer or one
  // created through `Operation::Start`.
  std::shared_ptr<AsyncOperation> current_op_;

 protected:
  Operation operation_;
};

}  // namespace internal
}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INTERNAL_ASYNC_POLL_OP_H_
