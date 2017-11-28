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

#include "bigtable/client/rpc_backoff_policy.h"
#include "bigtable/client/chrono_literals.h"

#include <gtest/gtest.h>
#include <chrono>

namespace {
/// Create a grpc::Status with a status code for transient errors.
grpc::Status transient() {
  return grpc::Status(grpc::StatusCode::UNAVAILABLE, "please try again");
}
}  // anonymous namespace

/// @test A simple test for the ExponentialBackoffRetryPolicy.
TEST(ExponentialBackoffRetryPolicy, Simple) {
  using namespace bigtable::chrono_literals;
  bigtable::ExponentialBackoffPolicy tested(10_ms, 50_ms);
  
  EXPECT_EQ(10_ms, tested.on_completion(transient()));
  EXPECT_EQ(20_ms, tested.on_completion(transient()));
  EXPECT_EQ(40_ms, tested.on_completion(transient()));
  EXPECT_EQ(50_ms, tested.on_completion(transient()));
  EXPECT_EQ(50_ms, tested.on_completion(transient()));
}

/// @test Test cloning for ExponentialBackoffRetryPolicy.
TEST(ExponentialBackoffRetryPolicy, Clone) {
  using namespace bigtable::chrono_literals;
  bigtable::ExponentialBackoffPolicy original(10_ms, 150_ms);
  auto tested = original.clone();

  EXPECT_EQ(10_ms, tested->on_completion(transient()));
  EXPECT_EQ(20_ms, tested->on_completion(transient()));
  EXPECT_EQ(40_ms, tested->on_completion(transient()));
  EXPECT_EQ(80_ms, tested->on_completion(transient()));
  EXPECT_EQ(150_ms, tested->on_completion(transient()));
}
