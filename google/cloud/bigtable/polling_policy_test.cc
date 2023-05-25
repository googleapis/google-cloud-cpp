// Copyright 2018 Google LLC
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

#include "google/cloud/bigtable/polling_policy.h"
#include "google/cloud/bigtable/rpc_retry_policy.h"
#include "google/cloud/grpc_error_delegate.h"
#include "google/cloud/status.h"
#include "google/cloud/testing_util/check_predicate_becomes_false.h"
#include "google/cloud/testing_util/chrono_literals.h"
#include <gtest/gtest.h>
#include <chrono>
#include <thread>

namespace google {
namespace cloud {
namespace bigtable {
namespace {

Status PermanentError() {
  return Status(StatusCode::kFailedPrecondition, "failed");
}

grpc::Status GrpcPermanentError() {
  return grpc::Status(grpc::StatusCode::FAILED_PRECONDITION, "failed");
}

Status TransientError() {
  return Status(StatusCode::kUnavailable, "try again");
}

grpc::Status GrpcTransientError() {
  return grpc::Status(grpc::StatusCode::UNAVAILABLE, "try again");
}

using testing_util::chrono_literals::operator"" _ms;  // NOLINT
auto const kLimitedTimeTestPeriod = 100_ms;
auto const kLimitedTimeTolerance = 20_ms;

/**
 * @test Verify that a polling policy configured to run for 50ms
 * works correctly.
 */
void CheckLimitedTime(PollingPolicy& tested) {
  testing_util::CheckPredicateBecomesFalse(
      [&tested] { return tested.OnFailure(GrpcTransientError()); },
      std::chrono::system_clock::now() + kLimitedTimeTestPeriod,
      kLimitedTimeTolerance);
}

void CheckLimitedTime(std::unique_ptr<google::cloud::PollingPolicy> common) {
  testing_util::CheckPredicateBecomesFalse(
      [&common] { return common->OnFailure(TransientError()); },
      std::chrono::system_clock::now() + kLimitedTimeTestPeriod,
      kLimitedTimeTolerance);
}

/// @test A simple test for the GenericPollingPolicy.
TEST(GenericPollingPolicy, Simple) {
  LimitedTimeRetryPolicy retry(kLimitedTimeTestPeriod);
  ExponentialBackoffPolicy backoff(10_ms, 50_ms);
  GenericPollingPolicy<> tested(retry, backoff);
  CheckLimitedTime(tested);

  auto common = bigtable_internal::MakeCommonPollingPolicy(tested.clone());
  CheckLimitedTime(std::move(common));
}

/// @test Test cloning for GenericPollingPolicy.
TEST(GenericPollingPolicy, Clone) {
  LimitedErrorCountRetryPolicy retry(1);
  ExponentialBackoffPolicy backoff(10_ms, 50_ms);
  GenericPollingPolicy<LimitedErrorCountRetryPolicy> original(retry, backoff);
  EXPECT_TRUE(original.OnFailure(TransientError()));
  EXPECT_FALSE(original.OnFailure(TransientError()));
  EXPECT_GE(10_ms, original.WaitPeriod());
  EXPECT_LE(10_ms, original.WaitPeriod());

  // Ensure the initial state of the policy is cloned, not the current state.
  auto clone = original.clone();
  EXPECT_TRUE(clone->OnFailure(TransientError()));
  EXPECT_FALSE(clone->OnFailure(TransientError()));
  EXPECT_GE(10_ms, clone->WaitPeriod());
  EXPECT_LE(10_ms, clone->WaitPeriod());

  auto common = bigtable_internal::MakeCommonPollingPolicy(original.clone());
  EXPECT_TRUE(common->OnFailure(TransientError()));
  EXPECT_FALSE(common->OnFailure(TransientError()));
  EXPECT_GE(10_ms, common->WaitPeriod());
  EXPECT_LE(10_ms, common->WaitPeriod());

  // Ensure the initial state of the policy is cloned, not the current state.
  auto common_clone = common->clone();
  EXPECT_TRUE(common_clone->OnFailure(TransientError()));
  EXPECT_FALSE(common_clone->OnFailure(TransientError()));
  EXPECT_GE(10_ms, common_clone->WaitPeriod());
  EXPECT_LE(10_ms, common_clone->WaitPeriod());
}

/// @test Verify that non-retryable errors cause an immediate failure.
TEST(GenericPollingPolicy, OnNonRetryable) {
  LimitedTimeRetryPolicy retry(kLimitedTimeTestPeriod);
  ExponentialBackoffPolicy backoff(10_ms, 50_ms);
  GenericPollingPolicy<> tested(retry, backoff);
  EXPECT_FALSE(
      static_cast<PollingPolicy&>(tested).OnFailure(GrpcPermanentError()));
  EXPECT_FALSE(tested.OnFailure(PermanentError()));

  auto common = bigtable_internal::MakeCommonPollingPolicy(tested.clone());
  EXPECT_FALSE(common->OnFailure(PermanentError()));
}

/// @test Verify that IsPermanentError works.
TEST(GenericPollingPolicy, IsPermanentError) {
  LimitedTimeRetryPolicy retry(kLimitedTimeTestPeriod);
  ExponentialBackoffPolicy backoff(10_ms, 50_ms);
  GenericPollingPolicy<> tested(retry, backoff);
  EXPECT_TRUE(tested.IsPermanentError(PermanentError()));
  EXPECT_FALSE(tested.IsPermanentError(TransientError()));
  EXPECT_TRUE(static_cast<PollingPolicy&>(tested).IsPermanentError(
      GrpcPermanentError()));
  EXPECT_FALSE(static_cast<PollingPolicy&>(tested).IsPermanentError(
      GrpcTransientError()));
}

/// @test Verify that the backoff policy's wait period is used.
TEST(GenericPollingPolicy, WaitPeriod) {
  LimitedTimeRetryPolicy retry(kLimitedTimeTestPeriod);
  ExponentialBackoffPolicy backoff(10_ms, 50_ms);
  GenericPollingPolicy<> tested(retry, backoff);
  EXPECT_GE(10_ms, tested.WaitPeriod());
  EXPECT_LE(10_ms, tested.WaitPeriod());

  auto common = bigtable_internal::MakeCommonPollingPolicy(tested.clone());
  EXPECT_GE(10_ms, common->WaitPeriod());
  EXPECT_LE(10_ms, common->WaitPeriod());
}

}  // anonymous namespace
}  // namespace bigtable
}  // namespace cloud
}  // namespace google
