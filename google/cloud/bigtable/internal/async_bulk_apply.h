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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INTERNAL_ASYNC_BULK_APPLY_H_
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INTERNAL_ASYNC_BULK_APPLY_H_

#include "google/cloud/bigtable/async_operation.h"
#include "google/cloud/bigtable/bigtable_strong_types.h"
#include "google/cloud/bigtable/completion_queue.h"
#include "google/cloud/bigtable/data_client.h"
#include "google/cloud/bigtable/idempotent_mutation_policy.h"
#include "google/cloud/bigtable/internal/async_retry_op.h"
#include "google/cloud/bigtable/internal/bulk_mutator.h"
#include "google/cloud/bigtable/table_strong_types.h"
#include "google/cloud/internal/invoke_result.h"
#include "google/cloud/internal/make_unique.h"

namespace google {
namespace cloud {
namespace bigtable {
inline namespace BIGTABLE_CLIENT_NS {
namespace internal {

/**
 * Perform an AsyncBulkApply operation request with retries.
 *
 * @tparam Client the class implementing the asynchronous operation, examples
 *     include `DataClient`, `AdminClient`, and `InstanceAdminClient`.
 *
 * @tparam MemberFunctionType the type of the member function to call on the
 *     `Client` object. This type must meet the requirements of
 *     `internal::CheckAsyncUnaryRpcSignature`, the `AsyncRetryUnaryRpc`
 *     template is disabled otherwise.
 *
 * @tparam Functor the type of the function-like object that will receive the
 *     results. It must satisfy (using C++17 types):
 *     static_assert(std::is_invocable_v<
 *         Functor, CompletionQueue&, std::vector<FailedMutation>&,
 *             grpc::Status&>);
 *
 * @tparam valid_callback_type a format parameter, uses `std::enable_if<>` to
 *     disable this template if the functor does not match the expected
 *     signature.
 */
template <typename Functor,
          typename std::enable_if<
              google::cloud::internal::is_invocable<
                  Functor, CompletionQueue&, std::vector<FailedMutation>&,
                  grpc::Status&>::value,
              int>::type valid_callback_type = 0>
class AsyncRetryBulkApply : public AsyncRetryOp<ConstantIdempotencyPolicy,
                                                Functor, AsyncBulkMutator> {
 public:
  AsyncRetryBulkApply(std::unique_ptr<RPCRetryPolicy> rpc_retry_policy,
                      std::unique_ptr<RPCBackoffPolicy> rpc_backoff_policy,
                      IdempotentMutationPolicy& idempotent_policy,
                      MetadataUpdatePolicy metadata_update_policy,
                      std::shared_ptr<bigtable::DataClient> client,
                      bigtable::AppProfileId const& app_profile_id,
                      bigtable::TableId const& table_name, BulkMutation&& mut,
                      Functor&& callback)
      : AsyncRetryOp<ConstantIdempotencyPolicy, Functor, AsyncBulkMutator>(
            __func__, std::move(rpc_retry_policy),
            // BulkMutator is idempotent because it keeps track of idempotency
            // of the mutations it holds.
            std::move(rpc_backoff_policy), ConstantIdempotencyPolicy(true),
            std::move(metadata_update_policy), std::forward<Functor>(callback),
            AsyncBulkMutator(client, std::move(app_profile_id),
                             std::move(table_name), idempotent_policy,
                             std::forward<BulkMutation>(mut))) {}
};

}  // namespace internal
}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INTERNAL_ASYNC_BULK_APPLY_H_
