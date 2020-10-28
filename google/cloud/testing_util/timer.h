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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_TESTING_UTIL_TIMER_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_TESTING_UTIL_TIMER_H

#include "google/cloud/version.h"
#include <chrono>
#if GOOGLE_CLOUD_CPP_HAVE_GETRUSAGE
#include <sys/resource.h>
#endif  // GOOGLE_CLOUD_CPP_HAVE_GETRUSAGE

namespace google {
namespace cloud {
inline namespace GOOGLE_CLOUD_CPP_NS {
namespace testing_util {

class Timer {
 public:
  Timer() = default;

  /// Start the timer, call before the code being measured.
  void Start();

  /// Stop the timer, call after the code being measured.
  void Stop();

  //@{
  /**
   * @name Measurement results.
   *
   * @note The values are only valid after calling Start() and Stop().
   */
  std::chrono::microseconds elapsed_time() const { return elapsed_time_; }
  std::chrono::microseconds cpu_time() const { return cpu_time_; }
  std::string const& annotations() const { return annotations_; }
  //@}

  static bool SupportPerThreadUsage();

 private:
  std::chrono::steady_clock::time_point start_;
  std::chrono::microseconds elapsed_time_;
  std::chrono::microseconds cpu_time_;
  std::string annotations_;
#if GOOGLE_CLOUD_CPP_HAVE_GETRUSAGE
  struct rusage start_usage_;
#endif  // GOOGLE_CLOUD_CPP_HAVE_GETRUSAGE
};

}  // namespace testing_util
}  // namespace GOOGLE_CLOUD_CPP_NS
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_TESTING_UTIL_TIMER_H
