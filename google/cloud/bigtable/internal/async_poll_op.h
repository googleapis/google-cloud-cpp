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
              int>::type valid_callback_type = 0>
class AsyncPollOp
    : public std::enable_shared_from_this<AsyncPollOp<Functor, Operation>> {
 public:
  explicit AsyncPollOp(char const* error_message,
                       std::unique_ptr<PollingPolicy> polling_policy,
                       MetadataUpdatePolicy metadata_update_policy,
                       Functor&& callback, Operation&& operation)
      : error_message_(error_message),
        polling_policy_(std::move(polling_policy)),
        metadata_update_policy_(std::move(metadata_update_policy)),
        callback_(std::forward<Functor>(callback)),
        operation_(std::move(operation)) {}

  void Start(CompletionQueue& cq) {
    std::unique_lock<std::mutex> lk(mu_);
    StartUnlocked(cq);
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

    operation_.Start(
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

    auto delay = polling_policy_->WaitPeriod();
    auto self = this->shared_from_this();
    cq.MakeRelativeTimer(delay,
                         [self](CompletionQueue& cq, AsyncTimerResult result) {
                           self->OnTimer(cq, result);
                         });
  }

  void OnTimer(CompletionQueue& cq, AsyncTimerResult& timer) {
    std::unique_lock<std::mutex> lk(mu_);
    if (timer.cancelled) {
      // Cancelled, no more action to take.
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
  char const* error_message_;
  std::unique_ptr<PollingPolicy> polling_policy_;
  MetadataUpdatePolicy metadata_update_policy_;
  Functor callback_;

 protected:
  Operation operation_;
};

}  // namespace internal
}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INTERNAL_ASYNC_POLL_OP_H_
