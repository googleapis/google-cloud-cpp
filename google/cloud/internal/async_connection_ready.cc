// Copyright 2020 Google LLC
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

#include "google/cloud/internal/async_connection_ready.h"
#include "google/cloud/internal/make_status.h"
#include "google/cloud/options.h"

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {

AsyncConnectionReadyFuture::AsyncConnectionReadyFuture(
    std::shared_ptr<google::cloud::internal::CompletionQueueImpl> cq,
    std::shared_ptr<grpc::Channel> channel,
    std::chrono::system_clock::time_point deadline)
    : cq_(std::move(cq)), channel_(std::move(channel)), deadline_(deadline) {}

future<Status> AsyncConnectionReadyFuture::Start() {
  RunIteration();
  return promise_.get_future();
}

void AsyncConnectionReadyFuture::Notify(bool ok) {
  if (!ok) {
    promise_.set_value(internal::DeadlineExceededError(
        "Connection couldn't connect before requested deadline",
        GCP_ERROR_INFO()));
    return;
  }
  auto state = channel_->GetState(true);
  if (state == GRPC_CHANNEL_READY) {
    promise_.set_value(Status{});
    return;
  }
  if (state == GRPC_CHANNEL_SHUTDOWN) {
    promise_.set_value(internal::CancelledError(
        "Connection will never succeed because it's shut down.",
        GCP_ERROR_INFO()));
    return;
  }
  // If connection was idle, GetState(true) triggered an attempt to connect.
  // Otherwise, it is either in state CONNECTING or TRANSIENT_FAILURE, so let's
  // register for a state change.
  RunIteration();
}

void AsyncConnectionReadyFuture::RunIteration() {
  // If the connection is ready, we do not need to wait for a state change.
  auto state = channel_->GetState(true);
  if (state == GRPC_CHANNEL_READY) {
    promise_.set_value(Status{});
    return;
  }
  NotifyOnStateChange::Start(cq_, channel_, deadline_, state)
      .then([s = shared_from_this(), c = CallContext{}](auto f) {
        ScopedCallContext scope(std::move(c));
        s->Notify(f.get());
      });
}

future<bool> NotifyOnStateChange::Start(
    std::shared_ptr<CompletionQueueImpl> cq,
    std::shared_ptr<grpc::Channel> channel,
    std::chrono::system_clock::time_point deadline) {
  auto last_observed = channel->GetState(true);
  return Start(std::move(cq), std::move(channel), deadline, last_observed);
}

future<bool> NotifyOnStateChange::Start(
    std::shared_ptr<CompletionQueueImpl> cq,
    std::shared_ptr<grpc::Channel> channel,
    std::chrono::system_clock::time_point deadline,
    grpc_connectivity_state last_observed) {
  auto op = std::make_shared<NotifyOnStateChange>();
  cq->StartOperation(op, [&](void* tag) {
    channel->NotifyOnStateChange(last_observed, deadline, cq->cq(), tag);
  });
  return op->promise_.get_future();
}

bool NotifyOnStateChange::Notify(bool ok) {
  ScopedCallContext scope(call_context_);
  promise_.set_value(ok);
  return true;
}

}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google
