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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INTERNAL_ASYNC_RETRY_UNARY_RPC_AND_POLL_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INTERNAL_ASYNC_RETRY_UNARY_RPC_AND_POLL_H

#include "google/cloud/bigtable/completion_queue.h"
#include "google/cloud/bigtable/internal/async_longrunning_op.h"
#include "google/cloud/bigtable/internal/async_poll_op.h"
#include "google/cloud/bigtable/internal/async_retry_unary_rpc.h"
#include "google/cloud/bigtable/polling_policy.h"
#include "google/cloud/bigtable/version.h"
#include <google/longrunning/operations.grpc.pb.h>
#include <sstream>

namespace google {
namespace cloud {
namespace bigtable {
inline namespace BIGTABLE_CLIENT_NS {
namespace internal {

/**
 * Asynchronously start a longrunning operation (with retries) and poll its
 * result.
 *
 * @param location typically the name of the function that created this
 *     asynchronous retry loop.
 * @param polling_policy controls how often the server is queried.
 * @param rpc_retry_policy controls the number of retries, and what errors are
 *     considered retryable.
 * @param rpc_backoff_policy determines the wait time between retries.
 * @param idempotent_policy determines if a request is retryable.
 * @param metadata_update_policy controls how to update the metadata fields in
 *     the request.
 * @param client the client on which `AsyncGetOperation` is called to query
 *     the longrunning operation's status.
 * @param async_call the callable to start a new asynchronous operation.
 * @param request the parameters of the request.
 * @param cq the completion queue where the retry loop is executed.
 *
 * @return a future that becomes satisfied when either the retried RPC or
 *     polling for the longrunning operation's results fail despite retries or
 *     after both the initial RPC and the polling for the result of the
 *     longrunning operation it initiated complete successfully.
 */
template <typename Response, typename AsyncCallType, typename RequestType,
          typename IdempotencyPolicy, typename Client>
future<StatusOr<Response>> AsyncStartPollAfterRetryUnaryRpc(
    char const* location, std::unique_ptr<PollingPolicy> polling_policy,
    std::unique_ptr<RPCRetryPolicy> rpc_retry_policy,
    std::unique_ptr<RPCBackoffPolicy> rpc_backoff_policy,
    IdempotencyPolicy idempotent_policy,
    MetadataUpdatePolicy metadata_update_policy, std::shared_ptr<Client> client,
    AsyncCallType async_call, RequestType request, CompletionQueue cq) {
  static_assert(
      std::is_same<typename google::cloud::internal::AsyncCallResponseType<
                       AsyncCallType, RequestType>::type,
                   google::longrunning::Operation>::value,
      "async_call should return a google::longrunning::Operation");
  return StartAsyncLongrunningOp<Client, Response>(
      location, std::move(polling_policy), metadata_update_policy, client, cq,
      StartRetryAsyncUnaryRpc(
          location, std::move(rpc_retry_policy), std::move(rpc_backoff_policy),
          std::move(idempotent_policy), metadata_update_policy,
          std::move(async_call), std::move(request), cq));
}

}  // namespace internal
}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INTERNAL_ASYNC_RETRY_UNARY_RPC_AND_POLL_H
