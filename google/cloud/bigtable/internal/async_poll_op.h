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
#include "google/cloud/bigtable/internal/async_loop_op.h"
#include "google/cloud/bigtable/internal/async_op_traits.h"
#include "google/cloud/bigtable/metadata_update_policy.h"
#include "google/cloud/bigtable/polling_policy.h"
#include "google/cloud/internal/make_unique.h"

namespace google {
namespace cloud {
namespace bigtable {
inline namespace BIGTABLE_CLIENT_NS {
namespace internal {

/**
 * A dummy function object only to ease specification of pollable operations.
 *
 * It is an example type which could be passed to `Start()` member function of
 * the operation to be polled.
 */
struct PrototypePollOpStartCallback {
  void operator()(CompletionQueue&, bool finished, grpc::Status&) const {}
};

/**
 * A check if the template parameter meets criteria for `AsyncPollOp`.
 */
template <typename Operation>
struct MeetsAsyncPollOperationRequirements {
  static_assert(
      HasStart<Operation, PrototypePollOpStartCallback>::value,
      "Operation has to have a templated Start() member function "
      "instantiatable with functors like PrototypePollOpStartCallback");
  static_assert(
      HasAccumulatedResult<Operation>::value,
      "Operation has to have an AccumulatedResult() member function.");
  static_assert(
      google::cloud::internal::is_invocable<
          decltype(&Operation::template Start<PrototypePollOpStartCallback>),
          Operation&, CompletionQueue&, std::unique_ptr<grpc::ClientContext>&&,
          PrototypePollOpStartCallback&&>::value,
      "Operation::Start<PrototypePollOpStartCallback> has to be "
      "non-static and invocable with "
      "CompletionQueue&, std::unique_ptr<grpc::ClientContext>&&, "
      "PrototypePollOpStartCallback&&.");
  static_assert(google::cloud::internal::is_invocable<
                    decltype(&Operation::AccumulatedResult), Operation&>::value,
                "Operation::AccumulatedResult has to be non-static and "
                "invokable with no arguments");
  static_assert(std::is_same<google::cloud::internal::invoke_result_t<
                                 decltype(&Operation::template Start<
                                          PrototypePollOpStartCallback>),
                                 Operation&, CompletionQueue&,
                                 std::unique_ptr<grpc::ClientContext>&&,
                                 PrototypePollOpStartCallback&&>,
                             std::shared_ptr<AsyncOperation>>::value,
                "Operation::Start<>(...) has to return a "
                "std::shared_ptr<AsyncOperation>");

  using Response = google::cloud::internal::invoke_result_t<
      decltype(&Operation::AccumulatedResult), Operation&>;
};

/**
 * A wrapper for operations adding polling logic if passed to `AsyncLoopOp`.
 *
 * If used in `AsyncLoopOp`, it will asynchronously poll `Operation`
 *
 * @tparam UserFunctor the type of the function-like object that will receive
 *     the results.
 *
 * @tparam Operation a class responsible for submitting requests. Its `Start()`
 *     member function will be used for sending the retries and the original
 *     request. It follows the same scheme as AsyncRetryOp. It should satisfy
 *     `MeetsAsyncPollOperationRequirements`.
 */
template <typename UserFunctor, typename Operation>
class PollableLoopAdapter {
  using Response =
      typename MeetsAsyncPollOperationRequirements<Operation>::Response;
  static_assert(
      google::cloud::internal::is_invocable<UserFunctor, CompletionQueue&,
                                            Response&, grpc::Status&>::value,
      "UserFunctor should be callable with Operation::AccumulatedResult()'s "
      "return value.");

 public:
  explicit PollableLoopAdapter(char const* error_message,
                               std::unique_ptr<PollingPolicy> polling_policy,
                               MetadataUpdatePolicy metadata_update_policy,
                               UserFunctor&& callback, Operation&& operation)
      : error_message_(error_message),
        polling_policy_(std::move(polling_policy)),
        metadata_update_policy_(std::move(metadata_update_policy)),
        user_callback_(std::forward<UserFunctor>(callback)),
        operation_(std::move(operation)) {}

  template <typename AttemptFunctor>
  std::shared_ptr<AsyncOperation> Start(
      CompletionQueue& cq, AttemptFunctor&& attempt_completed_callback) {
    auto context = google::cloud::internal::make_unique<grpc::ClientContext>();
    // TODO(1431): add polling_policy_->Setup();
    metadata_update_policy_.Setup(*context);

    return operation_.Start(
        cq, std::move(context),
        [this, attempt_completed_callback](CompletionQueue& cq, bool finished,
                                           grpc::Status& status) {
          OnCompletion(cq, finished, status,
                       std::move(attempt_completed_callback));
        });
  }

  std::chrono::milliseconds WaitPeriod() {
    return polling_policy_->WaitPeriod();
  }

  void Cancel(CompletionQueue& cq) {
    auto res = operation_.AccumulatedResult();
    grpc::Status res_status(
        grpc::StatusCode::CANCELLED,
        FullErrorMessageUnlocked("pending operation cancelled"));
    user_callback_(cq, res, res_status);
  }

 private:
  /// The callback to handle one asynchronous request completing.
  template <typename AttemptFunctor>
  void OnCompletion(CompletionQueue& cq, bool finished, grpc::Status& status,
                    AttemptFunctor&& attempt_completed_callback) {
    if (status.error_code() == grpc::StatusCode::CANCELLED) {
      // Cancelled, no retry necessary.
      Cancel(cq);
      attempt_completed_callback(cq, true);
      return;
    }
    if (finished) {
      // Finished, just report the result.
      auto res = operation_.AccumulatedResult();
      user_callback_(cq, res, status);
      attempt_completed_callback(cq, true);
      return;
    }
    // At this point we know the operation is not finished and not cancelled.

    // TODO(#1475) remove this hack.
    // PollingPolicy's interface doesn't allow it to make a decision on the
    // delay depending on whether there was a success or a failure. This is
    // because PollingPolicy never gets to know about a successful attempt. In
    // order to work around it in this particular class we keep the invariant
    // that a call to OnFailure() always preceeds a call to WaitPeriod(). That
    // way the policy can react differently to successful requests.
    bool const allowed_to_retry = polling_policy_->OnFailure(status);
    if (not status.ok() and not allowed_to_retry) {
      std::string full_message =
          FullErrorMessageUnlocked(polling_policy_->IsPermanentError(status)
                                       ? "permanent error"
                                       : "too many transient errors",
                                   status);
      auto res = operation_.AccumulatedResult();
      grpc::Status res_status(status.error_code(), full_message,
                              status.error_details());
      user_callback_(cq, res, res_status);
      attempt_completed_callback(cq, true);
      return;
    }
    if (polling_policy_->Exhausted()) {
      auto res = operation_.AccumulatedResult();
      grpc::Status res_status(
          grpc::StatusCode::UNKNOWN,
          FullErrorMessageUnlocked("polling policy exhausted"));
      user_callback_(cq, res, res_status);
      attempt_completed_callback(cq, true);
      return;
    }
    status_ = status;
    attempt_completed_callback(cq, false);
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

  char const* error_message_;
  std::unique_ptr<PollingPolicy> polling_policy_;
  MetadataUpdatePolicy metadata_update_policy_;
  UserFunctor user_callback_;
  Operation operation_;
  grpc::Status status_;
};

/**
 * Perform asynchronous polling.
 *
 * @tparam Functor the type of the function-like object that will receive the
 *     results.
 *
 * @tparam Operation a class responsible for submitting requests. Its `Start()`
 *     member function will be used for sending the retries and the original
 *     request. It follows the same scheme as AsyncRetryOp. It should satisfy
 *     `MeetsAsyncPollOperationRequirements`.
 */
template <typename Functor, typename Operation>
class AsyncPollOp
    : public AsyncLoopOp<PollableLoopAdapter<Functor, Operation>> {
 public:
  explicit AsyncPollOp(char const* error_message,
                       std::unique_ptr<PollingPolicy> polling_policy,
                       MetadataUpdatePolicy metadata_update_policy,
                       Functor&& callback, Operation&& operation)
      : AsyncLoopOp<PollableLoopAdapter<Functor, Operation>>(
            PollableLoopAdapter<Functor, Operation>(
                error_message, std::move(polling_policy),
                metadata_update_policy, std::forward<Functor>(callback),
                std::move(operation))) {}
};

}  // namespace internal
}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INTERNAL_ASYNC_POLL_OP_H_
