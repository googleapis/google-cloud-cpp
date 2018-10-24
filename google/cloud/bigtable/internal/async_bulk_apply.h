// Copyright 2018 Google Inc.
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INTERNAL_ASYNC_BULK_APPLY_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INTERNAL_ASYNC_BULK_APPLY_H

#include "google/cloud/bigtable/async_operation.h"
#include "google/cloud/bigtable/bigtable_strong_types.h"
#include "google/cloud/bigtable/completion_queue.h"
#include "google/cloud/bigtable/data_client.h"
#include "google/cloud/bigtable/internal/bulk_mutator.h"
#include "google/cloud/bigtable/metadata_update_policy.h"
#include "google/cloud/bigtable/rpc_backoff_policy.h"
#include "google/cloud/bigtable/rpc_retry_policy.h"
#include "google/cloud/bigtable/table_strong_types.h"
#include "google/cloud/internal/invoke_result.h"
#include "google/cloud/internal/make_unique.h"

namespace google {
namespace cloud {
namespace bigtable {
inline namespace BIGTABLE_CLIENT_NS {
namespace internal {

template <typename Functor,
          typename std::enable_if<
              google::cloud::internal::is_invocable<
                  Functor, CompletionQueue&, std::vector<FailedMutation>&,
                  grpc::Status&>::value,
              int>::type valid_callback_type = 0>
class AsyncRetryBulkApply
    : public std::enable_shared_from_this<AsyncRetryBulkApply<Functor>> {
 public:
  AsyncRetryBulkApply(std::unique_ptr<RPCRetryPolicy> rpc_retry_policy,
                      std::unique_ptr<RPCBackoffPolicy> rpc_backoff_policy,
                      IdempotentMutationPolicy& idempotent_policy,
                      MetadataUpdatePolicy metadata_update_policy,
                      std::shared_ptr<bigtable::DataClient> client,
                      bigtable::AppProfileId const& app_profile_id,
                      bigtable::TableId const& table_name, BulkMutation&& mut,
                      Functor&& callback)
      : impl_(client, std::move(app_profile_id), std::move(table_name),
              idempotent_policy, std::forward<BulkMutation>(mut)),
        rpc_retry_policy_(std::move(rpc_retry_policy)),
        rpc_backoff_policy_(std::move(rpc_backoff_policy)),
        metadata_update_policy_(std::move(metadata_update_policy)),
        callback_(std::forward<Functor>(callback)) {}
  /**
   * Kick off the asynchronous request.
   *
   * @param cq the completion queue to run the asynchronous operations.
   */
  void Start(CompletionQueue& cq) {
    auto self = this->shared_from_this();
    auto context = google::cloud::internal::make_unique<grpc::ClientContext>();
    rpc_retry_policy_->Setup(*context);
    rpc_backoff_policy_->Setup(*context);
    metadata_update_policy_.Setup(*context);

    impl_.Start(cq, std::move(context),
                [self](CompletionQueue& cq, grpc::Status& status) {
                  self->OnCompletion(cq, status);
                });
  }

 private:
  std::string FullErrorMessage(char const* where) {
    std::string full_message =
        "AsyncBulkApply(" + metadata_update_policy_.value() + ") ";
    full_message += where;
    return full_message;
  }

  std::string FullErrorMessage(char const* where, grpc::Status const& status) {
    std::string full_message = FullErrorMessage(where);
    full_message += ", last error=";
    full_message += status.error_message();
    return full_message;
  }

  /// The callback to handle one asynchronous request completing.
  void OnCompletion(CompletionQueue& cq, grpc::Status& status) {
    if (status.error_code() == grpc::StatusCode::CANCELLED) {
      // Cancelled, no retry necessary.
      auto res = impl_.ExtractFinalFailures();
      grpc::Status res_status(
          grpc::StatusCode::CANCELLED,
          FullErrorMessage("pending operation cancelled", status),
          status.error_details());
      callback_(cq, res, res_status);
      return;
    }
    if (status.ok()) {
      // Success, just report the result.
      auto res = impl_.ExtractFinalFailures();
      callback_(cq, res, status);
      return;
    }
    if (not rpc_retry_policy_->OnFailure(status)) {
      std::string full_message =
          FullErrorMessage(RPCRetryPolicy::IsPermanentFailure(status)
                               ? "permanent error"
                               : "too many transient errors",
                           status);
      auto res = impl_.ExtractFinalFailures();
      grpc::Status res_status(status.error_code(), full_message,
                              status.error_details());
      callback_(cq, res, res_status);
      return;
    }
    // BulkMutator keeps track of idempotency of the mutations it holds, so
    // we'll just keep retrying if the policy says so.

    auto delay = rpc_backoff_policy_->OnCompletion(status);
    auto self = this->shared_from_this();
    cq.MakeRelativeTimer(
        delay,
        [self](CompletionQueue& cq, AsyncTimerResult timer,
               AsyncOperation::Disposition d) { self->OnTimer(cq, timer, d); });
  }

  void OnTimer(CompletionQueue& cq, AsyncTimerResult& timer,
               AsyncOperation::Disposition d) {
    if (d == AsyncOperation::CANCELLED) {
      // Cancelled, no more action to take.
      auto res = impl_.ExtractFinalFailures();
      grpc::Status res_status(grpc::StatusCode::CANCELLED,
                              FullErrorMessage("pending timer cancelled"));
      callback_(cq, res, res_status);
      return;
    }
    Start(cq);
  }

  AsyncBulkMutator impl_;
  std::unique_ptr<RPCRetryPolicy> rpc_retry_policy_;
  std::unique_ptr<RPCBackoffPolicy> rpc_backoff_policy_;
  MetadataUpdatePolicy metadata_update_policy_;
  Functor callback_;
};

}  // namespace internal
}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INTERNAL_ASYNC_BULK_APPLY_H
