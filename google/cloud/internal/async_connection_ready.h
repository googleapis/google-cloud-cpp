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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_ASYNC_CONNECTION_READY_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_ASYNC_CONNECTION_READY_H

#include "google/cloud/future.h"
#include "google/cloud/internal/completion_queue_impl.h"
#include "google/cloud/status.h"
#include "google/cloud/version.h"
#include <grpcpp/channel.h>
#include <chrono>
#include <memory>

namespace google {
namespace cloud {
inline namespace GOOGLE_CLOUD_CPP_NS {
namespace internal {

/**
 * Underlies `DefaultCompletionQueueImpl::AsyncWaitForConnectionStateChange`.
 *
 * Objects of this class handles connection state changes events. This could
 * well be hidden away from the header, but is useful in
 * `FakeCompletionQueueImpl`.
 */
class AsyncConnectionReadyFuture
    : public std::enable_shared_from_this<AsyncConnectionReadyFuture> {
 public:
  AsyncConnectionReadyFuture(
      std::shared_ptr<google::cloud::internal::CompletionQueueImpl> cq,
      std::shared_ptr<grpc::Channel> channel,
      std::chrono::system_clock::time_point deadline);

  future<Status> Start();

 private:
  void Notify(bool ok);

  // gRPC uses an anonymous type for the gRPC channel state enum :shrug:.
  using ChannelStateType = decltype(GRPC_CHANNEL_READY);
  void RunIteration(ChannelStateType state);

  std::shared_ptr<google::cloud::internal::CompletionQueueImpl> const cq_;
  std::shared_ptr<grpc::Channel> const channel_;
  std::chrono::system_clock::time_point const deadline_;
  promise<Status> promise_;
};

}  // namespace internal
}  // namespace GOOGLE_CLOUD_CPP_NS
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_ASYNC_CONNECTION_READY_H
