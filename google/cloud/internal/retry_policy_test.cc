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
#include "google/cloud/testing_util/check_predicate_becomes_false.h"
#include <gmock/gmock.h>

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
 */
void CheckLimitedTime(RetryPolicyForTest& tested) {
  google::cloud::testing_util::CheckPredicateBecomesFalse(
      [&tested] { return tested.OnFailure(CreateTransientError()); },
      std::chrono::system_clock::now() + kLimitedTimeTestPeriod,
      kLimitedTimeTolerance);
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
