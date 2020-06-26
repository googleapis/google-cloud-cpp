// Copyright 2020 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_TESTING_UTIL_SCOPED_THREAD_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_TESTING_UTIL_SCOPED_THREAD_H

#include "google/cloud/version.h"
#include <thread>

namespace google {
namespace cloud {
inline namespace GOOGLE_CLOUD_CPP_NS {
namespace testing_util {

/**
 * A simple wrapper around std::thread that automatically joins the thread
 * (if needed) on destruction.
 */
class ScopedThread {
 public:
  /// call the `std::thread()` constructor with the given `args`
  template <class... Args>
  explicit ScopedThread(Args&&... args) : t_(std::forward<Args>(args)...) {}
  ~ScopedThread() {
    if (t_.joinable()) t_.join();
  }

  /// Access the owned `std::thread`
  std::thread& get() { return t_; }

 private:
  std::thread t_;
};

}  // namespace testing_util
}  // namespace GOOGLE_CLOUD_CPP_NS
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_TESTING_UTIL_SCOPED_THREAD_H
