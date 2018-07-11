// Copyright 2018 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_TESTING_UTIL_CHECK_PREDICATE_BECOMES_FALSE_H_
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_TESTING_UTIL_CHECK_PREDICATE_BECOMES_FALSE_H_

#include "google/cloud/version.h"
#include <gmock/gmock.h>
#include <chrono>
#include <thread>

namespace google {
namespace cloud {
inline namespace GOOGLE_CLOUD_CPP_NS {
namespace testing_util {
/**
 * Verify that an assertion becomes false after some prescribed time.
 *
 * Test that an assertion is initially true, but eventually becomes false after
 * some prescribed time.  Use tolerance to avoid creating flaky tests.
 */
template <typename Predicate>
void CheckPredicateBecomesFalse(Predicate&& predicate,
                                std::chrono::system_clock::time_point deadline,
                                std::chrono::milliseconds tolerance) {
  // This is one of those tests that can get annoyingly flaky, it is based on
  // time.  Basically we want to know that the policy will accept failures
  // until around its prescribed deadline (50ms in this test).  Instead of
  // measuring for *exactly* 50ms, we pass the test if:
  //   - All calls to OnFailure() in the first 50ms - 10ms pass.
  //   - Calls to OnFailure() after 50ms + 10ms are rejected.
  //   - We do not care about the results from 40ms to 60ms.
  //   - We must handle the case where the test is called before the 40ms mark
  //     and finishes after the 60ms mark. Yes, that can happen on heavily
  //     loaded machines, which CI servers often are.
  // I know 10ms feels like a long time, but it is not on a loaded VM running
  // the tests inside some container.
  auto must_be_true_before = deadline - tolerance;
  auto must_be_false_after = deadline + tolerance;
  int true_count = 0;
  int false_count = 0;
  auto duration = deadline - std::chrono::system_clock::now();
  while (std::chrono::system_clock::now() < deadline + duration) {
    auto iteration_start = std::chrono::system_clock::now();
    auto actual = predicate();
    auto iteration_end = std::chrono::system_clock::now();
    if (iteration_end < must_be_true_before) {
      EXPECT_TRUE(actual);
      if (actual) {
        ++true_count;
      }
    } else if (must_be_false_after < iteration_start) {
      EXPECT_FALSE(actual);
      if (not actual) {
        // Terminate the loop early if we can.
        ++false_count;
        break;
      }
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(tolerance / 2));
  }
  EXPECT_LE(0, true_count);
  EXPECT_LE(0, false_count);
}

}  // namespace testing_util
}  // namespace GOOGLE_CLOUD_CPP_NS
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_TESTING_UTIL_CHECK_PREDICATE_BECOMES_FALSE_H_
