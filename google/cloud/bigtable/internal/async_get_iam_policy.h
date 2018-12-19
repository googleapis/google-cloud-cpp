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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INTERNAL_ASYNC_GET_IAM_POLICY_H_
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INTERNAL_ASYNC_GET_IAM_POLICY_H_

#include "google/cloud/bigtable/async_operation.h"
#include "google/cloud/bigtable/bigtable_strong_types.h"
#include "google/cloud/bigtable/completion_queue.h"
#include "google/cloud/bigtable/idempotent_mutation_policy.h"
#include "google/cloud/bigtable/instance_admin_client.h"
#include "google/cloud/bigtable/internal/async_retry_op.h"
#include "google/cloud/bigtable/internal/bulk_mutator.h"
#include "google/cloud/iam_policy.h"
#include "google/cloud/internal/invoke_result.h"
#include "google/cloud/internal/make_unique.h"
#include <google/bigtable/admin/v2/bigtable_instance_admin.grpc.pb.h>

namespace google {
namespace cloud {
namespace bigtable {
inline namespace BIGTABLE_CLIENT_NS {
namespace internal {

/**
 * A AsyncGetIamPolicy call bound with client, project name and instance id.
 *
 * It satisfies the requirements to be used as the `Operation` parameter in
 * `AsyncRetryOp`.
 *
 * It encapsulates calling this RPC and accumulates the result. In case of an
 * error, all partially accumulated data is dropped.
 */
class AsyncGetIamPolicy {
 public:
  using Request = google::iam::v1::GetIamPolicyRequest;
  using Response = google::iam::v1::Policy;

  AsyncGetIamPolicy(std::shared_ptr<InstanceAdminClient> client,
                    std::string project_name, std::string instance_id)
      : client_(std::move(client)), response_() {
    request_.set_resource(project_name + "/instances/" + instance_id);
  }

  /** Start the bound aynchronous request.
   *
   * @tparam Functor the type of the function-like object that will receive the
   *     results.
   *
   * @tparam valid_callback_type a format parameter, uses `std::enable_if<>` to
   *     disable this template if the functor does not match the expected
   *     signature.
   *
   * @param cq the completion queue to run the asynchronous operations.
   *
   * @param context the gRPC context used for this request
   *
   * @param callback the functor which will be fired in an unspecified thread
   *     once the response stream completes
   */
  template <typename Functor,
            typename std::enable_if<
                google::cloud::internal::is_invocable<Functor, CompletionQueue&,
                                                      grpc::Status&>::value,
                int>::type valid_callback_type = 0>
  std::shared_ptr<AsyncOperation> Start(
      CompletionQueue& cq, std::unique_ptr<grpc::ClientContext>&& context,
      Functor&& callback) {
    return cq.MakeUnaryRpc(
        *client_, &InstanceAdminClient::AsyncGetIamPolicy, request_,
        std::move(context),
        [this, callback](CompletionQueue& cq, Response& response,
                         grpc::Status& status) {
          if (status.ok()) {
            response_ = response;
            callback(cq, status);
            return;
          }
          callback(cq, status);
        });
  }

  google::cloud::IamPolicy AccumulatedResult() {
    return ProtoToWrapper(std::move(response_));
  }

 private:
  std::shared_ptr<InstanceAdminClient> client_;
  Response response_;
  Request request_;

 private:
  google::cloud::IamPolicy ProtoToWrapper(google::iam::v1::Policy proto) {
    google::cloud::IamPolicy result;
    result.version = proto.version();
    result.etag = std::move(*proto.mutable_etag());
    for (auto& binding : *proto.mutable_bindings()) {
      for (auto& member : *binding.mutable_members()) {
        result.bindings.AddMember(binding.role(), std::move(member));
      }
    }

    return result;
  }
};

/**
 * Perform an `AsyncGetIamPolicy` operation request with retries.
 *
 * @tparam Functor the type of the function-like object that will receive the
 *     results. It must satisfy (using C++17 types):
 *     static_assert(std::is_invocable_v<
 *         Functor, CompletionQueue&,
 * 		google::cloud::IamPolicy&, grpc::Status&>);
 *
 * @tparam valid_callback_type a format parameter, uses `std::enable_if<>` to
 *     disable this template if the functor does not match the expected
 *     signature.
 */
template <typename Functor,
          typename std::enable_if<
              google::cloud::internal::is_invocable<Functor, CompletionQueue&,
                                                    google::cloud::IamPolicy&,
                                                    grpc::Status&>::value,
              int>::type valid_callback_type = 0>
class AsyncRetryGetIamPolicy : public AsyncRetryOp<ConstantIdempotencyPolicy,
                                                   Functor, AsyncGetIamPolicy> {
 public:
  AsyncRetryGetIamPolicy(char const* error_message,
                         std::unique_ptr<RPCRetryPolicy> rpc_retry_policy,
                         std::unique_ptr<RPCBackoffPolicy> rpc_backoff_policy,
                         MetadataUpdatePolicy metadata_update_policy,
                         std::shared_ptr<InstanceAdminClient> client,
                         std::string project_name, std::string instance_id,
                         Functor&& callback)
      : AsyncRetryOp<ConstantIdempotencyPolicy, Functor, AsyncGetIamPolicy>(
            error_message, std::move(rpc_retry_policy),
            // BulkMutator is idempotent because it keeps track of idempotency
            // of the mutations it holds.
            std::move(rpc_backoff_policy), ConstantIdempotencyPolicy(true),
            std::move(metadata_update_policy), std::forward<Functor>(callback),
            AsyncGetIamPolicy(client, std::move(project_name),
                              std::move(instance_id))) {}
};

}  // namespace internal
}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INTERNAL_ASYNC_GET_IAM_POLICY_H_
