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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_TESTING_FAKE_CLOCK_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_TESTING_FAKE_CLOCK_H

#include "google/cloud/spanner/internal/clock.h"
#include <chrono>
#include <mutex>

namespace google {
namespace cloud {
namespace spanner_testing {
inline namespace SPANNER_CLIENT_NS {

/**
 * A fake clock intended for use in tests.
 *
 * The `time_point` returned from `Now()` only changes via explicit calls to
 * `SetTime` or `AdvanceTime`.
 *
 * `RealClock` should be an `internal::Clock<TrivialClock>` type that is being
 * faked - see `internal/clock.h` for details.
 */
template <typename RealClock>
class FakeClock : public RealClock {
 public:
  using time_point = typename RealClock::time_point;
  using duration = typename RealClock::duration;

  time_point Now() const override {
    std::lock_guard<std::mutex> lock(mu_);
    return now_;
  }

  /// Sets the time to `now`.
  void SetTime(time_point now) {
    std::lock_guard<std::mutex> lock(mu_);
    now_ = now;
  }
  /// Advances the time by `increment`.
  void AdvanceTime(duration increment) {
    std::lock_guard<std::mutex> lock(mu_);
    now_ += increment;
  }

 private:
  mutable std::mutex mu_;
  time_point now_;
};

using FakeSteadyClock =
    FakeClock<google::cloud::spanner::internal::SteadyClock>;

using FakeSystemClock =
    FakeClock<google::cloud::spanner::internal::SystemClock>;

}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner_testing
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_TESTING_FAKE_CLOCK_H
