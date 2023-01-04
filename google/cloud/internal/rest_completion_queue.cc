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

#include "google/cloud/internal/rest_completion_queue.h"
#include "google/cloud/options.h"
#include <mutex>
#include <thread>

namespace google {
namespace cloud {
namespace rest_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

void RestCompletionQueue::Shutdown() {
  if (!shutdown_) {
    std::cout << " thread=" << std::this_thread::get_id() << " "
              << __PRETTY_FUNCTION__ << std::endl;
    std::lock_guard<std::mutex> lock(mu_);
    shutdown_ = true;
    tq_.Shutdown();
  }
}

void RestCompletionQueue::CancelAll() { tq_.CancelAll(); }

future<StatusOr<std::chrono::system_clock::time_point>>
RestCompletionQueue::ScheduleTimer(std::chrono::system_clock::time_point tp) {
  return tq_.Schedule(std::move(tp));
}

void RestCompletionQueue::Service() { tq_.Service(); }

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace rest_internal
}  // namespace cloud
}  // namespace google
