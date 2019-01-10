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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INTERNAL_ASYNC_RETRY_UNARY_RPC_H_
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INTERNAL_ASYNC_RETRY_UNARY_RPC_H_

#include "google/cloud/bigtable/completion_queue.h"
#include "google/cloud/bigtable/internal/async_retry_op.h"
#include "google/cloud/bigtable/metadata_update_policy.h"
#include "google/cloud/bigtable/rpc_backoff_policy.h"
#include "google/cloud/bigtable/rpc_retry_policy.h"
#include "google/cloud/internal/make_unique.h"
#include <google/protobuf/empty.pb.h>

namespace google {
namespace cloud {
namespace bigtable {
inline namespace BIGTABLE_CLIENT_NS {
namespace internal {

/**
 * A wrapper for an unary RPC bound with client and argument.
 *
 * It binds together the client its member function and its argument, so that
 * you can call it only providing the externalities. By repeatedly calling
 * `Start()` you'll be sending the same request over and over.
 *
 * @tparam Client the class implementing the asynchronous operation, examples
 *     include `DataClient`, `AdminClient`, and `InstanceAdminClient`.
 *
 * @tparam MemberFunctionType the type of the member function to call on the
 *     `Client` object. This type must meet the requirements of
 *     `internal::CheckAsyncUnaryRpcSignature`, the `AsyncRetryOp`
 *     template is disabled otherwise.
 *
 * @tparam Sig A formal parameter to discover if `MemberFunctionType` matches
 *     the required signature for an asynchronous gRPC call, and if so, what are
 *     the request and response parameter types for the RPC.
 *
 * @tparam valid_member_function_type a formal parameter, uses
 *     `std::enable_if<>` to disable this template if the member function type
 *     does not match the desired signature.
 */
template <
    typename Client, typename MemberFunctionType,
    typename Sig = internal::CheckAsyncUnaryRpcSignature<MemberFunctionType>,
    typename std::enable_if<Sig::value, int>::type valid_member_function_type =
        0>
class AsyncUnaryRpc {
 public:
  //@{
  /// @name Convenience aliases for the RPC request response and callback types.
  using Request = typename Sig::RequestType;
  using Response = typename Sig::ResponseType;
  //@}
  AsyncUnaryRpc(std::shared_ptr<Client> client,
                MemberFunctionType Client::*call, Request&& request)
      : client_(std::move(client)),
        call_(std::move(call)),
        request_(std::move(request)) {}

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
   *     once the request completes; it should accept the completion queue
   *     (passed in the `cq` param), and a `grpc::Status`.
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
        *client_, call_, request_, std::move(context),
        [this, callback](CompletionQueue& cq, Response& response,
                         grpc::Status& status) {
          response_ = std::move(response);
          callback(cq, status);
        });
  }

  Response AccumulatedResult() { return response_; }

  std::shared_ptr<Client> client_;
  MemberFunctionType Client::*call_;
  Request request_;
  Response response_;
};

template <
    typename Client, typename MemberFunctionType, typename IdempotencyPolicy,
    typename Functor,
    typename Sig = internal::CheckAsyncUnaryRpcSignature<MemberFunctionType>,
    typename std::enable_if<Sig::value, int>::type valid_member_function_type =
        0,
    typename std::enable_if<
        google::cloud::internal::is_invocable<Functor, CompletionQueue&,
                                              typename Sig::ResponseType&,
                                              grpc::Status&>::value,
        int>::type valid_callback_type = 0>
class AsyncRetryUnaryRpc
    : public AsyncRetryOp<IdempotencyPolicy, Functor,
                          AsyncUnaryRpc<Client, MemberFunctionType>> {
 public:
  using Request = typename Sig::RequestType;
  using Response = typename Sig::ResponseType;
  explicit AsyncRetryUnaryRpc(
      char const* error_message,
      std::unique_ptr<RPCRetryPolicy> rpc_retry_policy,
      std::unique_ptr<RPCBackoffPolicy> rpc_backoff_policy,
      IdempotencyPolicy idempotent_policy,
      MetadataUpdatePolicy metadata_update_policy,
      std::shared_ptr<Client> client, MemberFunctionType Client::*call,
      Request&& request, Functor&& callback)
      : AsyncRetryOp<IdempotencyPolicy, Functor,
                     AsyncUnaryRpc<Client, MemberFunctionType>>(
            error_message, std::move(rpc_retry_policy),
            std::move(rpc_backoff_policy), std::move(idempotent_policy),
            std::move(metadata_update_policy), std::forward<Functor>(callback),
            AsyncUnaryRpc<Client, MemberFunctionType>(
                std::move(client), std::move(call), std::move(request))) {}
};

/**
 * A wrapper to eliminate `google::protobuf::Empty` response from Async APIs.
 */
template <typename Functor,
          typename std::enable_if<
              google::cloud::internal::is_invocable<Functor, CompletionQueue&,
                                                    grpc::Status&>::value,
              int>::type valid_callback_type = 0>
class EmptyResponseAdaptor {
 public:
  explicit EmptyResponseAdaptor(Functor&& callback)
      : callback_(std::move(callback)) {}

  const void operator()(CompletionQueue& cq, google::protobuf::Empty& response,
                        grpc::Status const& status) const {
    callback_(cq, status);
    return;
  }

 private:
  Functor callback_;
};

}  // namespace internal
}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INTERNAL_ASYNC_RETRY_UNARY_RPC_H_
