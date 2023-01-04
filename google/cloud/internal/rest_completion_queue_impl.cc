// Copyright 2022 Google LLC
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
#include "google/cloud/internal/algorithm.h"
#include "google/cloud/internal/rest_completion_queue.h"
#include "google/cloud/internal/throw_delegate.h"
#include "google/cloud/options.h"
#include "absl/memory/memory.h"
#include <sstream>

namespace google {
namespace cloud {
namespace rest_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

RestCompletionQueueImpl::RestCompletionQueueImpl()
    : shutdown_guard_(
          // Capturing `this` here is safe because the lifetime of copies of
          // this member do not outlive `StartOperation`.
          std::shared_ptr<void>(reinterpret_cast<void*>(this),
                                [this](void*) { cq_.Shutdown(); })) {}

void RestCompletionQueueImpl::RunStart() {
  std::lock_guard<std::mutex> lk(mu_);
  ++thread_pool_size_;
  thread_pool_hwm_ = (std::max)(thread_pool_hwm_, thread_pool_size_);
}

void RestCompletionQueueImpl::RunStop() {
  std::lock_guard<std::mutex> lk(mu_);
  --thread_pool_size_;
}

void RestCompletionQueueImpl::Run() {
  class ThreadPoolCount {
   public:
    explicit ThreadPoolCount(RestCompletionQueueImpl* self) : self_(self) {
      self_->RunStart();
    }
    ~ThreadPoolCount() { self_->RunStop(); }

   private:
    RestCompletionQueueImpl* self_;
  } count(this);

  cq_.Service();
}

void RestCompletionQueueImpl::Shutdown() {
  std::cout << " thread=" << std::this_thread::get_id() << " "
            << __PRETTY_FUNCTION__ << std::endl;
  {
    std::lock_guard<std::mutex> lk(mu_);
    cq_.Shutdown();
    shutdown_ = true;
    shutdown_guard_.reset();
  }
}

void RestCompletionQueueImpl::CancelAll() { cq_.CancelAll(); }

future<StatusOr<std::chrono::system_clock::time_point>>
RestCompletionQueueImpl::MakeDeadlineTimer(
    std::chrono::system_clock::time_point deadline) {
  return cq_.ScheduleTimer(deadline);
}

future<StatusOr<std::chrono::system_clock::time_point>>
RestCompletionQueueImpl::MakeRelativeTimer(std::chrono::nanoseconds duration) {
  using std::chrono::system_clock;
  auto const d = std::chrono::duration_cast<system_clock::duration>(duration);
  return MakeDeadlineTimer(system_clock::now() + d);
}

// Use an immediately expiring timer in order to get the thread(s) servicing
// the RestCompletionQueue to execute the function.
void RestCompletionQueueImpl::RunAsync(
    std::unique_ptr<internal::RunAsyncBase> function) {
  ++run_async_counter_;
  std::cout << __func__ << " thread=" << std::this_thread::get_id()
            << std::endl;
  MakeRelativeTimer(std::chrono::seconds())
      .then([f = std::move(function)](auto) { f->exec(); });
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace rest_internal
}  // namespace cloud
}  // namespace google
