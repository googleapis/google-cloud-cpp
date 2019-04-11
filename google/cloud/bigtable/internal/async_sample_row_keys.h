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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INTERNAL_ASYNC_SAMPLE_ROW_KEYS_H_
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INTERNAL_ASYNC_SAMPLE_ROW_KEYS_H_

#include "google/cloud/bigtable/async_operation.h"
#include "google/cloud/bigtable/bigtable_strong_types.h"
#include "google/cloud/bigtable/completion_queue.h"
#include "google/cloud/bigtable/data_client.h"
#include "google/cloud/bigtable/idempotent_mutation_policy.h"
#include "google/cloud/bigtable/internal/async_retry_op.h"
#include "google/cloud/bigtable/internal/bulk_mutator.h"
#include "google/cloud/bigtable/row_key_sample.h"
#include "google/cloud/bigtable/table_strong_types.h"
#include "google/cloud/bigtable/version.h"
#include "google/cloud/internal/invoke_result.h"
#include "google/cloud/internal/make_unique.h"

namespace google {
namespace cloud {
namespace bigtable {
inline namespace BIGTABLE_CLIENT_NS {
namespace internal {

/**
 * A SampleRowKeys call bound with client, table and app_profile_id.
 *
 * It satisfies the requirements to be used as the `Operation` parameter in
 * `AsyncRetryOp`.
 *
 * It encapsulates calling this RPC and accumulates the result. In case of an
 * error, all partially accumulated data is dropped.
 */
class AsyncSampleRowKeys {
 public:
  using Request = google::bigtable::v2::SampleRowKeysRequest;
  using Response = std::vector<RowKeySample>;

  AsyncSampleRowKeys(std::shared_ptr<DataClient> client,
                     bigtable::AppProfileId const& app_profile_id,
                     bigtable::TableId const& table_name);

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
      CompletionQueue& cq, std::unique_ptr<grpc::ClientContext> context,
      Functor&& callback) {
    return cq.MakeUnaryStreamRpc(
        *client_, &DataClient::AsyncSampleRowKeys, request_, std::move(context),
        [this](CompletionQueue&, const grpc::ClientContext&,
               google::bigtable::v2::SampleRowKeysResponse& response) {
          response_.emplace_back(RowKeySample{std::move(response.row_key()),
                                              response.offset_bytes()});
        },
        FinishedCallback<Functor>(*this, std::forward<Functor>(callback)));
  }

  Response AccumulatedResult() { return response_; }

 private:
  template <typename Functor,
            typename std::enable_if<
                google::cloud::internal::is_invocable<Functor, CompletionQueue&,
                                                      grpc::Status&>::value,
                int>::type valid_callback_type = 0>
  struct FinishedCallback {
    FinishedCallback(AsyncSampleRowKeys& parent, Functor callback)
        : parent_(parent), callback_(callback) {}

    void operator()(CompletionQueue& cq, grpc::ClientContext& context,
                    grpc::Status& status) {
      if (!status.ok()) {
        // The sample must be a consistent sample of the rows in the table. On
        // failure we must forget the previous responses and accumulate only
        // new values.
        parent_.response_ = Response();
      }
      callback_(cq, status);
    }

    // The user of AsyncSampleRowKeys has to make sure that it is not destructed
    // before all callbacks return, so we have a guarantee that this reference
    // is valid for as long as we don't call callback_.
    AsyncSampleRowKeys& parent_;
    Functor callback_;
  };

 private:
  std::shared_ptr<bigtable::DataClient> client_;
  Request request_;
  Response response_;
};

/**
 * Perform an `AsyncSampleRowKeys` operation request with retries.
 *
 * @tparam Functor the type of the function-like object that will receive the
 *     results. It must satisfy (using C++17 types):
 *     static_assert(std::is_invocable_v<
 *         Functor, CompletionQueue&, std::vector<RowKeySample>&,
 *             grpc::Status&>);
 *
 * @tparam valid_callback_type a format parameter, uses `std::enable_if<>` to
 *     disable this template if the functor does not match the expected
 *     signature.
 */
template <typename Functor,
          typename std::enable_if<
              google::cloud::internal::is_invocable<Functor, CompletionQueue&,
                                                    std::vector<RowKeySample>&,
                                                    grpc::Status&>::value,
              int>::type valid_callback_type = 0>
class AsyncRetrySampleRowKeys
    : public AsyncRetryOp<ConstantIdempotencyPolicy, Functor,
                          AsyncSampleRowKeys> {
 public:
  AsyncRetrySampleRowKeys(char const* error_message,
                          std::unique_ptr<RPCRetryPolicy> rpc_retry_policy,
                          std::unique_ptr<RPCBackoffPolicy> rpc_backoff_policy,
                          MetadataUpdatePolicy metadata_update_policy,
                          std::shared_ptr<bigtable::DataClient> client,
                          bigtable::AppProfileId const& app_profile_id,
                          bigtable::TableId const& table_name, Functor callback)
      : AsyncRetryOp<ConstantIdempotencyPolicy, Functor, AsyncSampleRowKeys>(
            error_message, std::move(rpc_retry_policy),
            // BulkMutator is idempotent because it keeps track of idempotency
            // of the mutations it holds.
            std::move(rpc_backoff_policy), ConstantIdempotencyPolicy(true),
            std::move(metadata_update_policy), std::move(callback),
            AsyncSampleRowKeys(client, std::move(app_profile_id),
                               std::move(table_name))) {}
};

}  // namespace internal
}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INTERNAL_ASYNC_SAMPLE_ROW_KEYS_H_
