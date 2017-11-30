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

#include "bigtable/client/rpc_retry_policy.h"
#include "bigtable/client/chrono_literals.h"

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

/// Refactor two test cases for bigtable::LimitedTimeRetryPolicy.
void CheckLimitedTime(bigtable::RPCRetryPolicy& tested) {
  using namespace bigtable::chrono_literals;
  auto start = std::chrono::system_clock::now();
  for (int i = 0; i != 100; ++i) {
    auto actual = tested.on_failure(
        grpc::Status(grpc::StatusCode::UNAVAILABLE, "please try again"));
    if (std::chrono::system_clock::now() < start + 50_ms) {
      EXPECT_TRUE(actual);
    } else {
      EXPECT_FALSE(actual);
      break;
    }
    std::this_thread::sleep_for(1_ms);
  }
}

/// Refactor two test cases for bigtable::LimitedErrorCountRetryPolicy.
void CheckLimitedErrorCount(bigtable::RPCRetryPolicy& tested) {
  EXPECT_TRUE(tested.on_failure(CreateTransientError()));
  EXPECT_TRUE(tested.on_failure(CreateTransientError()));
  EXPECT_TRUE(tested.on_failure(CreateTransientError()));
  EXPECT_FALSE(tested.on_failure(CreateTransientError()));
  EXPECT_FALSE(tested.on_failure(CreateTransientError()));
}

}  // anonymous namespace

/// @test A simple test for the LimitedTimeRetryPolicy.
TEST(LimitedTimeRetryPolicy, Simple) {
  using namespace bigtable::chrono_literals;
  bigtable::LimitedTimeRetryPolicy tested(50_ms);
  CheckLimitedTime(tested);
}

/// @test Test cloning for LimitedTimeRetryPolicy.
TEST(LimitedTimeRetryPolicy, Clone) {
  using namespace bigtable::chrono_literals;
  bigtable::LimitedTimeRetryPolicy original(50_ms);
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
  CheckLimitedErrorCount(tested);
}

/// @test Test cloning for LimitedErrorCountRetryPolicy.
TEST(LimitedErrorCountRetryPolicy, Clone) {
  using namespace bigtable::chrono_literals;
  bigtable::LimitedErrorCountRetryPolicy original(3);
  auto tested = original.clone();
  CheckLimitedErrorCount(*tested);
}

/// @test Verify that non-retryable errors cause an immediate failure.
TEST(LimitedErrorCountRetryPolicy, OnNonRetryable) {
  bigtable::LimitedErrorCountRetryPolicy tested(3);
  EXPECT_FALSE(tested.on_failure(CreatePermanentError()));
}
