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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_THREAD_ANNOTATIONS_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_THREAD_ANNOTATIONS_H

#include "google/cloud/version.h"
#include "absl/base/thread_annotations.h"
#include <mutex>

#ifdef _LIBCPP_HAS_THREAD_SAFETY_ANNOTATIONS

#define GOOGLE_CLOUD_GUARDED_BY(mu) ABSL_GUARDED_BY(mu)
#define GOOGLE_CLOUD_ACQUIRED_AFTER(...) ABSL_ACQUIRED_AFTER(__VA_ARGS__)
#define GOOGLE_CLOUD_ACQUIRED_BEFORE(...) ABSL_ACQUIRED_BEFORE(__VA_ARGS__)
#define GOOGLE_CLOUD_EXCLUSIVE_LOCKS_REQUIRED(...) ABSL_EXCLUSIVE_LOCKS_REQUIRED(__VA_ARGS__)
#define GOOGLE_CLOUD_SHARED_LOCKS_REQUIRED(...) ABSL_SHARED_LOCKS_REQUIRED(__VA_ARGS__)
#define GOOGLE_CLOUD_LOCKS_EXCLUDED(...) ABSL_LOCKS_EXCLUDED(__VA_ARGS__)
#define GOOGLE_CLOUD_NO_THREAD_SAFETY_ANALYSIS ABSL_NO_THREAD_SAFETY_ANALYSIS

#else  // _LIBCPP_HAS_THREAD_SAFETY_ANNOTATIONS

#define GOOGLE_CLOUD_GUARDED_BY(mu)
#define GOOGLE_CLOUD_ACQUIRED_AFTER(...)
#define GOOGLE_CLOUD_ACQUIRED_BEFORE(...)
#define GOOGLE_CLOUD_EXCLUSIVE_LOCKS_REQUIRED(...)
#define GOOGLE_CLOUD_SHARED_LOCKS_REQUIRED(...)
#define GOOGLE_CLOUD_LOCKS_EXCLUDED(...)
#define GOOGLE_CLOUD_NO_THREAD_SAFETY_ANALYSIS

#endif  // _LIBCPP_HAS_THREAD_SAFETY_ANNOTATIONS

namespace google {
namespace cloud {
namespace internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

// A condition_variable wait that plays nicely with thread annotations.
inline void wait(std::condition_variable& cv, std::mutex& mu) GOOGLE_CLOUD_EXCLUSIVE_LOCKS_REQUIRED(mu) {
  std::unique_lock<std::mutex> adopted{mu, std::adopt_lock};
  cv.wait(adopted);
  (void)adopted.release();
}

template <class Predicate>
void wait(std::condition_variable& cv, std::mutex& mu, Predicate&& stop_waiting) GOOGLE_CLOUD_EXCLUSIVE_LOCKS_REQUIRED(mu) {
  std::unique_lock<std::mutex> adopted{mu, std::adopt_lock};
  cv.wait(adopted, std::forward<Predicate>(stop_waiting));
  (void)adopted.release();
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_THREAD_ANNOTATIONS_H
