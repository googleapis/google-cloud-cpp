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

#include "google/cloud/async_operation.h"
#include "google/cloud/future.h"
#include "google/cloud/internal/completion_queue_impl.h"
#include "google/cloud/status.h"
#include "google/cloud/version.h"
#include <grpcpp/channel.h>
#include <grpcpp/completion_queue.h>
#include <grpcpp/generic/async_generic_service.h>
#include <grpcpp/generic/generic_stub.h>
#include <grpcpp/server.h>
#include <grpcpp/server_builder.h>
#include <chrono>
#include <memory>

namespace google {
namespace cloud {
inline namespace GOOGLE_CLOUD_CPP_NS {
namespace internal {

class CompletionQueueTest;

/**
 * Underlies `DefaultCompletionQueueImpl::AsyncWaitForConnectionStateChange`.
 *
 * Objects of this class handles connection state changes events. This could
 * well be hidden away from the header, but is useful in
 * `FakeCompletionQueueImpl`.
 */
class AsyncConnectionReadyFuture : public internal::AsyncGrpcOperation {
 public:
  AsyncConnectionReadyFuture(
      std::shared_ptr<grpc::Channel> channel,
      std::chrono::system_clock::time_point deadline,
      std::shared_ptr<CompletionQueueImpl> const& cq_impl);

  void Start(void* tag, grpc::CompletionQueue& cq);
  // There doesn't seem to be a way to cancel this operation:
  // https://github.com/grpc/grpc/issues/3064
  void Cancel() override {}
  /// The future will be set to whether the state changed (false means timeout).
  future<Status> GetFuture() { return promise_.get_future(); }

 private:
  bool Notify(bool ok) override;
  bool SatisfyPromiseIfFinished(grpc_connectivity_state state);

  std::shared_ptr<grpc::Channel> channel_;
  std::chrono::system_clock::time_point deadline_;
  promise<Status> promise_;
  std::weak_ptr<internal::CompletionQueueImpl> weak_cq_impl_;
  void* tag_;

  friend class CompletionQueueTest;
};

}  // namespace internal
}  // namespace GOOGLE_CLOUD_CPP_NS
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_ASYNC_CONNECTION_READY_H
