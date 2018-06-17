// Copyright 2018 Google LLC
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

#include "google/cloud/internal/retry_policy.h"
#include <gmock/gmock.h>
#include <thread>

namespace {
struct Status {
  bool is_retryable;
  bool is_ok;
};
struct IsRetryablePolicy {
  static bool IsPermanentFailure(Status const& s) {
    return s.is_ok or not s.is_retryable;
  }
};

Status CreateTransientError() { return Status{true, false}; }
Status CreatePermanentError() { return Status{false, false}; }

using RetryPolicyForTest =
    google::cloud::internal::RetryPolicy<Status, IsRetryablePolicy>;
using LimitedTimeRetryPolicyForTest =
    google::cloud::internal::LimitedTimeRetryPolicy<Status, IsRetryablePolicy>;
using LimitedErrorCountRetryPolicyForTest =
    google::cloud::internal::LimitedErrorCountRetryPolicy<Status,
                                                          IsRetryablePolicy>;

auto const kLimitedTimeTestPeriod = std::chrono::milliseconds(50);
auto const kLimitedTimeTolerance = std::chrono::milliseconds(10);

/**
 * @test Verify that a retry policy configured to run for 50ms works correctly.
 *
 * This eliminates some amount of code duplication in the following tests.
 *
 * TODO(#733) - refactor this and the implementations in bigtable/
 */
void CheckLimitedTime(RetryPolicyForTest& tested) {
  auto start = std::chrono::system_clock::now();
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
  auto must_be_true_before =
      start + kLimitedTimeTestPeriod - kLimitedTimeTolerance;
  auto must_be_false_after =
      start + kLimitedTimeTestPeriod + kLimitedTimeTolerance;
  int true_count = 0;
  int false_count = 0;
  for (int i = 0; i != 100; ++i) {
    auto iteration_start = std::chrono::system_clock::now();
    auto actual = tested.OnFailure(CreateTransientError());
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
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
  }
  EXPECT_LE(0, true_count);
  EXPECT_LE(0, false_count);
}

}  // anonymous namespace

/// @test A simple test for the LimitedTimeRetryPolicy.
TEST(LimitedTimeRetryPolicy, Simple) {
  LimitedTimeRetryPolicyForTest tested(kLimitedTimeTestPeriod);
  CheckLimitedTime(tested);
}

/// @test Test cloning for LimitedTimeRetryPolicy.
TEST(LimitedTimeRetryPolicy, Clone) {
  LimitedTimeRetryPolicyForTest original(kLimitedTimeTestPeriod);
  auto cloned = original.clone();
  CheckLimitedTime(*cloned);
}

/// @test Verify that non-retryable errors cause an immediate failure.
TEST(LimitedTimeRetryPolicy, OnNonRetryable) {
  LimitedTimeRetryPolicyForTest tested(std::chrono::milliseconds(10));
  EXPECT_FALSE(tested.OnFailure(CreatePermanentError()));
}

/// @test A simple test for the LimitedErrorCountRetryPolicy.
TEST(LimitedErrorCountRetryPolicy, Simple) {
  LimitedErrorCountRetryPolicyForTest tested(3);
  EXPECT_TRUE(tested.OnFailure(CreateTransientError()));
  EXPECT_TRUE(tested.OnFailure(CreateTransientError()));
  EXPECT_TRUE(tested.OnFailure(CreateTransientError()));
  EXPECT_FALSE(tested.OnFailure(CreateTransientError()));
  EXPECT_FALSE(tested.OnFailure(CreateTransientError()));
}

/// @test Test cloning for LimitedErrorCountRetryPolicy.
TEST(LimitedErrorCountRetryPolicy, Clone) {
  LimitedErrorCountRetryPolicyForTest original(3);
  auto tested = original.clone();
  EXPECT_TRUE(tested->OnFailure(CreateTransientError()));
  EXPECT_TRUE(tested->OnFailure(CreateTransientError()));
  EXPECT_TRUE(tested->OnFailure(CreateTransientError()));
  EXPECT_FALSE(tested->OnFailure(CreateTransientError()));
  EXPECT_FALSE(tested->OnFailure(CreateTransientError()));
}

/// @test Verify that non-retryable errors cause an immediate failure.
TEST(LimitedErrorCountRetryPolicy, OnNonRetryable) {
  LimitedErrorCountRetryPolicyForTest tested(3);
  EXPECT_FALSE(tested.OnFailure(CreatePermanentError()));
}
