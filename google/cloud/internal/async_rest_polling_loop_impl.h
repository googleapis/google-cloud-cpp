// Copyright 2024 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_ASYNC_REST_POLLING_LOOP_IMPL_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_ASYNC_REST_POLLING_LOOP_IMPL_H

#include "google/cloud/completion_queue.h"
#include "google/cloud/future.h"
#include "google/cloud/internal/async_rest_polling_loop.h"
#include "google/cloud/internal/call_context.h"
#include "google/cloud/internal/make_status.h"
#include "google/cloud/internal/rest_context.h"
#include "google/cloud/log.h"
#include "google/cloud/options.h"
#include "google/cloud/status_or.h"
#include "google/cloud/version.h"
#include <chrono>
#include <functional>
#include <memory>
#include <mutex>
#include <string>

namespace google {
namespace cloud {
namespace rest_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

template <typename OperationType, typename GetOperationRequestType,
          typename CancelOperationRequestType>
class AsyncRestPollingLoopImpl
    : public std::enable_shared_from_this<AsyncRestPollingLoopImpl<
          OperationType, GetOperationRequestType, CancelOperationRequestType>> {
 public:
  AsyncRestPollingLoopImpl(
      google::cloud::CompletionQueue cq, internal::ImmutableOptions options,
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
        options_(std::move(options)),
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
      google::cloud::CompletionQueue cq, internal::ImmutableOptions options,
      AsyncRestPollLongRunningOperation<OperationType, GetOperationRequestType>
          poll,
      AsyncRestCancelLongRunningOperation<CancelOperationRequestType> cancel,
      std::unique_ptr<PollingPolicy> polling_policy, std::string location)
      : AsyncRestPollingLoopImpl(
            std::move(cq), std::move(options), poll, cancel,
            std::move(polling_policy), location,
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
        [w, c = internal::CallContext{options_}]() mutable {
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
    cancel_(cq_, std::make_unique<RestContext>(), options_, request)
        .then([w](future<Status> f) {
          if (auto self = w.lock()) self->OnCancel(f.get());
        });
  }

  void OnCancel(Status const& status) {
    GCP_LOG(DEBUG) << location_ << "() cancelled: " << status;
  }

  void OnStart(StatusOr<OperationType> op) {
    if (!op) return promise_.set_value(std::move(op));
    internal::AddSpanAttribute(*options_, "gl-cpp.LRO_name", op->name());
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
    internal::TracedAsyncBackoff(cq_, *options_, duration, "Async Backoff")
        .then([self](TimerResult f) { self->OnTimer(std::move(f)); });
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
    poll_(cq_, std::make_unique<RestContext>(), options_, request)
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
        return promise_.set_value(internal::DeadlineExceededError(
            location_ + "() - polling loop terminated by "
                        "polling policy",
            GCP_ERROR_INFO()));
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
  internal::ImmutableOptions options_;
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

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_ASYNC_REST_POLLING_LOOP_IMPL_H
