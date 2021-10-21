// Copyright 2020 Google LLC
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

#include "google/cloud/internal/async_connection_ready.h"

namespace google {
namespace cloud {
inline namespace GOOGLE_CLOUD_CPP_NS {
namespace internal {

AsyncConnectionReadyFuture::AsyncConnectionReadyFuture(
    std::shared_ptr<google::cloud::internal::CompletionQueueImpl> cq,
    std::shared_ptr<grpc::Channel> channel,
    std::chrono::system_clock::time_point deadline)
    : cq_(std::move(cq)), channel_(std::move(channel)), deadline_(deadline) {}

future<Status> AsyncConnectionReadyFuture::Start() {
  RunIteration(channel_->GetState(true));
  return promise_.get_future();
}

void AsyncConnectionReadyFuture::Notify(bool ok) {
  if (!ok) {
    promise_.set_value(
        Status(StatusCode::kDeadlineExceeded,
               "Connection couldn't connect before requested deadline"));
    return;
  }
  auto state = channel_->GetState(true);
  if (state == GRPC_CHANNEL_READY) {
    promise_.set_value(Status{});
    return;
  }
  if (state == GRPC_CHANNEL_SHUTDOWN) {
    promise_.set_value(
        Status(StatusCode::kCancelled,
               "Connection will never succeed because it's shut down."));
    return;
  }
  // If connection was idle, GetState(true) triggered an attempt to connect.
  // Otherwise it is either in state CONNECTING or TRANSIENT_FAILURE, so let's
  // register for a state change.
  RunIteration(state);
}

void AsyncConnectionReadyFuture::RunIteration(ChannelStateType state) {
  class OnStateChange : public AsyncGrpcOperation {
   public:
    explicit OnStateChange(std::shared_ptr<AsyncConnectionReadyFuture> s)
        : self_(std::move(s)) {}
    bool Notify(bool ok) override {
      self_->Notify(ok);
      return true;
    }
    void Cancel() override {}

   private:
    std::shared_ptr<AsyncConnectionReadyFuture> const self_;
  };

  auto op = std::make_shared<OnStateChange>(shared_from_this());
  cq_->StartOperation(op, [this, state](void* tag) {
    channel_->NotifyOnStateChange(state, deadline_, &cq_->cq(), tag);
  });
}

}  // namespace internal
}  // namespace GOOGLE_CLOUD_CPP_NS
}  // namespace cloud
}  // namespace google
