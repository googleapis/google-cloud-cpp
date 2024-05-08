// Copyright 2021 Google LLC
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

#include "google/cloud/internal/async_polling_loop.h"
#include "google/cloud/grpc_options.h"
#include "google/cloud/internal/call_context.h"
#include "google/cloud/internal/grpc_opentelemetry.h"
#include "google/cloud/log.h"
#include <algorithm>
#include <mutex>
#include <string>

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {

using ::google::longrunning::Operation;

class AsyncPollingLoopImpl
    : public std::enable_shared_from_this<AsyncPollingLoopImpl> {
 public:
  AsyncPollingLoopImpl(google::cloud::CompletionQueue cq,
                       ImmutableOptions options,
                       AsyncPollLongRunningOperation poll,
                       AsyncCancelLongRunningOperation cancel,
                       std::unique_ptr<PollingPolicy> polling_policy,
                       std::string location)
      : cq_(std::move(cq)),
        options_(std::move(options)),
        poll_(std::move(poll)),
        cancel_(std::move(cancel)),
        polling_policy_(std::move(polling_policy)),
        location_(std::move(location)),
        promise_(null_promise_t{}) {}

  future<StatusOr<Operation>> Start(future<StatusOr<Operation>> op) {
    auto self = shared_from_this();
    auto w = WeakFromThis();
    promise_ = promise<StatusOr<Operation>>([w, c = CallContext{}]() mutable {
      if (auto self = w.lock()) {
        ScopedCallContext scope(std::move(c));
        self->DoCancel();
      }
    });
    op.then([self](future<StatusOr<Operation>> f) { self->OnStart(f.get()); });
    return promise_.get_future();
  }

 private:
  using TimerResult = future<StatusOr<std::chrono::system_clock::time_point>>;

  std::weak_ptr<AsyncPollingLoopImpl> WeakFromThis() {
    return shared_from_this();
  }

  void DoCancel() {
    google::longrunning::CancelOperationRequest request;
    {
      std::unique_lock<std::mutex> lk(mu_);
      if (op_name_.empty()) {
        delayed_cancel_ = true;  // Wait for OnStart() to set `op_name_`.
        return;
      }
      request.set_name(op_name_);
    }
    // Cancels are best effort, so we use weak pointers.
    auto w = WeakFromThis();
    auto context = std::make_shared<grpc::ClientContext>();
    ConfigurePollContext(*context, *options_);
    cancel_(cq_, std::move(context), options_, request)
        .then([w](future<Status> f) {
          if (auto self = w.lock()) self->OnCancel(f.get());
        });
  }

  void OnCancel(Status const& status) {
    GCP_LOG(DEBUG) << location_ << "() cancelled: " << status;
  }

  void OnStart(StatusOr<Operation> op) {
    if (!op) {
      promise_.set_value(std::move(op));
      return;
    }
    AddSpanAttribute(*options_, "gl-cpp.LRO_name", op->name());
    if (op->done()) {
      promise_.set_value(std::move(op));
      return;
    }
    GCP_LOG(DEBUG) << location_ << "() polling loop starting for "
                   << op->name();
    bool do_cancel = false;
    {
      std::unique_lock<std::mutex> lk(mu_);
      std::swap(delayed_cancel_, do_cancel);
      op_name_ = std::move(*op->mutable_name());
    }
    if (do_cancel) DoCancel();
    Wait();
  }

  void Wait() {
    std::chrono::milliseconds duration = polling_policy_->WaitPeriod();
    GCP_LOG(DEBUG) << location_ << "() polling loop waiting "
                   << duration.count() << "ms";
    auto self = shared_from_this();
    TracedAsyncBackoff(cq_, *options_, duration, "Async Backoff")
        .then([self](TimerResult f) { self->OnTimer(std::move(f)); });
  }

  void OnTimer(TimerResult f) {
    GCP_LOG(DEBUG) << location_ << "() polling loop awakened";
    auto t = f.get();
    if (!t) {
      promise_.set_value(std::move(t).status());
      return;
    }
    google::longrunning::GetOperationRequest request;
    {
      std::unique_lock<std::mutex> lk(mu_);
      request.set_name(op_name_);
    }
    auto self = shared_from_this();
    auto context = std::make_shared<grpc::ClientContext>();
    ConfigurePollContext(*context, *options_);
    poll_(cq_, std::move(context), options_, request)
        .then([self](future<StatusOr<Operation>> g) {
          self->OnPoll(std::move(g));
        });
  }

  void OnPoll(future<StatusOr<Operation>> f) {
    GCP_LOG(DEBUG) << location_ << "() polling loop result";
    auto op = f.get();
    if (op && op->done()) {
      promise_.set_value(*std::move(op));
      return;
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
        promise_.set_value(Status(
            StatusCode::kDeadlineExceeded,
            location_ + "() - polling loop terminated by polling policy"));
        return;
      }
      // This could be a transient error if the policy is exhausted.
      promise_.set_value(std::move(op).status());
      return;
    }
    Wait();
  }

  // These member variables are initialized in the constructor or from
  // `Start()`, and then only used from the `On*()` callbacks, which are
  // serialized, so they need no external synchronization.
  google::cloud::CompletionQueue cq_;
  ImmutableOptions options_;
  AsyncPollLongRunningOperation poll_;
  AsyncCancelLongRunningOperation cancel_;
  std::unique_ptr<PollingPolicy> polling_policy_;
  std::string location_;
  promise<StatusOr<Operation>> promise_;

  // `delayed_cancel_` and `op_name_`, in contrast, are also used from
  // `DoCancel()`, which is called asynchronously, so they need locking.
  std::mutex mu_;
  bool delayed_cancel_ = false;  // GUARDED_BY(mu_)
  std::string op_name_;          // GUARDED_BY(mu_)
};

future<StatusOr<Operation>> AsyncPollingLoop(
    google::cloud::CompletionQueue cq, ImmutableOptions options,
    future<StatusOr<Operation>> op, AsyncPollLongRunningOperation poll,
    AsyncCancelLongRunningOperation cancel,
    std::unique_ptr<PollingPolicy> polling_policy, std::string location) {
  auto loop = std::make_shared<AsyncPollingLoopImpl>(
      std::move(cq), std::move(options), std::move(poll), std::move(cancel),
      std::move(polling_policy), std::move(location));
  return loop->Start(std::move(op));
}

}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google
