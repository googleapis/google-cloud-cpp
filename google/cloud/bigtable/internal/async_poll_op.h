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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INTERNAL_ASYNC_POLL_OP_H_
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INTERNAL_ASYNC_POLL_OP_H_

#include "google/cloud/bigtable/completion_queue.h"
#include "google/cloud/bigtable/internal/async_loop_op.h"
#include "google/cloud/bigtable/internal/async_op_traits.h"
#include "google/cloud/bigtable/metadata_update_policy.h"
#include "google/cloud/bigtable/polling_policy.h"
#include "google/cloud/bigtable/version.h"
#include "google/cloud/internal/make_unique.h"

namespace google {
namespace cloud {
namespace bigtable {
inline namespace BIGTABLE_CLIENT_NS {
namespace internal {

/**
 * A dummy function object only to ease specification of pollable operations.
 *
 * It is an example type which could be passed to `Start()` member function of
 * the operation to be polled.
 */
struct PrototypePollOpStartCallback {
  void operator()(CompletionQueue&, bool finished, grpc::Status&) const {}
};

/**
 * A check if the template parameter meets criteria for `AsyncPollOp`.
 */
template <typename Operation>
struct MeetsAsyncPollOperationRequirements {
  static_assert(
      HasStart<Operation, PrototypePollOpStartCallback>::value,
      "Operation has to have a templated Start() member function "
      "instantiatable with functors like PrototypePollOpStartCallback");
  static_assert(
      HasAccumulatedResult<Operation>::value,
      "Operation has to have an AccumulatedResult() member function.");
  static_assert(
      google::cloud::internal::is_invocable<
          decltype(&Operation::template Start<PrototypePollOpStartCallback>),
          Operation&, CompletionQueue&, std::unique_ptr<grpc::ClientContext>&&,
          PrototypePollOpStartCallback&&>::value,
      "Operation::Start<PrototypePollOpStartCallback> has to be "
      "non-static and invocable with "
      "CompletionQueue&, std::unique_ptr<grpc::ClientContext>&&, "
      "PrototypePollOpStartCallback&&.");
  static_assert(google::cloud::internal::is_invocable<
                    decltype(&Operation::AccumulatedResult), Operation&>::value,
                "Operation::AccumulatedResult has to be non-static and "
                "invokable with no arguments");
  static_assert(std::is_same<google::cloud::internal::invoke_result_t<
                                 decltype(&Operation::template Start<
                                          PrototypePollOpStartCallback>),
                                 Operation&, CompletionQueue&,
                                 std::unique_ptr<grpc::ClientContext>&&,
                                 PrototypePollOpStartCallback&&>,
                             std::shared_ptr<AsyncOperation>>::value,
                "Operation::Start<>(...) has to return a "
                "std::shared_ptr<AsyncOperation>");

  using Response = google::cloud::internal::invoke_result_t<
      decltype(&Operation::AccumulatedResult), Operation&>;
};

/**
 * A wrapper for operations adding polling logic if passed to `AsyncLoopOp`.
 *
 * If used in `AsyncLoopOp`, it will asynchronously poll `Operation`
 *
 * @tparam UserFunctor the type of the function-like object that will receive
 *     the results.
 *
 * @tparam Operation a class responsible for submitting requests. Its `Start()`
 *     member function will be used for sending the retries and the original
 *     request. It follows the same scheme as AsyncRetryOp. It should satisfy
 *     `MeetsAsyncPollOperationRequirements`.
 */
template <typename UserFunctor, typename Operation>
class PollableLoopAdapter {
  using Response =
      typename MeetsAsyncPollOperationRequirements<Operation>::Response;
  static_assert(
      google::cloud::internal::is_invocable<UserFunctor, CompletionQueue&,
                                            Response&, grpc::Status&>::value,
      "UserFunctor should be callable with Operation::AccumulatedResult()'s "
      "return value.");

 public:
  explicit PollableLoopAdapter(char const* error_message,
                               std::unique_ptr<PollingPolicy> polling_policy,
                               MetadataUpdatePolicy metadata_update_policy,
                               UserFunctor callback, Operation operation)
      : error_message_(error_message),
        polling_policy_(std::move(polling_policy)),
        metadata_update_policy_(std::move(metadata_update_policy)),
        user_callback_(std::move(callback)),
        operation_(std::move(operation)) {}

  template <typename AttemptFunctor>
  std::shared_ptr<AsyncOperation> Start(
      CompletionQueue& cq, AttemptFunctor&& attempt_completed_callback) {
    auto context = google::cloud::internal::make_unique<grpc::ClientContext>();

    polling_policy_->Setup(*context);
    metadata_update_policy_.Setup(*context);

    return operation_.Start(
        cq, std::move(context),
        [this, attempt_completed_callback](CompletionQueue& cq, bool finished,
                                           grpc::Status& status) {
          OnCompletion(cq, finished, status,
                       std::move(attempt_completed_callback));
        });
  }

  std::chrono::milliseconds WaitPeriod() {
    return polling_policy_->WaitPeriod();
  }

  void Cancel(CompletionQueue& cq) {
    auto res = operation_.AccumulatedResult();
    grpc::Status res_status(
        grpc::StatusCode::CANCELLED,
        FullErrorMessageUnlocked("pending operation cancelled"));
    user_callback_(cq, res, res_status);
  }

 private:
  /// The callback to handle one asynchronous request completing.
  template <typename AttemptFunctor>
  void OnCompletion(CompletionQueue& cq, bool finished, grpc::Status& status,
                    AttemptFunctor&& attempt_completed_callback) {
    if (status.error_code() == grpc::StatusCode::CANCELLED) {
      // Cancelled, no retry necessary.
      Cancel(cq);
      attempt_completed_callback(cq, true);
      return;
    }
    if (finished) {
      // Finished, just report the result.
      auto res = operation_.AccumulatedResult();
      user_callback_(cq, res, status);
      attempt_completed_callback(cq, true);
      return;
    }
    // At this point we know the operation is not finished and not cancelled.

    // TODO(#1475) remove this hack.
    // PollingPolicy's interface doesn't allow it to make a decision on the
    // delay depending on whether there was a success or a failure. This is
    // because PollingPolicy never gets to know about a successful attempt. In
    // order to work around it in this particular class we keep the invariant
    // that a call to OnFailure() always preceeds a call to WaitPeriod(). That
    // way the policy can react differently to successful requests.
    bool const allowed_to_retry = polling_policy_->OnFailure(status);
    if (!status.ok() && !allowed_to_retry) {
      std::string full_message =
          FullErrorMessageUnlocked(polling_policy_->IsPermanentError(status)
                                       ? "permanent error"
                                       : "too many transient errors",
                                   status);
      auto res = operation_.AccumulatedResult();
      grpc::Status res_status(status.error_code(), full_message,
                              status.error_details());
      user_callback_(cq, res, res_status);
      attempt_completed_callback(cq, true);
      return;
    }
    if (polling_policy_->Exhausted()) {
      auto res = operation_.AccumulatedResult();
      grpc::Status res_status(
          grpc::StatusCode::UNKNOWN,
          FullErrorMessageUnlocked("polling policy exhausted"));
      user_callback_(cq, res, res_status);
      attempt_completed_callback(cq, true);
      return;
    }
    status_ = status;
    attempt_completed_callback(cq, false);
  }

  std::string FullErrorMessageUnlocked(char const* where) {
    std::string full_message = error_message_;
    full_message += "(" + metadata_update_policy_.value() + ") ";
    full_message += where;
    return full_message;
  }

  std::string FullErrorMessageUnlocked(char const* where,
                                       grpc::Status const& status) {
    std::string full_message = FullErrorMessageUnlocked(where);
    full_message += ", last error=";
    full_message += status.error_message();
    return full_message;
  }

  char const* error_message_;
  std::unique_ptr<PollingPolicy> polling_policy_;
  MetadataUpdatePolicy metadata_update_policy_;
  UserFunctor user_callback_;
  Operation operation_;
  grpc::Status status_;
};

/**
 * Perform asynchronous polling.
 *
 * @tparam Functor the type of the function-like object that will receive the
 *     results.
 *
 * @tparam Operation a class responsible for submitting requests. Its `Start()`
 *     member function will be used for sending the retries and the original
 *     request. It follows the same scheme as AsyncRetryOp. It should satisfy
 *     `MeetsAsyncPollOperationRequirements`.
 */
template <typename Functor, typename Operation>
class AsyncPollOp
    : public AsyncLoopOp<PollableLoopAdapter<Functor, Operation>> {
 public:
  explicit AsyncPollOp(char const* error_message,
                       std::unique_ptr<PollingPolicy> polling_policy,
                       MetadataUpdatePolicy metadata_update_policy,
                       Functor callback, Operation operation)
      : AsyncLoopOp<PollableLoopAdapter<Functor, Operation>>(
            PollableLoopAdapter<Functor, Operation>(
                error_message, std::move(polling_policy),
                metadata_update_policy, std::move(callback),
                std::move(operation))) {}
};

/// SFINAE matcher for `future<StatusOr<optional<T>>>`, false branch.
template <typename Future>
struct MatchesFutureStatusOrOptionalResponse : public std::false_type {};

/// SFINAE matcher for `future<StatusOr<optional<T>>>`, true branch.
template <typename R>
struct MatchesFutureStatusOrOptionalResponse<future<StatusOr<optional<R>>>>
    : public std::true_type {};

/// Extractor of `T` from `future<StatusOr<optional<T>>>`
template <typename T>
struct ResponseFromFutureStatusOrOptionalResponse;

template <typename T>
struct ResponseFromFutureStatusOrOptionalResponse<
    future<StatusOr<optional<T>>>> {
  using type = T;
};

/**
 * Traits for operation which can be polled via `StartAsyncPollOp`.
 *
 * It should be invocable with
 * `(CompletionQueue&, std::unique_ptr<ClientContext>)` and return a
 * `future<StatusOr<optional<T>>>`. The semantics should be:
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
      "A pollable operation should return future<StatusOr<optional<T>>>");
  /// `T`, assuming that `Operation` returns `future<StatusOr<optional<T>>>`
  using ResponseType =
      typename ResponseFromFutureStatusOrOptionalResponse<ReturnType>::type;
};

template <typename Operation>
future<
    StatusOr<typename PollableOperationRequestTraits<Operation>::ResponseType>>
StartAsyncPollOp(char const* location,
                 std::unique_ptr<PollingPolicy> polling_policy,
                 MetadataUpdatePolicy metadata_update_policy,
                 CompletionQueue cq,
                 future<StatusOr<Operation>> operation_future);

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
                    CompletionQueue cq)
      : location_(location),
        polling_policy_(std::move(polling_policy)),
        metadata_update_policy_(std::move(metadata_update_policy)),
        cq_(std::move(cq)) {}

  /// The callback for a completed request, successful or not.
  static void OnCompletion(std::shared_ptr<PollAsyncOpFuture> self,
                           StatusOr<optional<Response>> result) {
    if (result && *result) {
      self->final_result_.set_value(**std::move(result));
      return;
    }
    // TODO(#1475) remove this hack.
    // PollingPolicy's interface doesn't allow it to make a decision on the
    // delay depending on whether there was a success or a failure. This is
    // because PollingPolicy never gets to know about a successful attempt. In
    // order to work around it in this particular class we keep the invariant
    // that a call to OnFailure() always preceeds a call to WaitPeriod(). That
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
        .then([self](future<std::chrono::system_clock::time_point>) {
          StartIteration(self);
        });
  }

  /// The callback to start another iteration of the retry loop.
  static void StartIteration(std::shared_ptr<PollAsyncOpFuture> self) {
    auto context =
        ::google::cloud::internal::make_unique<grpc::ClientContext>();
    self->polling_policy_->Setup(*context);
    self->metadata_update_policy_.Setup(*context);

    (*self->operation_)(self->cq_, std::move(context))
        .then([self](future<StatusOr<optional<Response>>> fut) {
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
  // This will be filled once operation is computed.
  optional<Operation> operation_;
  promise<StatusOr<Response>> final_result_;

  friend future<StatusOr<
      typename PollableOperationRequestTraits<Operation>::ResponseType>>
  StartAsyncPollOp<Operation>(char const* location,
                              std::unique_ptr<PollingPolicy> polling_policy,
                              MetadataUpdatePolicy metadata_update_policy,
                              CompletionQueue cq,
                              future<StatusOr<Operation>> operation_future);
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
StartAsyncPollOp(char const* location,
                 std::unique_ptr<PollingPolicy> polling_policy,
                 MetadataUpdatePolicy metadata_update_policy,
                 CompletionQueue cq,
                 future<StatusOr<Operation>> operation_future) {
  using Response =
      typename PollableOperationRequestTraits<Operation>::ResponseType;
  auto req = std::shared_ptr<PollAsyncOpFuture<Operation>>(
      new PollAsyncOpFuture<Operation>(location, std::move(polling_policy),
                                       std::move(metadata_update_policy),
                                       std::move(cq)));
  return operation_future.then(
      [req](future<StatusOr<Operation>> operation_future)
          -> future<StatusOr<Response>> {
        auto operation = operation_future.get();
        if (!operation) {
          return make_ready_future<StatusOr<Response>>(operation.status());
        }
        req->operation_.emplace(*std::move(operation));
        req->StartIteration(req);
        return req->final_result_.get_future();
      });
}

}  // namespace internal
}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INTERNAL_ASYNC_POLL_OP_H_
