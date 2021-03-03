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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INTERNAL_ASYNC_POLL_OP_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INTERNAL_ASYNC_POLL_OP_H

#include "google/cloud/bigtable/completion_queue.h"
#include "google/cloud/bigtable/metadata_update_policy.h"
#include "google/cloud/bigtable/polling_policy.h"
#include "google/cloud/bigtable/version.h"
#include "absl/memory/memory.h"
#include "absl/types/optional.h"
#include <chrono>
#include <string>

namespace google {
namespace cloud {
namespace bigtable {
inline namespace BIGTABLE_CLIENT_NS {
namespace internal {

/// SFINAE matcher for `future<StatusOr<absl::optional<T>>>`, false branch.
template <typename Future>
struct MatchesFutureStatusOrOptionalResponse : public std::false_type {};

/// SFINAE matcher for `future<StatusOr<absl::optional<T>>>`, true branch.
template <typename R>
struct MatchesFutureStatusOrOptionalResponse<
    future<StatusOr<absl::optional<R>>>> : public std::true_type {};

/// Extractor of `T` from `future<StatusOr<absl::optional<T>>>`
template <typename T>
struct ResponseFromFutureStatusOrOptionalResponse;

template <typename T>
struct ResponseFromFutureStatusOrOptionalResponse<
    future<StatusOr<absl::optional<T>>>> {
  using type = T;
};

/**
 * Traits for operation which can be polled via `StartAsyncPollOp`.
 *
 * It should be invocable with
 * `(CompletionQueue&, std::unique_ptr<ClientContext>)` and return a
 * `future<StatusOr<absl::optional<T>>>`. The semantics should be:
 *       - on error return a non-ok status,
 *       - on successfully checking that the polled operation is not finished,
 *         return an empty option
 *       - on finished poll return the polled value
 */
template <typename Operation>
struct PollableOperationRequestTraits {
  static_assert(::google::cloud::internal::is_invocable<
                    Operation, CompletionQueue&,
                    std::unique_ptr<grpc::ClientContext>>::value,
                "A pollable operation needs to be invocable with "
                "(CompletionQueue&, std::unique_ptr<grpc::ClientContext>)");
  /// The type of what `Operation` returns.
  using ReturnType = typename google::cloud::internal::invoke_result_t<
      Operation, CompletionQueue&, std::unique_ptr<grpc::ClientContext>>;
  static_assert(
      MatchesFutureStatusOrOptionalResponse<ReturnType>::value,
      "A pollable operation should return future<StatusOr<absl::optional<T>>>");
  /// `T`, assuming `Operation` returns `future<StatusOr<absl::optional<T>>>`
  using ResponseType =
      typename ResponseFromFutureStatusOrOptionalResponse<ReturnType>::type;
};

template <typename Operation>
future<
    StatusOr<typename PollableOperationRequestTraits<Operation>::ResponseType>>
StartAsyncPollOp(char const* location,
                 std::unique_ptr<PollingPolicy> polling_policy,
                 MetadataUpdatePolicy metadata_update_policy,
                 CompletionQueue cq, Operation operation);

/**
 * The state machine created by StartAsyncPollOp().
 */
template <typename Operation>
class PollAsyncOpFuture {
 private:
  /// Convenience alias for the value we are polling.
  using Response =
      typename PollableOperationRequestTraits<Operation>::ResponseType;

  // The constructor is private because we always want to wrap the object in
  // a shared pointer. The lifetime is controlled by any pending operations in
  // the CompletionQueue.
  PollAsyncOpFuture(char const* location,
                    std::unique_ptr<PollingPolicy> polling_policy,
                    MetadataUpdatePolicy metadata_update_policy,
                    CompletionQueue cq, Operation operation)
      : location_(location),
        polling_policy_(std::move(polling_policy)),
        metadata_update_policy_(std::move(metadata_update_policy)),
        cq_(std::move(cq)),
        operation_(std::move(operation)) {}

  /// The callback for a completed request, successful or not.
  static void OnCompletion(std::shared_ptr<PollAsyncOpFuture> self,
                           StatusOr<absl::optional<Response>> result) {
    if (result && *result) {
      self->final_result_.set_value(**std::move(result));
      return;
    }
    // TODO(#1475) remove this hack.
    // PollingPolicy's interface doesn't allow it to make a decision on the
    // delay depending on whether there was a success or a failure. This is
    // because PollingPolicy never gets to know about a successful attempt. In
    // order to work around it in this particular class we keep the invariant
    // that a call to OnFailure() always precedes a call to WaitPeriod(). That
    // way the policy can react differently to successful requests.
    bool const allowed_to_retry =
        self->polling_policy_->OnFailure(result.status());
    if (!result && !allowed_to_retry) {
      self->final_result_.set_value(self->DetailedStatus(
          self->polling_policy_->IsPermanentError(result.status())
              ? "permanent error"
              : "too many transient errors",
          result.status()));
      return;
    }
    if (self->polling_policy_->Exhausted()) {
      self->final_result_.set_value(self->DetailedStatus(
          "polling policy exhausted", Status(StatusCode::kUnknown, "")));
      return;
    }
    self->cq_.MakeRelativeTimer(self->polling_policy_->WaitPeriod())
        .then([self](future<StatusOr<std::chrono::system_clock::time_point>>
                         result) {
          if (auto tp = result.get()) {
            StartIteration(self);
          } else {
            self->final_result_.set_value(
                self->DetailedStatus("timer error", tp.status()));
          }
        });
  }

  /// The callback to start another iteration of the retry loop.
  static void StartIteration(std::shared_ptr<PollAsyncOpFuture> self) {
    auto context = absl::make_unique<grpc::ClientContext>();
    self->polling_policy_->Setup(*context);
    self->metadata_update_policy_.Setup(*context);

    self->operation_(self->cq_, std::move(context))
        .then([self](future<StatusOr<absl::optional<Response>>> fut) {
          OnCompletion(self, fut.get());
        });
  }

  /// Generate an error message
  Status DetailedStatus(char const* context, Status const& status) {
    std::string full_message = location_;
    full_message += "(" + metadata_update_policy_.value() + ") ";
    full_message += context;
    full_message += ", last error=";
    full_message += status.message();
    return Status(status.code(), std::move(full_message));
  }

  char const* location_;
  std::unique_ptr<PollingPolicy> polling_policy_;
  MetadataUpdatePolicy metadata_update_policy_;
  CompletionQueue cq_;
  Operation operation_;
  promise<StatusOr<Response>> final_result_;

  friend future<StatusOr<
      typename PollableOperationRequestTraits<Operation>::ResponseType>>
  StartAsyncPollOp<Operation>(char const* location,
                              std::unique_ptr<PollingPolicy> polling_policy,
                              MetadataUpdatePolicy metadata_update_policy,
                              CompletionQueue cq, Operation operation);
};

/**
 * Start the asynchronous retry loop.
 *
 * @param location typically the name of the function that created this
 *     asynchronous retry loop.
 * @param polling_policy controls how often the server is queried
 * @param metadata_update_policy controls how to update the metadata fields in
 *     the request.
 * @param cq the completion queue where the retry loop is executed.
 * @param operation the operation to poll; refer to
 *     `PollableOperationRequestTraits` for requirements.
 * @return a future that becomes satisfied when (a) the service responds with an
 *     indication that the poll is finished, or (b) one of the poll attempts
 *     fails with a non-retryable error, or (d) the polling policy is expired.
 */
template <typename Operation>
future<
    StatusOr<typename PollableOperationRequestTraits<Operation>::ResponseType>>
StartAsyncPollOp(
    char const* location, std::unique_ptr<PollingPolicy> polling_policy,
    // NOLINTNEXTLINE(performance-unnecessary-value-param) TODO(#4112)
    MetadataUpdatePolicy metadata_update_policy,
    // NOLINTNEXTLINE(performance-unnecessary-value-param) TODO(#4112)
    CompletionQueue cq, Operation operation) {
  auto req = std::shared_ptr<PollAsyncOpFuture<Operation>>(
      new PollAsyncOpFuture<Operation>(location, std::move(polling_policy),
                                       std::move(metadata_update_policy),
                                       std::move(cq), std::move(operation)));
  req->StartIteration(req);
  return req->final_result_.get_future();
}

}  // namespace internal
}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INTERNAL_ASYNC_POLL_OP_H
