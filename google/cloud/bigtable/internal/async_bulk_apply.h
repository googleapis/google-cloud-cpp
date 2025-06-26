// Copyright 2018 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INTERNAL_ASYNC_BULK_APPLY_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INTERNAL_ASYNC_BULK_APPLY_H

#include "google/cloud/bigtable/internal/bigtable_stub.h"
#include "google/cloud/bigtable/internal/bulk_mutator.h"
#include "google/cloud/bigtable/internal/operation_context.h"
#include "google/cloud/bigtable/options.h"
#include "google/cloud/bigtable/version.h"
#include "google/cloud/completion_queue.h"
#include "google/cloud/internal/call_context.h"
#include <atomic>
#include <string>
#include <vector>

namespace google {
namespace cloud {
namespace bigtable_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

/**
 * Implement the retry loop for AsyncBulkApply.
 *
 * The retry loop for AsyncBulkApply() is fairly different from all the other
 * retry loops: only those mutations that are idempotent and had a transient
 * failure can be retried, and the result for each mutation arrives in a stream.
 * This class implements that retry loop.
 */
class AsyncBulkApplier : public std::enable_shared_from_this<AsyncBulkApplier> {
 public:
  static future<std::vector<bigtable::FailedMutation>> Create(
      CompletionQueue cq, std::shared_ptr<BigtableStub> stub,
      std::shared_ptr<MutateRowsLimiter> limiter,
      std::unique_ptr<bigtable::DataRetryPolicy> retry_policy,
      std::unique_ptr<BackoffPolicy> backoff_policy, bool enable_server_retries,
      bigtable::IdempotentMutationPolicy& idempotent_policy,
      std::string const& app_profile_id, std::string const& table_name,
      bigtable::BulkMutation mut,
      // TODO: Remove default from parameter.
      std::shared_ptr<OperationContext> operation_context = {});

 private:
  AsyncBulkApplier(CompletionQueue cq, std::shared_ptr<BigtableStub> stub,
                   std::shared_ptr<MutateRowsLimiter> limiter,
                   std::unique_ptr<bigtable::DataRetryPolicy> retry_policy,
                   std::unique_ptr<BackoffPolicy> backoff_policy,
                   bool enable_server_retries,
                   bigtable::IdempotentMutationPolicy& idempotent_policy,
                   std::string const& app_profile_id,
                   std::string const& table_name, bigtable::BulkMutation mut,
                   // TODO: Remove default from parameter.
                   std::shared_ptr<OperationContext> operation_context = {});

  void StartIteration();
  void MakeRequest();
  void OnRead(google::bigtable::v2::MutateRowsResponse response);
  void OnFinish(Status const& status);
  void SetPromise();

  CompletionQueue cq_;
  std::shared_ptr<BigtableStub> stub_;
  std::shared_ptr<MutateRowsLimiter> limiter_;
  std::unique_ptr<bigtable::DataRetryPolicy> retry_policy_;
  std::unique_ptr<BackoffPolicy> backoff_policy_;
  bool enable_server_retries_;
  BulkMutatorState state_;
  std::atomic<bool> keep_reading_{true};
  promise<std::vector<bigtable::FailedMutation>> promise_;
  internal::ImmutableOptions options_;
  internal::CallContext call_context_;
  std::shared_ptr<grpc::ClientContext> context_;
  std::shared_ptr<OperationContext> operation_context_;
};

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INTERNAL_ASYNC_BULK_APPLY_H
