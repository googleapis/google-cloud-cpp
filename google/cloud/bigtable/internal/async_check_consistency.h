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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INTERNAL_ASYNC_CHECK_CONSISTENCY_H_
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INTERNAL_ASYNC_CHECK_CONSISTENCY_H_

#include "google/cloud/bigtable/admin_client.h"
#include "google/cloud/bigtable/async_operation.h"
#include "google/cloud/bigtable/bigtable_strong_types.h"
#include "google/cloud/bigtable/completion_queue.h"
#include "google/cloud/bigtable/internal/async_poll_op.h"
#include "google/cloud/bigtable/internal/async_retry_unary_rpc.h"
#include "google/cloud/bigtable/table_strong_types.h"
#include "google/cloud/internal/invoke_result.h"
#include "google/cloud/internal/make_unique.h"

namespace google {
namespace cloud {
namespace bigtable {
inline namespace BIGTABLE_CLIENT_NS {
namespace internal {

/**
 * A CheckConsistency call bound with client and table.
 *
 * It satisfies the requirements to be used as the `Operation` parameter in
 * `AsyncPollOp`.
 *
 * It encapsulates calling this RPC and holds the result.
 */
class AsyncCheckConsistency {
 public:
  using Request = google::bigtable::admin::v2::CheckConsistencyRequest;
  using Response = bool;

  AsyncCheckConsistency(std::shared_ptr<AdminClient> client,
                        ConsistencyToken&& consistency_token,
                        std::string const& table_name)
      : client_(client), response_() {
    request_.set_name(table_name);
    request_.set_consistency_token(consistency_token.get());
  }

  /**
   * Start the bound aynchronous request.
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
    return cq.MakeUnaryRpc(
        *client_, &AdminClient::AsyncCheckConsistency, request_,
        std::move(context),
        [this, callback](
            CompletionQueue& cq,
            google::bigtable::admin::v2::CheckConsistencyResponse& response,
            grpc::Status& status) {
          if (status.ok() && response.consistent()) {
            response_ = true;
            callback(cq, true, status);
            return;
          }
          callback(cq, false, status);
        });
  }

  bool AccumulatedResult() { return response_; }

 private:
  std::shared_ptr<bigtable::AdminClient> client_;
  Request request_;
  bool response_;
};

/**
 * Poll `AsyncCheckConsistency` result.
 *
 * @tparam Functor the type of the function-like object that will receive the
 *     results. It must satisfy (using C++17 types):
 *     static_assert(std::is_invocable_v<
 *         Functor, CompletionQueue&, bool, grpc::Status&>);
 *
 * @tparam valid_callback_type a format parameter, uses `std::enable_if<>` to
 *     disable this template if the functor does not match the expected
 *     signature.
 */
template <typename Functor,
          typename std::enable_if<
              google::cloud::internal::is_invocable<Functor, CompletionQueue&,
                                                    bool, grpc::Status&>::value,
              int>::type valid_callback_type = 0>
class AsyncPollCheckConsistency
    : public AsyncPollOp<Functor, AsyncCheckConsistency> {
 public:
  AsyncPollCheckConsistency(char const* error_message,
                            std::unique_ptr<PollingPolicy> polling_policy,
                            MetadataUpdatePolicy metadata_update_policy,
                            std::shared_ptr<bigtable::AdminClient> client,
                            ConsistencyToken consistency_token,
                            std::string const& table_name, Functor&& callback)
      : AsyncPollOp<Functor, AsyncCheckConsistency>(
            error_message, std::move(polling_policy),
            std::move(metadata_update_policy), std::forward<Functor>(callback),
            AsyncCheckConsistency(std::move(client),
                                  std::move(consistency_token),
                                  std::move(table_name))) {}
};

}  // namespace internal
}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INTERNAL_ASYNC_CHECK_CONSISTENCY_H_
