// Copyright 2023 Google LLC
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

#include "google/cloud/internal/rest_completion_queue_impl.h"
#include "google/cloud/internal/timer_queue.h"

namespace google {
namespace cloud {
namespace rest_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

RestCompletionQueueImpl::RestCompletionQueueImpl()
    : tq_(internal::TimerQueue::Create()) {}

void RestCompletionQueueImpl::Run() { tq_->Service(); }

void RestCompletionQueueImpl::Shutdown() { tq_->Shutdown(); }

void RestCompletionQueueImpl::CancelAll() {}

future<StatusOr<std::chrono::system_clock::time_point>>
RestCompletionQueueImpl::MakeDeadlineTimer(
    std::chrono::system_clock::time_point deadline) {
  return tq_->Schedule(deadline);
}

future<StatusOr<std::chrono::system_clock::time_point>>
RestCompletionQueueImpl::MakeRelativeTimer(std::chrono::nanoseconds duration) {
  using std::chrono::system_clock;
  auto d = std::chrono::duration_cast<system_clock::duration>(duration);
  if (d < duration) d += system_clock::duration{1};
  return MakeDeadlineTimer(system_clock::now() + d);
}

// Use an "immediately" expiring timer in order to get the thread(s) servicing
// the TimerQueue to execute the function. However, if the timer
// expires before .then() is invoked, the lambda will be immediately called and
// the passing of execution to the queue servicing thread will not occur.
void RestCompletionQueueImpl::RunAsync(
    std::unique_ptr<internal::RunAsyncBase> function) {
  ++run_async_counter_;
  tq_->Schedule([f = std::move(function)](auto) { f->exec(); });
}

void RestCompletionQueueImpl::StartOperation(
    std::shared_ptr<internal::AsyncGrpcOperation>,
    absl::FunctionRef<void(void*)>) {
  GCP_LOG(FATAL) << " function not supported.\n";
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace rest_internal
}  // namespace cloud
}  // namespace google
