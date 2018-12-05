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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INTERNAL_ASYNC_LIST_INSTANCES_H_
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INTERNAL_ASYNC_LIST_INSTANCES_H_

#include "google/bigtable/admin/v2/bigtable_instance_admin.grpc.pb.h"
#include "google/cloud/bigtable/async_instance_list_responses.h"
#include "google/cloud/bigtable/completion_queue.h"
#include "google/cloud/bigtable/instance_admin_client.h"
#include "google/cloud/bigtable/internal/async_retry_multi_page.h"
#include "google/cloud/bigtable/metadata_update_policy.h"
#include "google/cloud/internal/make_unique.h"

namespace google {
namespace cloud {
namespace bigtable {
inline namespace BIGTABLE_CLIENT_NS {
namespace internal {

/**
 * A wrapped call to AsyncListInstances, to be used in AsyncRetryMultiPage.
 *
 * It also encapsulates calling this RPC and accumulates the result.
 */
class AsyncListInstances {
 public:
  using Request = google::bigtable::admin::v2::ListInstancesRequest;
  using Response = InstanceList;

  AsyncListInstances(std::shared_ptr<InstanceAdminClient> client,
                     std::string project_name)
      : client_(client), project_name_(std::move(project_name)) {}

  /**
   * Start the bound asynchronous request.
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
                google::cloud::internal::is_invocable<
                    Functor, CompletionQueue&, bool, grpc::Status&>::value,
                int>::type valid_callback_type = 0>
  std::shared_ptr<AsyncOperation> Start(
      CompletionQueue& cq, std::unique_ptr<grpc::ClientContext>&& context,
      Functor&& callback) {
    Request request;
    request.set_parent(project_name_);
    if (!next_page_token_.empty()) {
      request.set_page_token(next_page_token_);
    }

    return cq.MakeUnaryRpc(
        *client_, &InstanceAdminClient::AsyncListInstances, request,
        std::move(context),
        [this, callback](
            CompletionQueue& cq,
            google::bigtable::admin::v2::ListInstancesResponse& response,
            grpc::Status& status) {
          if (!status.ok()) {
            callback(cq, false, status);
            return;
          }
          next_page_token_ = response.next_page_token();
          std::move(response.failed_locations().begin(),
                    response.failed_locations().end(),
                    std::inserter(failed_locations_, failed_locations_.end()));
          std::move(response.instances().begin(), response.instances().end(),
                    std::back_inserter(response_.instances));
          callback(cq, next_page_token_.empty(), status);
        });
  }

  InstanceList AccumulatedResult() {
    std::copy(failed_locations_.begin(), failed_locations_.end(),
              std::back_inserter(response_.failed_locations));
    failed_locations_.clear();
    return response_;
  }

 private:
  std::shared_ptr<bigtable::InstanceAdminClient> client_;
  std::string next_page_token_;
  std::string project_name_;
  std::set<std::string> failed_locations_;
  InstanceList response_;
};

/**
 * Perform an `AsyncListInstances` operation request with retries.
 *
 * @tparam Functor the type of the function-like object that will receive the
 *     results. It must satisfy (using C++17 types):
 *     static_assert(std::is_invocable_v<
 *         Functor, CompletionQueue&, InstanceList&, grpc::Status&>);
 *
 * @tparam valid_callback_type a format parameter, uses `std::enable_if<>` to
 *     disable this template if the functor does not match the expected
 *     signature.
 */
template <typename Functor,
          typename std::enable_if<google::cloud::internal::is_invocable<
                                      Functor, CompletionQueue&, InstanceList&,
                                      grpc::Status&>::value,
                                  int>::type valid_callback_type = 0>
class AsyncRetryListInstances
    : public AsyncRetryMultiPage<Functor, AsyncListInstances> {
 public:
  AsyncRetryListInstances(char const* error_message,
                          std::unique_ptr<RPCRetryPolicy> rpc_retry_policy,
                          std::unique_ptr<RPCBackoffPolicy> rpc_backoff_policy,
                          MetadataUpdatePolicy metadata_update_policy,
                          std::shared_ptr<InstanceAdminClient> client,
                          std::string project_name, Functor&& callback)
      : AsyncRetryMultiPage<Functor, AsyncListInstances>(
            error_message, std::move(rpc_retry_policy),
            std::move(rpc_backoff_policy), metadata_update_policy,
            std::forward<Functor>(callback),
            AsyncListInstances(std::move(client), std::move(project_name))){};
};

}  // namespace internal
}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INTERNAL_ASYNC_LIST_INSTANCES_H_
