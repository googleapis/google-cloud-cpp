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
    std::shared_ptr<grpc::Channel> channel,
    std::chrono::system_clock::time_point deadline,
    std::shared_ptr<CompletionQueueImpl> const& cq_impl)
    : channel_(std::move(channel)),
      deadline_(deadline),
      weak_cq_impl_(cq_impl) {}

void AsyncConnectionReadyFuture::Start(void* tag, grpc::CompletionQueue& cq) {
  tag_ = tag;
  auto state = channel_->GetState(true);
  if (SatisfyPromiseIfFinished(state)) return;
  // If connection was idle, GetState(true) triggered an attempt to connect.
  // If we didn't satisfy the promise yet, the connection is either in state
  // CONNECTING or TRANSIENT_FAILURE, so let's register for a state change.
  //
  // This function is supposed to be called by the completion queue inside
  // `StartOperation()`, which delays any `grpc::CompletionQueue::Shutdown`
  // calls until after this function completes, so it is safe to make the
  // following call.
  channel_->NotifyOnStateChange(state, deadline_, &cq, tag_);
}

bool AsyncConnectionReadyFuture::Notify(bool ok) {
  if (!ok) {
    promise_.set_value(
        Status(StatusCode::kDeadlineExceeded,
               "Connection couldn't connect before requested deadline"));
    return true;
  }
  if (SatisfyPromiseIfFinished(channel_->GetState(true))) return true;
  // If we call channel_->NotifyOnStateChange() on a shut down
  // `CompletionQueue`, we'll trigger an assertion in gRPC. In order to make
  // sure that the `CompletionQueue` is not shut down, we need to use
  // `StartOperation()`. We cannot use it directly here, because it will try to
  // register this operation for the second time. In order to circumvent it,
  // we'll allocate a new operation and let it continue the waiting process.
  auto cq_impl = weak_cq_impl_.lock();
  if (!cq_impl) {
    promise_.set_value(Status(StatusCode::kCancelled,
                              "Connection will never succeed because the "
                              "completion queue is being destroyed."));
    return true;
  }
  auto continuation =
      std::make_shared<AsyncConnectionReadyFuture>(std::move(*this));
  cq_impl->StartOperation(continuation, [&](void* tag) {
    continuation->Start(tag, cq_impl->cq());
  });
  return true;
}

bool AsyncConnectionReadyFuture::SatisfyPromiseIfFinished(
    grpc_connectivity_state state) {
  if (state == GRPC_CHANNEL_READY) {
    promise_.set_value(Status{});
    return true;
  }
  if (state == GRPC_CHANNEL_SHUTDOWN) {
    promise_.set_value(
        Status(StatusCode::kCancelled,
               "Connection will never succeed because it's shut down."));
    return true;
  }
  return false;
}

}  // namespace internal
}  // namespace GOOGLE_CLOUD_CPP_NS
}  // namespace cloud
}  // namespace google
