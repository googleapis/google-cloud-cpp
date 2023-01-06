// Copyright 2017 Google Inc.
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

#include "google/cloud/bigtable/rpc_backoff_policy.h"
#include "google/cloud/testing_util/chrono_literals.h"
#include <gtest/gtest.h>
#include <chrono>
#include <vector>

namespace google {
namespace cloud {
namespace bigtable {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::google::cloud::testing_util::chrono_literals::operator"" _ms;  // NOLINT

/// Create a grpc::Status with a status code for transient errors.
grpc::Status GrpcTransientError() {
  return grpc::Status(grpc::StatusCode::UNAVAILABLE, "please try again");
}

/// @test A simple test for the ExponentialBackoffRetryPolicy.
TEST(ExponentialBackoffRetryPolicy, Simple) {
  ExponentialBackoffPolicy tested(10_ms, 500_ms);

  EXPECT_GE(10_ms, tested.OnCompletion(GrpcTransientError()));
  EXPECT_NE(500_ms, tested.OnCompletion(GrpcTransientError()));
  EXPECT_NE(500_ms, tested.OnCompletion(GrpcTransientError()));
  // Value should not be exactly XX_ms after few iterations.
  for (int i = 0; i < 5; ++i) {
    tested.OnCompletion(GrpcTransientError());
  }
  EXPECT_GE(500_ms, tested.OnCompletion(GrpcTransientError()));

  // Verify that converting to a common backoff policy preserves behavior.
  auto common = bigtable_internal::MakeCommonBackoffPolicy(tested.clone());
  EXPECT_GE(10_ms, common->OnCompletion());
  EXPECT_NE(500_ms, common->OnCompletion());
  EXPECT_NE(500_ms, common->OnCompletion());
  // Value should not be exactly XX_ms after few iterations.
  for (int i = 0; i < 5; ++i) {
    common->OnCompletion();
  }
  EXPECT_GE(500_ms, common->OnCompletion());
}

/// @test Test cloning for ExponentialBackoffRetryPolicy.
TEST(ExponentialBackoffRetryPolicy, Clone) {
  ExponentialBackoffPolicy original(10_ms, 50_ms);
  auto tested = original.clone();

  EXPECT_GE(10_ms, tested->OnCompletion(GrpcTransientError()));
  EXPECT_LE(10_ms, tested->OnCompletion(GrpcTransientError()));

  // Ensure the initial state of the policy is cloned, not the current state.
  tested = tested->clone();
  EXPECT_GE(10_ms, tested->OnCompletion(GrpcTransientError()));

  // Verify that converting to a common backoff policy preserves behavior.
  auto common = bigtable_internal::MakeCommonBackoffPolicy(original.clone());
  auto common_clone = common->clone();
  EXPECT_GE(10_ms, common_clone->OnCompletion());
  EXPECT_LE(10_ms, common_clone->OnCompletion());

  // Ensure the initial state of the policy is cloned, not the current state.
  common_clone = common_clone->clone();
  EXPECT_GE(10_ms, common_clone->OnCompletion());
}

/// @test Test for testing randomness for 2 objects of
/// ExponentialBackoffRetryPolicy such that no two clients have same sleep time.
TEST(ExponentialBackoffRetryPolicy, Randomness) {
  ExponentialBackoffPolicy test_object1(10_ms, 1500_ms);
  ExponentialBackoffPolicy test_object2(10_ms, 1500_ms);
  std::vector<std::chrono::milliseconds::rep> output1;
  std::vector<std::chrono::milliseconds::rep> output2;

  EXPECT_GE(10_ms, test_object1.OnCompletion(GrpcTransientError()));
  EXPECT_GE(10_ms, test_object2.OnCompletion(GrpcTransientError()));
  for (int i = 0; i < 100; ++i) {
    output1.push_back(test_object1.OnCompletion(GrpcTransientError()).count());
    output2.push_back(test_object2.OnCompletion(GrpcTransientError()).count());
  }
  EXPECT_NE(output1, output2);
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable
}  // namespace cloud
}  // namespace google
