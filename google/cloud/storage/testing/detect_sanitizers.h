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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_TESTING_DETECT_SANITIZERS_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_TESTING_DETECT_SANITIZERS_H

// Sanitizers introduce massive CPU and thread-synchronization overhead, causing
// multi-threaded GCS integration tests to scale exponentially in execution
// duration. On highly parallel multi-core machines, this extreme CPU starvation
// blocks GCS emulator (Gunicorn) workers, triggering worker timeouts and
// crashing the emulator. We scale down test concurrency (thread/object counts)
// under sanitizers to keep execution fast and prevent emulator crashes.
// See: https://github.com/googleapis/google-cloud-cpp/issues/16430
#if defined(__SANITIZE_ADDRESS__) || defined(__SANITIZE_THREAD__) || \
    defined(__SANITIZE_MEMORY__) || defined(__SANITIZE_UNDEFINED__)
#  define GCS_TEST_HAVE_SANITIZER 1
#elif defined(__has_feature)
#  if __has_feature(address_sanitizer) || __has_feature(thread_sanitizer) || \
      __has_feature(memory_sanitizer) || \
      __has_feature(undefined_behavior_sanitizer)
#    define GCS_TEST_HAVE_SANITIZER 1
#  endif
#endif

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_TESTING_DETECT_SANITIZERS_H
