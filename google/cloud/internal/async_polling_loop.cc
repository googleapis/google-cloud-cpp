// Copyright 2021 Google LLC
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

#include "google/cloud/internal/async_polling_loop.h"
#include <mutex>
#include <string>

namespace google {
namespace cloud {
inline namespace GOOGLE_CLOUD_CPP_NS {
namespace internal {

using ::google::longrunning::Operation;

class AsyncPollingLoopImpl
    : public std::enable_shared_from_this<AsyncPollingLoopImpl> {
 public:
  AsyncPollingLoopImpl(google::cloud::CompletionQueue cq,
                       AsyncPollLongRunningOperation poll,
                       AsyncCancelLongRunningOperation cancel,
                       std::unique_ptr<PollingPolicy> polling_policy,
                       std::string location)
      : cq_(std::move(cq)),
        poll_(std::move(poll)),
        cancel_(std::move(cancel)),
        polling_policy_(std::move(polling_policy)),
        location_(std::move(location)),
        promise_(null_promise_t{}),
        canceled_(false) {}

  future<StatusOr<Operation>> Start(future<StatusOr<Operation>> op) {
    auto self = shared_from_this();
    auto w = WeakFromThis();
    promise_ = promise<StatusOr<Operation>>([w] {
      if (auto self = w.lock()) self->DoCancel();
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
      if (op_name_.empty()) return;
      request.set_name(op_name_);
    }
    // Cancels are best effort, so we use weak pointers.
    auto w = WeakFromThis();
    cancel_(cq_, absl::make_unique<grpc::ClientContext>(), request)
        .then([w](future<Status> f) {
          if (auto self = w.lock()) self->OnCancel(f.get());
        });
  }

  void OnCancel(Status const& status) {
    if (!status.ok()) return;
    std::unique_lock<std::mutex> lk(mu_);
    canceled_ = true;
  }

  void OnStart(StatusOr<Operation> op) {
    if (!op || op->done()) return promise_.set_value(std::move(op));
    {
      std::unique_lock<std::mutex> lk(mu_);
      if (canceled_) return Cancelled();
      op_name_ = std::move(*op->mutable_name());
    }
    Wait();
  }

  void Cancelled() {
    promise_.set_value(
        Status{StatusCode::kCancelled,
               location_ + "() - polling loop terminated via cancel()"});
  }

  void Wait() {
    auto self = shared_from_this();
    cq_.MakeRelativeTimer(polling_policy_->WaitPeriod())
        .then([self](TimerResult f) { self->OnTimer(std::move(f)); });
  }

  void OnTimer(TimerResult f) {
    auto t = f.get();
    if (!t) return promise_.set_value(std::move(t).status());
    google::longrunning::GetOperationRequest request;
    {
      std::unique_lock<std::mutex> lk(mu_);
      if (canceled_) return Cancelled();
      request.set_name(op_name_);
    }
    auto self = shared_from_this();
    poll_(cq_, absl::make_unique<grpc::ClientContext>(), request)
        .then([self](future<StatusOr<Operation>> g) {
          self->OnPoll(std::move(g));
        });
  }

  void OnPoll(future<StatusOr<Operation>> f) {
    auto op = f.get();
    if (op && op->done()) {
      return promise_.set_value(*std::move(op));
    }
    // Update the polling policy even on successful requests, so we can stop
    // after too many polling attempts.
    if (!polling_policy_->OnFailure(op.status())) {
      if (op) {
        return promise_.set_value(Status(
            StatusCode::kDeadlineExceeded,
            location_ + "() - polling loop terminated by polling policy"));
      }
      return promise_.set_value(std::move(op).status());
    }
    if (op) {
      std::unique_lock<std::mutex> lk(mu_);
      if (canceled_) return Cancelled();
      op_name_ = std::move(*op->mutable_name());
    }
    Wait();
  }

  // These member variables are initialized in the constructor or from
  // `Start()`, and then only used from the `On*()` callbacks, which are
  // serialized, so they need no external synchronization.
  google::cloud::CompletionQueue cq_;
  AsyncPollLongRunningOperation poll_;
  AsyncCancelLongRunningOperation cancel_;
  std::unique_ptr<PollingPolicy> polling_policy_;
  std::string location_;
  promise<StatusOr<Operation>> promise_;

  // `canceled_` and `op_name_`, in contrast, are also used from `DoCancel()`,
  // which is called asynchronously, so they require synchronization.
  std::mutex mu_;
  bool canceled_;        // GUARDED_BY(mu_)
  std::string op_name_;  // GUARDED_BY(mu_)
};

future<StatusOr<Operation>> AsyncPollingLoop(
    google::cloud::CompletionQueue cq, future<StatusOr<Operation>> op,
    AsyncPollLongRunningOperation poll, AsyncCancelLongRunningOperation cancel,
    std::unique_ptr<PollingPolicy> polling_policy, std::string location) {
  auto loop = std::make_shared<AsyncPollingLoopImpl>(
      std::move(cq), std::move(poll), std::move(cancel),
      std::move(polling_policy), std::move(location));
  return loop->Start(std::move(op));
}

}  // namespace internal
}  // namespace GOOGLE_CLOUD_CPP_NS
}  // namespace cloud
}  // namespace google
