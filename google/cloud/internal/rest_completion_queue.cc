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
  std::lock_guard<std::mutex> lock(mu_);
  shutdown_ = true;
  std::deque<void*> pending_tags(std::move(pending_tags_));
}

RestCompletionQueue::QueueStatus RestCompletionQueue::GetNext(
    void** tag, bool* ok, std::chrono::system_clock::time_point deadline) {
  auto check_queue = [&]() mutable {
    std::lock_guard<std::mutex> lock(mu_);
    if (shutdown_) {
      *tag = nullptr;
      *ok = false;
      return QueueStatus::kShutdown;
    }

    if (pending_tags_.empty()) return QueueStatus::kTimeout;

    auto* front = std::move(pending_tags_.front());
    pending_tags_.pop_front();
    *tag = front;
    *ok = true;
    return QueueStatus::kGotEvent;
  };

  // This is a naive implementation of using the deadline to return if no tags
  // are present.
  auto initial_check = check_queue();
  if (initial_check != QueueStatus::kTimeout) return initial_check;
  std::this_thread::sleep_until(deadline);
  return check_queue();
}

void RestCompletionQueue::AddTag(void* tag) {
  std::lock_guard<std::mutex> lock(mu_);
  if (shutdown_) return;
  pending_tags_.push_back(tag);
}

void RestCompletionQueue::RemoveTag(void* tag) {
  std::lock_guard<std::mutex> lock(mu_);
  if (shutdown_) return;
  auto iter = std::find(pending_tags_.begin(), pending_tags_.end(), tag);
  if (iter == pending_tags_.end()) return;
  pending_tags_.erase(iter);
}

std::size_t RestCompletionQueue::size() const {
  std::lock_guard<std::mutex> lock(mu_);
  return pending_tags_.size();
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace rest_internal
}  // namespace cloud
}  // namespace google
