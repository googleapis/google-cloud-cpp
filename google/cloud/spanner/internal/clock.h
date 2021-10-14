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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_INTERNAL_CLOCK_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_INTERNAL_CLOCK_H

#include "google/cloud/spanner/version.h"
#include <chrono>

namespace google {
namespace cloud {
namespace spanner_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

/**
 * A simple `Clock` class that can be overridden for testing.
 *
 * All implementations of this class are required to be thread-safe.
 *
 * The template type `TrivialClock` must meet the C++ named requirements for
 * `TrivialClock` (for example, clocks from `std::chrono`).
 */
template <typename TrivialClock>
class Clock {
 public:
  using time_point = typename TrivialClock::time_point;
  using duration = typename TrivialClock::duration;

  virtual ~Clock() = default;
  virtual time_point Now() const { return TrivialClock::now(); }
};

/**
 * `SteadyClock` is a monotonic clock where time points cannot decrease as
 * physical time moves forward. It is not related to wall clock time.
 */
using SteadyClock =
    ::google::cloud::spanner_internal::Clock<std::chrono::steady_clock>;

/**
 * `SystemClock` represents the system-wide real time wall clock.
 * It may not be monotonic.
 */
using SystemClock =
    ::google::cloud::spanner_internal::Clock<std::chrono::system_clock>;

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace spanner_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_INTERNAL_CLOCK_H
