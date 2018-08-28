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

#include "google/cloud/bigtable/polling_policy.h"
#include "google/cloud/testing_util/check_predicate_becomes_false.h"
#include "google/cloud/testing_util/chrono_literals.h"
#include <gtest/gtest.h>
#include <chrono>
#include <thread>

namespace {

/// Create a grpc::Status with a status code for permanent errors.
grpc::Status CreatePermanentError() {
  return grpc::Status(grpc::StatusCode::FAILED_PRECONDITION, "failed");
}

namespace bigtable = google::cloud::bigtable;
using namespace google::cloud::testing_util::chrono_literals;
auto const kLimitedTimeTestPeriod = 50_ms;
auto const kLimitedTimeTolerance = 10_ms;

/**
 * @test Verify that a polling policy configured to run for 50ms
 * works correctly.
 *
 * This eliminates some amount of code duplication in the following tests.
 */
void CheckLimitedTime(bigtable::PollingPolicy& tested) {
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
TEST(GenericPollingPolicy, Simple) {
  bigtable::LimitedTimeRetryPolicy retry(kLimitedTimeTestPeriod);
  bigtable::ExponentialBackoffPolicy backoff(
      bigtable::internal::kBigtableLimits);
  bigtable::GenericPollingPolicy<> tested(retry, backoff);
  CheckLimitedTime(tested);
}

/// @test Test cloning for LimitedTimeRetryPolicy.
TEST(GenericPollingPolicy, Clone) {
  bigtable::LimitedTimeRetryPolicy retry(kLimitedTimeTestPeriod);
  bigtable::ExponentialBackoffPolicy backoff(
      bigtable::internal::kBigtableLimits);
  bigtable::GenericPollingPolicy<> original(retry, backoff);
  auto tested = original.clone();
  CheckLimitedTime(*tested);
}

/// @test Verify that non-retryable errors cause an immediate failure.
TEST(GenericPollingPolicy, OnNonRetryable) {
  bigtable::LimitedTimeRetryPolicy retry(kLimitedTimeTestPeriod);
  bigtable::ExponentialBackoffPolicy backoff(
      bigtable::internal::kBigtableLimits);
  bigtable::GenericPollingPolicy<> tested(retry, backoff);
  EXPECT_FALSE(tested.OnFailure(CreatePermanentError()));
}
