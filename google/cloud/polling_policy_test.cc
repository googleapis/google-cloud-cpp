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

#include "google/cloud/polling_policy.h"
#include "google/cloud/internal/retry_policy_impl.h"
#include "google/cloud/testing_util/check_predicate_becomes_false.h"
#include <gtest/gtest.h>
#include <chrono>

namespace google {
namespace cloud {
namespace {

struct TestRetryablePolicy {
  static bool IsPermanentFailure(google::cloud::Status const& s) {
    return !s.ok() &&
           (s.code() == google::cloud::StatusCode::kPermissionDenied);
  }
};

Status TransientError() { return Status(StatusCode::kUnavailable, ""); }
Status PermanentError() { return Status(StatusCode::kPermissionDenied, ""); }

using ms = std::chrono::milliseconds;
using RetryPolicyForTest =
    ::google::cloud::internal::TraitBasedRetryPolicy<TestRetryablePolicy>;
using LimitedTimeRetryPolicyForTest =
    ::google::cloud::internal::LimitedTimeRetryPolicy<TestRetryablePolicy>;
using LimitedErrorCountRetryPolicyForTest =
    ::google::cloud::internal::LimitedErrorCountRetryPolicy<
        TestRetryablePolicy>;

auto const kLimitedTimeTestPeriod = ms(100);
auto const kLimitedTimeTolerance = ms(20);

/**
 * @test Verify that a polling policy configured to run for 50ms works
 * correctly.
 */
void CheckLimitedTime(PollingPolicy& tested, char const* where) {
  google::cloud::testing_util::CheckPredicateBecomesFalse(
      [&tested] { return tested.OnFailure(TransientError()); },
      std::chrono::system_clock::now() + kLimitedTimeTestPeriod,
      kLimitedTimeTolerance, where);
}

/// @test A simple test for the GenericPollingPolicy.
TEST(GenericPollingPolicy, Simple) {
  LimitedTimeRetryPolicyForTest retry(kLimitedTimeTestPeriod);
  internal::ExponentialBackoffPolicy backoff(ms(10), ms(100), 2.0);
  GenericPollingPolicy<LimitedTimeRetryPolicyForTest,
                       internal::ExponentialBackoffPolicy>
      tested(retry, backoff);
  CheckLimitedTime(tested, __func__);
  auto delay = tested.WaitPeriod();
  EXPECT_LE(ms(10), delay);
  EXPECT_GE(ms(20), delay);
}

/// @test Ensure that we accept shared_ptr types.
TEST(GenericPollingPolicy, SimpleWithPointers) {
  LimitedTimeRetryPolicyForTest retry(kLimitedTimeTestPeriod);
  internal::ExponentialBackoffPolicy backoff(ms(10), ms(100), 2.0);
  GenericPollingPolicy<std::shared_ptr<RetryPolicy>,
                       std::shared_ptr<internal::BackoffPolicy>>
      tested(retry.clone(), backoff.clone());
  CheckLimitedTime(tested, __func__);
  auto delay = tested.WaitPeriod();
  EXPECT_LE(ms(10), delay);
  EXPECT_GE(ms(20), delay);
}

/// @test Test cloning for GenericPollingPolicy.
TEST(GenericPollingPolicy, Clone) {
  LimitedErrorCountRetryPolicyForTest retry(1);
  internal::ExponentialBackoffPolicy backoff(ms(10), ms(100), 2.0);
  GenericPollingPolicy<LimitedErrorCountRetryPolicyForTest,
                       internal::ExponentialBackoffPolicy>
      original(retry, backoff);
  EXPECT_TRUE(original.OnFailure(TransientError()));
  EXPECT_FALSE(original.OnFailure(TransientError()));
  auto delay = original.WaitPeriod();
  EXPECT_LE(ms(10), delay);
  EXPECT_GE(ms(20), delay);

  // Ensure the initial state of the policy is cloned, not the current state.
  auto clone = original.clone();
  EXPECT_TRUE(clone->OnFailure(TransientError()));
  EXPECT_FALSE(clone->OnFailure(TransientError()));
  delay = clone->WaitPeriod();
  EXPECT_LE(ms(10), delay);
  EXPECT_GE(ms(20), delay);
}

/// @test Test cloning for GenericPollingPolicy, with shared_ptr inputs.
TEST(GenericPollingPolicy, CloneWithPointers) {
  LimitedErrorCountRetryPolicyForTest retry(1);
  internal::ExponentialBackoffPolicy backoff(ms(10), ms(100), 2.0);
  GenericPollingPolicy<std::shared_ptr<google::cloud::RetryPolicy>,
                       std::shared_ptr<internal::BackoffPolicy>>
      original(retry.clone(), backoff.clone());
  EXPECT_TRUE(original.OnFailure(TransientError()));
  EXPECT_FALSE(original.OnFailure(TransientError()));
  auto delay = original.WaitPeriod();
  EXPECT_LE(ms(10), delay);
  EXPECT_GE(ms(20), delay);

  // Ensure the initial state of the policy is cloned, not the current state.
  auto clone = original.clone();
  EXPECT_TRUE(clone->OnFailure(TransientError()));
  EXPECT_FALSE(clone->OnFailure(TransientError()));
  delay = clone->WaitPeriod();
  EXPECT_LE(ms(10), delay);
  EXPECT_GE(ms(20), delay);
}

/// @test Verify that non-retryable errors cause an immediate failure.
TEST(GenericPollingPolicy, OnNonRetryable) {
  LimitedTimeRetryPolicyForTest retry(kLimitedTimeTestPeriod);
  internal::ExponentialBackoffPolicy backoff(ms(10), ms(100), 2.0);
  GenericPollingPolicy<LimitedTimeRetryPolicyForTest,
                       internal::ExponentialBackoffPolicy>
      tested(retry, backoff);
  EXPECT_FALSE(tested.OnFailure(PermanentError()));
}

/// @test Verify that the backoff policy's wait period is used.
TEST(GenericPollingPolicy, WaitPeriod) {
  LimitedTimeRetryPolicyForTest retry(kLimitedTimeTestPeriod);
  internal::ExponentialBackoffPolicy backoff(ms(10), ms(100), 2.0);
  GenericPollingPolicy<LimitedTimeRetryPolicyForTest,
                       internal::ExponentialBackoffPolicy>
      tested(retry, backoff);
  auto delay = tested.WaitPeriod();
  EXPECT_LE(ms(10), delay);
  EXPECT_GE(ms(20), delay);
  delay = tested.WaitPeriod();
  EXPECT_LE(ms(20), delay);
  EXPECT_GE(ms(40), delay);
}

}  // namespace
}  // namespace cloud
}  // namespace google
