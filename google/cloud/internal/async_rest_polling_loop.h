// Copyright 2023 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_ASYNC_REST_POLLING_LOOP_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_ASYNC_REST_POLLING_LOOP_H

#include "google/cloud/completion_queue.h"
#include "google/cloud/future.h"
#include "google/cloud/internal/call_context.h"
#include "google/cloud/internal/grpc_opentelemetry.h"
#include "google/cloud/internal/rest_context.h"
#include "google/cloud/log.h"
#include "google/cloud/options.h"
#include "google/cloud/polling_policy.h"
#include "google/cloud/status_or.h"
#include "google/cloud/version.h"
#include <google/longrunning/operations.pb.h>
#include <chrono>
#include <functional>
#include <memory>
#include <mutex>
#include <string>

namespace google {
namespace cloud {
namespace rest_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

template <typename OperationType = google::longrunning::Operation,
          typename GetOperationRequestType =
              google::longrunning::GetOperationRequest>
using AsyncRestPollLongRunningOperation =
    std::function<future<StatusOr<OperationType>>(
        google::cloud::CompletionQueue&, std::unique_ptr<RestContext>,
        GetOperationRequestType const&)>;

template <typename CancelOperationRequestType =
              google::longrunning::CancelOperationRequest>
using AsyncRestCancelLongRunningOperation = std::function<future<Status>(
    google::cloud::CompletionQueue&, std::unique_ptr<RestContext>,
    CancelOperationRequestType const&)>;

template <typename OperationType, typename GetOperationRequestType,
          typename CancelOperationRequestType>
class AsyncRestPollingLoopImpl;

/**
 * Runs an asynchronous polling loop for a long-running operation.
 *
 * Long-running operations [aip/151] are used for API methods that take a
 * significant amount of time to complete (think minutes, maybe an hour). The
 * API returns a "promise" object, represented by the
 * `google::longrunning::Operation` proto, and the application (or client
 * library) should periodically poll this object until it is "satisfied".
 *
 * This function runs an asynchronous loop to poll the long-running operation.
 * It periodically invokes the @p poll function to query the status of the
 * operation. If the operation has completed (even if completed with an error)
 * the loop stops. If the operation has not completed, or polling fails (say
 * because the network has a problem), the function sets an asynchronous timer
 * as configured by the polling policy and tries again later. The polling policy
 * can stop the loop too.
 *
 * The function returns a `future<>` that is satisfied when the loop stops. In
 * short, the returned future is satisfied under any of these conditions (a) the
 * polling policy is exhausted before it is known if the operation completed
 * successfully, or (b) the operation completes, and this is known because a
 * `GetOperation()` request returns the operation result.
 *
 * The promise can complete with an error, which is represented by a
 * `google::cloud::Status` object, or with success and some `ReturnType` value.
 * The application may also configure the "polling policy", which may stop the
 * polling even though the operation has not completed.
 *
 * Usually the initial request to "start" the long-running operation also
 * requires a retry loop, which is handled by this function too.
 *
 * Typically, library developers would use the function via
 * `AsyncLongRunningOperation`, but as a stand-alone function it can be used in
 * this context. First assume there is a `*Stub` class as follows:
 *
 * @code
 * class BarRestStub {
 *  public:
 *   virtual future<StatusOr<google::longrunning::Operation>> AsyncGetOperation(
 *     google::cloud::CompletionQueue& cq,
 *     std::unique_ptr<rest_internal::RestContext> context,
 *     google::longrunning::GetOperationRequest const& request) = 0;
 *   virtual future<Status> AsyncCancelOperation(
 *     google::cloud::CompletionQueue& cq,
 *     std::unique_ptr<rest_internal::RestContext> context,
 *     google::longrunning::CancelOperationRequest const& request) = 0;
 * };
 * @endcode
 *
 * As part of implementing a long-running operation one would do something like:
 *
 * @code
 * class BarRestConnectionImpl : public BarConnection {
 *  public:
 *   // Using C++14 for exposition purposes. The implementation supports C++11.
 *   future<StatusOr<FooResponse>> Foo(FooRequest const& request) override {
 *     using ::google::longrunning::Operation;
 *     future<StatusOr<Operation>> op = AsyncStart();
 *
 *     return op.then([stub = stub_, cq = cq_, loc = __func__](auto f) {
 *       StatusOr<Operation> op = f.get();
 *       if (!op) return make_ready_future(op);
 *       return AsyncPollingLoop(
 *           std::move(cq), *std::move(op),
 *           [stub](auto cq, auto context, auto const& r) {
 *             return stub->AsyncGetOperation(cq, std::move(context), r);
 *           },
 *           [stub](auto cq, auto context, auto const& r) {
 *             return stub->AsyncCancelOperation(cq, std::move(context), r);
 *           },
 *           polling_policy_->clone(), loc);
 *        });
 *   }
 *
 *  private:
 *    google::cloud::CompletionQueue cq_;
 *    std::shared_ptr<BarStub> stub_;
 * };
 * @endcode
 *
 * [aip/151]: https://google.aip.dev/151
 */
future<StatusOr<google::longrunning::Operation>> AsyncRestPollingLoop(
    google::cloud::CompletionQueue cq,
    future<StatusOr<google::longrunning::Operation>> op,
    AsyncRestPollLongRunningOperation<> poll,
    AsyncRestCancelLongRunningOperation<> cancel,
    std::unique_ptr<PollingPolicy> polling_policy, std::string location);

template <typename OperationType = google::longrunning::Operation,
          typename GetOperationRequestType =
              google::longrunning::CancelOperationRequest,
          typename CancelOperationRequestType =
              google::longrunning::CancelOperationRequest>
future<StatusOr<OperationType>> AsyncRestPollingLoop(
    google::cloud::CompletionQueue cq, future<StatusOr<OperationType>> op,
    AsyncRestPollLongRunningOperation<OperationType, GetOperationRequestType>
        poll,
    AsyncRestCancelLongRunningOperation<CancelOperationRequestType> cancel,
    std::unique_ptr<PollingPolicy> polling_policy, std::string location,
    std::function<bool(OperationType const&)> is_operation_done,
    std::function<void(std::string const&, GetOperationRequestType&)>
        get_request_set_operation_name,
    std::function<void(std::string const&, CancelOperationRequestType&)>
        cancel_request_set_operation_name) {
  auto loop = std::make_shared<AsyncRestPollingLoopImpl<
      OperationType, GetOperationRequestType, CancelOperationRequestType>>(
      std::move(cq), std::move(poll), std::move(cancel),
      std::move(polling_policy), std::move(location), is_operation_done,
      get_request_set_operation_name, cancel_request_set_operation_name);
  return loop->Start(std::move(op));
}

template <typename OperationType = google::longrunning::Operation,
          typename GetOperationRequestType =
              google::longrunning::GetOperationRequest,
          typename CancelOperationRequestType =
              google::longrunning::CancelOperationRequest>
class AsyncRestPollingLoopImpl
    : public std::enable_shared_from_this<AsyncRestPollingLoopImpl<
          OperationType, GetOperationRequestType, CancelOperationRequestType>> {
 public:
  AsyncRestPollingLoopImpl(
      google::cloud::CompletionQueue cq,
      AsyncRestPollLongRunningOperation<OperationType, GetOperationRequestType>
          poll,
      AsyncRestCancelLongRunningOperation<CancelOperationRequestType> cancel,
      std::unique_ptr<PollingPolicy> polling_policy, std::string location,
      std::function<bool(OperationType const&)> is_operation_done,
      std::function<void(std::string const&, GetOperationRequestType&)>
          get_request_set_operation_name,
      std::function<void(std::string const&, CancelOperationRequestType&)>
          cancel_request_set_operation_name)
      : cq_(std::move(cq)),
        poll_(std::move(poll)),
        cancel_(std::move(cancel)),
        polling_policy_(std::move(polling_policy)),
        location_(std::move(location)),
        promise_(null_promise_t{}),
        is_operation_done_(std::move(is_operation_done)),
        get_request_set_operation_name_(
            std::move(get_request_set_operation_name)),
        cancel_request_set_operation_name_(
            std::move(cancel_request_set_operation_name)) {}

  AsyncRestPollingLoopImpl(
      google::cloud::CompletionQueue cq,
      AsyncRestPollLongRunningOperation<OperationType, GetOperationRequestType>
          poll,
      AsyncRestCancelLongRunningOperation<CancelOperationRequestType> cancel,
      std::unique_ptr<PollingPolicy> polling_policy, std::string location)
      : AsyncRestPollingLoopImpl(
            std::move(cq), poll, cancel, std::move(polling_policy), location,
            [](google::longrunning::Operation const& op) { return op.done(); },
            [](std::string const& name,
               google::longrunning::GetOperationRequest& r) {
              r.set_name(name);
            },
            [](std::string const& name,
               google::longrunning::CancelOperationRequest& r) {
              r.set_name(name);
            }) {}

  future<StatusOr<OperationType>> Start(future<StatusOr<OperationType>> op) {
    auto self = this->shared_from_this();
    auto w = WeakFromThis();
    promise_ = promise<StatusOr<OperationType>>(
        [w, c = internal::CallContext{}]() mutable {
          if (auto self = w.lock()) {
            internal::ScopedCallContext scope(std::move(c));
            self->DoCancel();
          }
        });
    op.then(
        [self](future<StatusOr<OperationType>> f) { self->OnStart(f.get()); });
    return promise_.get_future();
  }

 private:
  using TimerResult = future<StatusOr<std::chrono::system_clock::time_point>>;

  std::weak_ptr<AsyncRestPollingLoopImpl> WeakFromThis() {
    return this->shared_from_this();
  }

  void DoCancel() {
    CancelOperationRequestType request;
    {
      std::unique_lock<std::mutex> lk(mu_);
      if (op_name_.empty()) {
        delayed_cancel_ = true;  // Wait for OnStart() to set `op_name_`.
        return;
      }
      cancel_request_set_operation_name_(op_name_, request);
    }
    // Cancels are best effort, so we use weak pointers.
    auto w = WeakFromThis();
    cancel_(cq_, std::make_unique<RestContext>(), request)
        .then([w](future<Status> f) {
          if (auto self = w.lock()) self->OnCancel(f.get());
        });
  }

  void OnCancel(Status const& status) {
    GCP_LOG(DEBUG) << location_ << "() cancelled: " << status;
  }

  void OnStart(StatusOr<OperationType> op) {
    if (!op) return promise_.set_value(std::move(op));
    internal::AddSpanAttribute("gcloud.LRO_name", op->name());
    if (is_operation_done_(*op)) return promise_.set_value(std::move(op));
    GCP_LOG(DEBUG) << location_ << "() polling loop starting for "
                   << op->name();
    bool do_cancel = false;
    {
      std::unique_lock<std::mutex> lk(mu_);
      std::swap(delayed_cancel_, do_cancel);
      op_name_ = std::move(*op->mutable_name());
    }
    if (do_cancel) DoCancel();
    return Wait();
  }

  void Wait() {
    std::chrono::milliseconds duration = polling_policy_->WaitPeriod();
    GCP_LOG(DEBUG) << location_ << "() polling loop waiting "
                   << duration.count() << "ms";
    auto self = this->shared_from_this();
    internal::TracedAsyncBackoff(cq_, duration).then([self](TimerResult f) {
      self->OnTimer(std::move(f));
    });
  }

  void OnTimer(TimerResult f) {
    GCP_LOG(DEBUG) << location_ << "() polling loop awakened";
    auto t = f.get();
    if (!t) return promise_.set_value(std::move(t).status());
    GetOperationRequestType request;
    {
      std::unique_lock<std::mutex> lk(mu_);
      get_request_set_operation_name_(op_name_, request);
    }
    auto self = this->shared_from_this();
    poll_(cq_, std::make_unique<RestContext>(), request)
        .then([self](future<StatusOr<OperationType>> g) {
          self->OnPoll(std::move(g));
        });
  }

  void OnPoll(future<StatusOr<OperationType>> f) {
    GCP_LOG(DEBUG) << location_ << "() polling loop result";
    auto op = f.get();
    if (op && is_operation_done_(*op)) {
      return promise_.set_value(*std::move(op));
    }
    // Update the polling policy even on successful requests, so we can stop
    // after too many polling attempts.
    if (!polling_policy_->OnFailure(op.status())) {
      if (op) {
        // We should not be fabricating a `Status` value here. Rather, we
        // should cancel the operation and wait for the next poll to return
        // an accurate status to the user, otherwise they will have no idea
        // how to react. But for now, we leave the operation running. It
        // may eventually complete.
        return promise_.set_value(Status(
            StatusCode::kDeadlineExceeded,
            location_ + "() - polling loop terminated by polling policy"));
      }
      // This could be a transient error if the policy is exhausted.
      return promise_.set_value(std::move(op).status());
    }
    return Wait();
  }

  // These member variables are initialized in the constructor or from
  // `Start()`, and then only used from the `On*()` callbacks, which are
  // serialized, so they need no external synchronization.
  google::cloud::CompletionQueue cq_;
  AsyncRestPollLongRunningOperation<OperationType, GetOperationRequestType>
      poll_;
  AsyncRestCancelLongRunningOperation<CancelOperationRequestType> cancel_;
  std::unique_ptr<PollingPolicy> polling_policy_;
  std::string location_;
  promise<StatusOr<OperationType>> promise_;
  std::function<bool(OperationType const&)> is_operation_done_;
  std::function<void(std::string const&, GetOperationRequestType&)>
      get_request_set_operation_name_;
  std::function<void(std::string const&, CancelOperationRequestType&)>
      cancel_request_set_operation_name_;

  // `delayed_cancel_` and `op_name_`, in contrast, are also used from
  // `DoCancel()`, which is called asynchronously, so they need locking.
  std::mutex mu_;
  bool delayed_cancel_ = false;  // GUARDED_BY(mu_)
  std::string op_name_;          // GUARDED_BY(mu_)
};

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace rest_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_ASYNC_REST_POLLING_LOOP_H
