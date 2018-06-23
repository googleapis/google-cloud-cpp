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

#include "google/cloud/bigtable/rpc_backoff_policy.h"
#include "google/cloud/bigtable/testing/chrono_literals.h"
#include <gtest/gtest.h>
#include <chrono>
#include <vector>

namespace bigtable = google::cloud::bigtable;

namespace {
/// Create a grpc::Status with a status code for transient errors.
grpc::Status CreateTransientError() {
  return grpc::Status(grpc::StatusCode::UNAVAILABLE, "please try again");
}

}  // anonymous namespace

/// @test A simple test for the ExponentialBackoffRetryPolicy.
TEST(ExponentialBackoffRetryPolicy, Simple) {
  using namespace bigtable::chrono_literals;
  bigtable::ExponentialBackoffPolicy tested(10_ms, 500_ms);

  EXPECT_GE(10_ms, tested.OnCompletion(CreateTransientError()));
  EXPECT_NE(500_ms, tested.OnCompletion(CreateTransientError()));
  EXPECT_NE(500_ms, tested.OnCompletion(CreateTransientError()));
  // Value should not be exactly XX_ms after few iterations.
  for (int i = 0; i < 5; ++i) {
    tested.OnCompletion(CreateTransientError());
  }
  EXPECT_GE(500_ms, tested.OnCompletion(CreateTransientError()));
}

/// @test Test cloning for ExponentialBackoffRetryPolicy.
TEST(ExponentialBackoffRetryPolicy, Clone) {
  using namespace bigtable::chrono_literals;
  bigtable::ExponentialBackoffPolicy original(10_ms, 50_ms);
  auto tested = original.clone();

  EXPECT_GE(10_ms, tested->OnCompletion(CreateTransientError()));
  EXPECT_LE(10_ms, tested->OnCompletion(CreateTransientError()));
}

/// @test Test for testing randomness for 2 objects of
/// ExponentialBackoffRetryPolicy such that no two clients have same sleep time.
TEST(ExponentialBackoffRetryPolicy, Randomness) {
  using namespace bigtable::chrono_literals;
  bigtable::ExponentialBackoffPolicy test_object1(10_ms, 1500_ms);
  bigtable::ExponentialBackoffPolicy test_object2(10_ms, 1500_ms);
  std::vector<int> output1, output2;

  EXPECT_GE(10_ms, test_object1.OnCompletion(CreateTransientError()));
  EXPECT_GE(10_ms, test_object2.OnCompletion(CreateTransientError()));
  for (int i = 0; i < 100; ++i) {
    output1.push_back(
        test_object1.OnCompletion(CreateTransientError()).count());
    output2.push_back(
        test_object2.OnCompletion(CreateTransientError()).count());
  }
  EXPECT_NE(output1, output2);
}
