// Copyright 2017 Google Inc.
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

#include "google/cloud/bigtable/rpc_retry_policy.h"
#include "google/cloud/testing_util/check_predicate_becomes_false.h"
#include "google/cloud/testing_util/chrono_literals.h"
#include <gtest/gtest.h>
#include <chrono>
#include <thread>

namespace {
/// Create a grpc::Status with a status code for transient errors.
grpc::Status CreateTransientError() {
  return grpc::Status(grpc::StatusCode::UNAVAILABLE, "please try again");
}

/// Create a grpc::Status with a status code for permanent errors.
grpc::Status CreatePermanentError() {
  return grpc::Status(grpc::StatusCode::FAILED_PRECONDITION, "failed");
}

namespace bigtable = google::cloud::bigtable;
using ::google::cloud::testing_util::chrono_literals::operator"" _ms;

auto const kLimitedTimeTestPeriod = 50_ms;
auto const kLimitedTimeTolerance = 10_ms;

/**
 * @test Verify that a retry policy configured to run for 50ms works correctly.
 *
 * This eliminates some amount of code duplication in the following tests.
 */
void CheckLimitedTime(bigtable::RPCRetryPolicy& tested) {
  google::cloud::testing_util::CheckPredicateBecomesFalse(
      [&tested] {
        return tested.OnFailure(
            grpc::Status(grpc::StatusCode::UNAVAILABLE, "please try again"));
      },
      std::chrono::system_clock::now() + kLimitedTimeTestPeriod,
      kLimitedTimeTolerance);
}

}  // anonymous namespace

/// @test A simple test for the LimitedTimeRetryPolicy.
TEST(LimitedTimeRetryPolicy, Simple) {
  bigtable::LimitedTimeRetryPolicy tested(kLimitedTimeTestPeriod);
  CheckLimitedTime(tested);
}

/// @test A simple test for grpc::StatusCode::OK is not Permanent Error.
TEST(LimitedTimeRetryPolicy, PermanentFailureCheck) {
  bigtable::LimitedTimeRetryPolicy tested(kLimitedTimeTestPeriod);
  EXPECT_FALSE(tested.IsPermanentFailure(grpc::Status::OK));
  EXPECT_FALSE(tested.IsPermanentFailure(CreateTransientError()));
  EXPECT_TRUE(tested.IsPermanentFailure(CreatePermanentError()));
}

/// @test Test cloning for LimitedTimeRetryPolicy.
TEST(LimitedTimeRetryPolicy, Clone) {
  bigtable::LimitedTimeRetryPolicy original(kLimitedTimeTestPeriod);
  auto tested = original.clone();
  CheckLimitedTime(*tested);
}

/// @test Verify that non-retryable errors cause an immediate failure.
TEST(LimitedTimeRetryPolicy, OnNonRetryable) {
  bigtable::LimitedTimeRetryPolicy tested(10_ms);
  EXPECT_FALSE(tested.OnFailure(CreatePermanentError()));
}

/// @test A simple test for the LimitedErrorCountRetryPolicy.
TEST(LimitedErrorCountRetryPolicy, Simple) {
  bigtable::LimitedErrorCountRetryPolicy tested(3);
  EXPECT_TRUE(tested.OnFailure(CreateTransientError()));
  EXPECT_TRUE(tested.OnFailure(CreateTransientError()));
  EXPECT_TRUE(tested.OnFailure(CreateTransientError()));
  EXPECT_FALSE(tested.OnFailure(CreateTransientError()));
  EXPECT_FALSE(tested.OnFailure(CreateTransientError()));
}

/// @test Test cloning for LimitedErrorCountRetryPolicy.
TEST(LimitedErrorCountRetryPolicy, Clone) {
  bigtable::LimitedErrorCountRetryPolicy original(3);
  auto tested = original.clone();
  EXPECT_TRUE(tested->OnFailure(CreateTransientError()));
  EXPECT_TRUE(tested->OnFailure(CreateTransientError()));
  EXPECT_TRUE(tested->OnFailure(CreateTransientError()));
  EXPECT_FALSE(tested->OnFailure(CreateTransientError()));
  EXPECT_FALSE(tested->OnFailure(CreateTransientError()));
}

/// @test Verify that non-retryable errors cause an immediate failure.
TEST(LimitedErrorCountRetryPolicy, OnNonRetryable) {
  bigtable::LimitedErrorCountRetryPolicy tested(3);
  EXPECT_FALSE(tested.OnFailure(CreatePermanentError()));
}
