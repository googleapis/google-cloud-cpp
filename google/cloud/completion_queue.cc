// Copyright 2018 Google LLC
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

#include "google/cloud/completion_queue.h"
#include "google/cloud/internal/async_connection_ready.h"
#include "google/cloud/internal/default_completion_queue_impl.h"

namespace google {
namespace cloud {
inline namespace GOOGLE_CLOUD_CPP_NS {

CompletionQueue::CompletionQueue()
    : impl_(new internal::DefaultCompletionQueueImpl) {}

future<Status> CompletionQueue::AsyncWaitConnectionReady(
    std::shared_ptr<grpc::Channel> channel,
    std::chrono::system_clock::time_point deadline) {
  auto op = std::make_shared<internal::AsyncConnectionReadyFuture>(
      impl_, std::move(channel), deadline);
  return op->Start();
}

}  // namespace GOOGLE_CLOUD_CPP_NS
}  // namespace cloud
}  // namespace google
