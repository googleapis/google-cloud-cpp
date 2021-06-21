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

namespace google {
namespace cloud {
inline namespace GOOGLE_CLOUD_CPP_NS {
namespace internal {

class AsyncPollingLoopImpl
    : public std::enable_shared_from_this<AsyncPollingLoopImpl> {
 public:
  AsyncPollingLoopImpl(google::cloud::CompletionQueue cq,
                       google::longrunning::Operation op,
                       AsyncPollLongRunningOperation poll,
                       std::unique_ptr<PollingPolicy> polling_policy,
                       std::string location)
      : cq_(std::move(cq)),
        op_(std::move(op)),
        poll_(std::move(poll)),
        polling_policy_(std::move(polling_policy)),
        location_(std::move(location)) {}

  future<StatusOr<google::longrunning::Operation>> Start() {
    if (op_.done()) {
      promise_.set_value(std::move(op_));
    } else {
      Wait();
    }
    return promise_.get_future();
  }

 private:
  std::weak_ptr<AsyncPollingLoopImpl> WeakFromThis() {
    return shared_from_this();
  }

  using TimerResult = future<StatusOr<std::chrono::system_clock::time_point>>;

  void Wait() {
    auto self = shared_from_this();
    cq_.MakeRelativeTimer(polling_policy_->WaitPeriod())
        .then([self](TimerResult f) { self->OnTimer(std::move(f)); });
  }

  void OnTimer(TimerResult f) {
    auto t = f.get();
    if (!t) return promise_.set_value(std::move(t).status());

    auto self = shared_from_this();
    google::longrunning::GetOperationRequest request;
    request.set_name(op_.name());
    poll_(cq_, absl::make_unique<grpc::ClientContext>(), request)
        .then([self](future<StatusOr<google::longrunning::Operation>> g) {
          self->OnPoll(std::move(g));
        });
  }

  void OnPoll(future<StatusOr<google::longrunning::Operation>> f) {
    auto op = f.get();
    if (op && op->done()) {
      return promise_.set_value(*std::move(op));
    }
    // Update the polling policy even on successful requests, so we can stop
    // after too many polling attempts.
    if (!polling_policy_->OnFailure(op.status())) {
      if (op) {
        return promise_.set_value(
            Status(StatusCode::kDeadlineExceeded,
                   "exhausted polling policy with no previous error from " +
                       location_));
      }
      return promise_.set_value(std::move(op).status());
    }
    if (op) op->Swap(&op_);
    Wait();
  }

  google::cloud::CompletionQueue cq_;
  google::longrunning::Operation op_;
  AsyncPollLongRunningOperation poll_;
  std::unique_ptr<PollingPolicy> polling_policy_;
  std::string location_;
  promise<StatusOr<google::longrunning::Operation>> promise_;
};

future<StatusOr<google::longrunning::Operation>> AsyncPollingLoop(
    google::cloud::CompletionQueue cq, google::longrunning::Operation op,
    AsyncPollLongRunningOperation poll,
    std::unique_ptr<PollingPolicy> polling_policy, std::string location) {
  auto loop = std::make_shared<AsyncPollingLoopImpl>(
      std::move(cq), std::move(op), std::move(poll), std::move(polling_policy),
      std::move(location));
  return loop->Start();
}

}  // namespace internal
}  // namespace GOOGLE_CLOUD_CPP_NS
}  // namespace cloud
}  // namespace google
