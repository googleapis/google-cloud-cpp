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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INTERNAL_ASYNC_BULK_APPLY_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INTERNAL_ASYNC_BULK_APPLY_H

#include "google/cloud/bigtable/completion_queue.h"
#include "google/cloud/bigtable/data_client.h"
#include "google/cloud/bigtable/idempotent_mutation_policy.h"
#include "google/cloud/bigtable/internal/async_retry_op.h"
#include "google/cloud/bigtable/internal/bulk_mutator.h"
#include "google/cloud/bigtable/version.h"
#include "google/cloud/internal/invoke_result.h"
#include "absl/memory/memory.h"
#include <string>
#include <vector>

namespace google {
namespace cloud {
namespace bigtable {
inline namespace BIGTABLE_CLIENT_NS {
namespace internal {
/**
 * Implement the retry loop for AsyncBulkApply.
 *
 * The retry loop for AsyncBulkApply() is fairly different from all the other
 * retry loops: only those mutations that are idempotent and had a transient
 * failure can be retried, and the result for each mutation arrives in a stream.
 * This class implements that retry loop.
 */
class AsyncRetryBulkApply
    : public std::enable_shared_from_this<AsyncRetryBulkApply> {
 public:
  static future<std::vector<FailedMutation>> Create(
      CompletionQueue cq, std::unique_ptr<RPCRetryPolicy> rpc_retry_policy,
      std::unique_ptr<RPCBackoffPolicy> rpc_backoff_policy,
      IdempotentMutationPolicy& idempotent_policy,
      MetadataUpdatePolicy metadata_update_policy,
      std::shared_ptr<bigtable::DataClient> client,
      std::string const& app_profile_id, std::string const& table_name,
      BulkMutation mut);

 private:
  AsyncRetryBulkApply(std::unique_ptr<RPCRetryPolicy> rpc_retry_policy,
                      std::unique_ptr<RPCBackoffPolicy> rpc_backoff_policy,
                      IdempotentMutationPolicy& idempotent_policy,
                      MetadataUpdatePolicy metadata_update_policy,
                      std::shared_ptr<bigtable::DataClient> client,
                      std::string const& app_profile_id,
                      std::string const& table_name, BulkMutation mut);

  void StartIteration(CompletionQueue cq);
  void OnRead(google::bigtable::v2::MutateRowsResponse response);
  void OnFinish(CompletionQueue cq, google::cloud::Status const& status);
  void SetPromise();

  std::unique_ptr<RPCRetryPolicy> rpc_retry_policy_;
  std::unique_ptr<RPCBackoffPolicy> rpc_backoff_policy_;
  MetadataUpdatePolicy metadata_update_policy_;
  std::shared_ptr<bigtable::DataClient> client_;
  BulkMutatorState state_;
  promise<std::vector<FailedMutation>> promise_;
};

}  // namespace internal
}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INTERNAL_ASYNC_BULK_APPLY_H
