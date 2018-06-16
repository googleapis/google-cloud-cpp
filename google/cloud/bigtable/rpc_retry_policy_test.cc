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
#include "google/cloud/bigtable/testing/chrono_literals.h"
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
using namespace bigtable::chrono_literals;
auto const kLimitedTimeTestPeriod = 50_ms;
auto const kLimitedTimeTolerance = 10_ms;

/**
 * @test Verify that a retry policy configured to run for 50ms works correctly.
 *
 * This eliminates some amount of code duplication in the following tests.
 */
void CheckLimitedTime(bigtable::RPCRetryPolicy& tested) {
  auto start = std::chrono::system_clock::now();
  // This is one of those tests that can get annoyingly flaky, it is based on
  // time.  Basically we want to know that the policy will accept failures
  // until around its prescribed deadline (50ms in this test).  Instead of
  // measuring for *exactly* 50ms, we pass the test if:
  //   - All calls to on_failure() in the first 50ms - 10ms pass.
  //   - Calls to on_failure() after 50ms + 10ms are rejected.
  //   - We do not care about the results from 40ms to 60ms.
  // I know 10ms feels like a long time, but it is not on a loaded VM running
  // the tests inside some container.
  // There is one case observed in this which will cause the result fail.
  //   - Due to server overload if there is a delay between "OnFailure" and
  //       "now" statement execution then test case may fail. Due to this
  //       reason we are checking the timestamp before OnFailure and with
  //       now and both should satify the criteria of the value of "actual"
  auto must_be_true_before =
      start + kLimitedTimeTestPeriod - kLimitedTimeTolerance;
  auto must_be_false_after =
      start + kLimitedTimeTestPeriod + kLimitedTimeTolerance;
  int true_counter = 0;
  int false_counter = 0;
  for (int i = 0; i != 100; ++i) {
    auto new_start = std::chrono::system_clock::now();
    auto actual = tested.OnFailure(
        grpc::Status(grpc::StatusCode::UNAVAILABLE, "please try again"));
    auto now = std::chrono::system_clock::now();
    if (new_start < must_be_true_before and now < must_be_true_before) {
      EXPECT_TRUE(actual);
      ++true_counter;
    } else if (must_be_false_after < new_start and must_be_false_after < now) {
      EXPECT_FALSE(actual);
      ++false_counter;
    }
    std::this_thread::sleep_for(1_ms);
  }
  EXPECT_LE(0, true_counter);
  EXPECT_GE(false_counter, 0);
}

}  // anonymous namespace

/// @test A simple test for the LimitedTimeRetryPolicy.
TEST(LimitedTimeRetryPolicy, Simple) {
  using namespace bigtable::chrono_literals;
  bigtable::LimitedTimeRetryPolicy tested(kLimitedTimeTestPeriod);
  CheckLimitedTime(tested);
}

/// @test Test cloning for LimitedTimeRetryPolicy.
TEST(LimitedTimeRetryPolicy, Clone) {
  using namespace bigtable::chrono_literals;
  bigtable::LimitedTimeRetryPolicy original(kLimitedTimeTestPeriod);
  auto tested = original.clone();
  CheckLimitedTime(*tested);
}

/// @test Verify that non-retryable errors cause an immediate failure.
TEST(LimitedTimeRetryPolicy, OnNonRetryable) {
  using namespace bigtable::chrono_literals;
  bigtable::LimitedTimeRetryPolicy tested(10_ms);
  EXPECT_FALSE(tested.on_failure(CreatePermanentError()));
}

/// @test A simple test for the LimitedErrorCountRetryPolicy.
TEST(LimitedErrorCountRetryPolicy, Simple) {
  using namespace bigtable::chrono_literals;
  bigtable::LimitedErrorCountRetryPolicy tested(3);
  EXPECT_TRUE(tested.on_failure(CreateTransientError()));
  EXPECT_TRUE(tested.on_failure(CreateTransientError()));
  EXPECT_TRUE(tested.on_failure(CreateTransientError()));
  EXPECT_FALSE(tested.on_failure(CreateTransientError()));
  EXPECT_FALSE(tested.on_failure(CreateTransientError()));
}

/// @test Test cloning for LimitedErrorCountRetryPolicy.
TEST(LimitedErrorCountRetryPolicy, Clone) {
  using namespace bigtable::chrono_literals;
  bigtable::LimitedErrorCountRetryPolicy original(3);
  auto tested = original.clone();
  EXPECT_TRUE(tested->on_failure(CreateTransientError()));
  EXPECT_TRUE(tested->on_failure(CreateTransientError()));
  EXPECT_TRUE(tested->on_failure(CreateTransientError()));
  EXPECT_FALSE(tested->on_failure(CreateTransientError()));
  EXPECT_FALSE(tested->on_failure(CreateTransientError()));
}

/// @test Verify that non-retryable errors cause an immediate failure.
TEST(LimitedErrorCountRetryPolicy, OnNonRetryable) {
  bigtable::LimitedErrorCountRetryPolicy tested(3);
  EXPECT_FALSE(tested.on_failure(CreatePermanentError()));
}
