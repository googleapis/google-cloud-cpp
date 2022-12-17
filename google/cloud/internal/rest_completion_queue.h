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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_REST_COMPLETION_QUEUE_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_REST_COMPLETION_QUEUE_H

#include "google/cloud/future.h"
#include "google/cloud/status_or.h"
#include "google/cloud/version.h"
#include "absl/memory/memory.h"
#include <chrono>
#include <deque>
#include <memory>
#include <mutex>
#include <thread>

namespace google {
namespace cloud {
namespace rest_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

// This class is a placeholder that exists in the google_cloud_cpp_rest_internal
// library. This is an important distinction as the other completion queue
// components reside in libraries that are dependent on protobuf and grpc. In
// the future, this class would leverage one or more curl_multi_handles in order
// to handle executing multiple HTTP requests on a single thread, and could be
// used with libraries that actively avoid having dependencies on protobuf or
// grpc.
class RestCompletionQueue {
 public:
  enum class QueueStatus {
    kShutdown,
    kGotEvent,
    kTimeout,
  };

  // Prevents further retrieval from or mutation of the RestCompletionQueue.
  void Shutdown();

  // Attempts to get the next tag from the queue before the deadline is reached.
  // If a tag is retrieved, tag is set to the retrieved value, ok is set true,
  //   and kGotEvent is returned.
  // If the deadline is reached and no tag is available, kTimeout is returned.
  // If Shutdown has been called, tag is set to nullptr, ok is set to false,
  //    and kShutdown is returned.
  QueueStatus GetNext(void** tag, bool* ok,
                      std::chrono::system_clock::time_point deadline);
  void AddTag(void* tag);
  void RemoveTag(void* tag);
  std::size_t size() const;

 private:
  mutable std::mutex mu_;
  bool shutdown_{false};            // GUARDED_BY(mu_)
  std::deque<void*> pending_tags_;  // GUARDED_BY(mu_)
};

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace rest_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_REST_COMPLETION_QUEUE_H
