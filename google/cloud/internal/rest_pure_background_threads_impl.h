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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_REST_PURE_BACKGROUND_THREADS_IMPL_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_REST_PURE_BACKGROUND_THREADS_IMPL_H

#include "google/cloud/internal/generic_background_threads_impl.h"
#include "google/cloud/internal/rest_pure_completion_queue_impl.h"
#include "google/cloud/version.h"
#include <thread>
#include <vector>

namespace google {
namespace cloud {
namespace rest_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

/// This interface mimics `google::cloud::BackgroundThreads` except it returns
/// a `RestPureCompletionQueue` instead of a `CompletionQueue`. This is
/// important as `CompletionQueue` has gRPC dependencies we need to avoid in a
/// pure REST library.
class RestPureBackgroundThreads {
 public:
  virtual ~RestPureBackgroundThreads() = default;

  /// The completion queue used for the background operations.
  virtual RestPureCompletionQueue cq() const = 0;
};

/// Background threads that run on a RestPureCompletionQueue.
using AutomaticallyCreatedRestPureBackgroundThreads =
    google::cloud::internal::AutomaticallyCreatedBackgroundThreadsImpl<
        RestPureCompletionQueue, RestPureBackgroundThreads>;

/// Background threads that run on a RestPureCompletionQueue.
using AutomaticallyCreatedRestPureBackgroundThreads =
    google::cloud::internal::AutomaticallyCreatedBackgroundThreadsImpl<
        RestPureCompletionQueue, RestPureBackgroundThreads,
        RestPureQueueTraits>;

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace rest_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_REST_PURE_BACKGROUND_THREADS_IMPL_H
