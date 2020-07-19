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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INTERNAL_ASYNC_LONGRUNNING_OP_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INTERNAL_ASYNC_LONGRUNNING_OP_H

#include "google/cloud/bigtable/completion_queue.h"
#include "google/cloud/bigtable/internal/async_poll_op.h"
#include "google/cloud/bigtable/polling_policy.h"
#include "google/cloud/bigtable/version.h"
#include "google/cloud/internal/async_retry_unary_rpc.h"
#include "absl/types/optional.h"
#include <google/longrunning/operations.grpc.pb.h>
#include <sstream>

namespace google {
namespace cloud {
namespace bigtable {
inline namespace BIGTABLE_CLIENT_NS {
namespace internal {

/**
 * The operation passed to StartAsyncPollOp to implement longrunning operations.
 */
template <typename Client, typename Response>
class AsyncLongrunningOperation {
 public:
  AsyncLongrunningOperation(std::shared_ptr<Client> client,
                            google::longrunning::Operation operation)
      : client_(std::move(client)), operation_(std::move(operation)) {}

  AsyncLongrunningOperation(AsyncLongrunningOperation const&) = delete;
  AsyncLongrunningOperation(AsyncLongrunningOperation&&) noexcept = default;

  // The semantics of the value returned by the future is as follows:
  // - outer status is the attempt's status (e.g. couldn't reach CBT)
  // - the `absl::optional<>` is empty if the poll has not finished
  // - the inner StatusOr is the overall result of the longrunning operation
  //
  // The reason for such an obscure return value is that we want to use it in
  // `StartAsyncPollOp`. `StartAsyncPollOp` needs to differentiate between
  // (a) errors while trying to communicate with CBT from (b) final errors of
  // the polled operation and (c) successfully checking that the longrunning
  // operation has not yet finished. For that reason, for (a) we communicate
  // errors in the outer `StatusOr<>`, for (b) in the inner `StatusOr<>` and for
  // (c) via an empty `absl::optional<>`.
  //
  // One could argue it could be `future<StatusOr<absl::optional<Response>>>`
  // instead.  However, if the final error from the longrunning operation was a
  // retriable error, `StartAsyncPollOp` would keep querying that operation.
  // That would be incorrect, hence the extra `StatusOr<>`.
  future<StatusOr<absl::optional<StatusOr<Response>>>> operator()(
      CompletionQueue& cq, std::unique_ptr<grpc::ClientContext> context) {
    if (operation_.done()) {
      // The operation supplied in the ctor can be already completed. In such a
      // case, we shouldn't send the RPC - we already have the response.
      return make_ready_future<StatusOr<absl::optional<StatusOr<Response>>>>(
          FinalResult());
    }
    google::longrunning::GetOperationRequest request;
    request.set_name(operation_.name());
    return cq
        .MakeUnaryRpc(
            [this](grpc::ClientContext* context,
                   google::longrunning::GetOperationRequest const& request,
                   grpc::CompletionQueue* cq) {
              return client_->AsyncGetOperation(context, request, cq);
            },
            std::move(request), std::move(context))
        .then([this](future<StatusOr<google::longrunning::Operation>> fut)
                  -> StatusOr<absl::optional<StatusOr<Response>>> {
          auto res = fut.get();
          if (!res) {
            return res.status();
          }
          operation_.Swap(&(*res));
          if (!operation_.done()) {
            return absl::optional<StatusOr<Response>>();
          }
          return FinalResult();
        });
  }

 private:
  StatusOr<absl::optional<StatusOr<Response>>> FinalResult() {
    if (operation_.has_error()) {
      return absl::optional<StatusOr<Response>>(
          Status(static_cast<StatusCode>(operation_.error().code()),
                 operation_.error().message()));
    }
    Response res;
    if (!operation_.response().UnpackTo(&res)) {
      return absl::optional<StatusOr<Response>>(
          Status(StatusCode::kInternal,
                 "Longrunning operation's result didn't parse."));
    }
    return absl::optional<StatusOr<Response>>(std::move(res));
  }

  std::shared_ptr<Client> client_;
  google::longrunning::Operation operation_;
};

/**
 * Poll until a longrunning operation is complete or the polling policy
 * exhausted.
 *
 * @param location typically the name of the function that created this
 *     asynchronous polling loop.
 * @param polling_policy controls how often the server is queried.
 * @param metadata_update_policy controls how to update the metadata fields in
 *     the request.
 * @param client the client on which AsyncGetOperation is executed to get the
 *     status of the longrunning operation.
 * @param cq the completion queue where the retry loop is executed.
 * @param operation the initial state of the operation; it might already be
 *     finished, in which case the returned future will be already satisfied.
 *
 * @return a future which is satisfied when either (a) the longrunning operation
 *     completed, (b) there is a permanent error contacting the service or (c)
 *     polling was stopped because `polling_policy` was exhausted.
 */
template <typename Client, typename Response>
future<StatusOr<Response>> StartAsyncLongrunningOp(
    char const* location, std::unique_ptr<PollingPolicy> polling_policy,
    MetadataUpdatePolicy metadata_update_policy, std::shared_ptr<Client> client,
    CompletionQueue cq, google::longrunning::Operation operation) {
  return StartAsyncPollOp(location, std::move(polling_policy),
                          std::move(metadata_update_policy), std::move(cq),
                          AsyncLongrunningOperation<Client, Response>(
                              std::move(client), std::move(operation)))
      .then([](future<StatusOr<StatusOr<Response>>> fut) -> StatusOr<Response> {
        auto res = fut.get();
        if (!res) {
          // failed to get status about the longrunning operation
          return res.status();
        }
        // longrunning operation finished
        return *std::move(res);
      });
}

}  // namespace internal
}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INTERNAL_ASYNC_LONGRUNNING_OP_H
