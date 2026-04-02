// Copyright 2026 Google LLC
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

#include "google/cloud/internal/rest_pure_completion_queue_impl.h"
#include "google/cloud/internal/timer_queue.h"

namespace google {
namespace cloud {
namespace rest_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

RestPureCompletionQueueImpl::RestPureCompletionQueueImpl()
    : tq_(internal::TimerQueue::Create()) {}

void RestPureCompletionQueueImpl::Run() { tq_->Service(); }

void RestPureCompletionQueueImpl::Shutdown() { tq_->Shutdown(); }

void RestPureCompletionQueueImpl::CancelAll() {}

future<StatusOr<std::chrono::system_clock::time_point>>
RestPureCompletionQueueImpl::MakeDeadlineTimer(
    std::chrono::system_clock::time_point deadline) {
  return tq_->Schedule(deadline);
}

future<StatusOr<std::chrono::system_clock::time_point>>
RestPureCompletionQueueImpl::MakeRelativeTimer(
    std::chrono::nanoseconds duration) {
  using std::chrono::system_clock;
  auto d = std::chrono::duration_cast<system_clock::duration>(duration);
  if (d < duration) d += system_clock::duration{1};
  return MakeDeadlineTimer(system_clock::now() + d);
}

// Use an "immediately" expiring timer in order to get the thread(s) servicing
// the TimerQueue to execute the function. However, if the timer
// expires before .then() is invoked, the lambda will be immediately called and
// the passing of execution to the queue servicing thread will not occur.
void RestPureCompletionQueueImpl::RunAsync(
    std::unique_ptr<RunAsyncBase> function) {
  ++run_async_counter_;
  tq_->Schedule([f = std::move(function)](auto) { f->exec(); });
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace rest_internal
}  // namespace cloud
}  // namespace google
