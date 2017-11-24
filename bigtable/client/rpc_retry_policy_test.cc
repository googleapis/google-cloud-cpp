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

#include <gtest/gtest.h>
#include <chrono>

// TODO(coryan) - these are useful when writing tests, consider refactoring.
// TODO(coryan) - these are generally useful, consider submitting to abseil.io
namespace bigtable {
namespace chrono_literals {
constexpr std::chrono::milliseconds operator "" _ms(unsigned long long ms) {
  return std::chrono::milliseconds(ms);
}
}  // namespace chrono_literals
}  // namespace bigtable

/**
 * @test A simple test for the ExponentialBackoffRetryPolicy.
 */
TEST(ExponentialBackoffRetryPolicy, Simple) {
  using namespace bigtable::chrono_literals;
  bigtable::ExponentialBackoffPolicy tested(3, 10_ms, 150_ms);
  
  std::chrono::milliseconds delay;
  EXPECT_TRUE(tested.on_failure(
      grpc::Status(grpc::StatusCode::UNAVAILABLE, "please try again"), &delay));
  EXPECT_TRUE(tested.on_failure(
      grpc::Status(grpc::StatusCode::UNAVAILABLE, "please try again"), &delay));
  EXPECT_TRUE(tested.on_failure(
      grpc::Status(grpc::StatusCode::UNAVAILABLE, "please try again"), &delay));
  EXPECT_FALSE(tested.on_failure(
      grpc::Status(grpc::StatusCode::UNAVAILABLE, "please try again"), &delay));
}

/**
 * @test Verify that non-retryable errors cause an immediate failure.
 */
TEST(ExponentialBackoffRetryPolicy, OnNonRetryable) {
  using namespace bigtable::chrono_literals;
  bigtable::ExponentialBackoffPolicy tested(3, 10_ms, 150_ms);

  std::chrono::milliseconds delay;
  EXPECT_FALSE(tested.on_failure(
      grpc::Status(grpc::StatusCode::FAILED_PRECONDITION, "uh oh"), &delay));
}

/**
 * @test Test cloning for ExponentialBackoffRetryPolicy.
 */
TEST(ExponentialBackoffRetryPolicy, Clone) {
  using namespace bigtable::chrono_literals;
  bigtable::ExponentialBackoffPolicy original(3, 10_ms, 150_ms);
  auto tested = original.clone();

  std::chrono::milliseconds delay;
  EXPECT_TRUE(tested->on_failure(
      grpc::Status(grpc::StatusCode::UNAVAILABLE, "please try again"), &delay));
  EXPECT_TRUE(tested->on_failure(
      grpc::Status(grpc::StatusCode::UNAVAILABLE, "please try again"), &delay));
  EXPECT_TRUE(tested->on_failure(
      grpc::Status(grpc::StatusCode::UNAVAILABLE, "please try again"), &delay));
  EXPECT_FALSE(tested->on_failure(
      grpc::Status(grpc::StatusCode::UNAVAILABLE, "please try again"), &delay));
}
