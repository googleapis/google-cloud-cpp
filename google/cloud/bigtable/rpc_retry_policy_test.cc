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

#include "google/cloud/bigtable/rpc_retry_policy.h"
#include "google/cloud/bigtable/admin/bigtable_instance_admin_connection.h"
#include "google/cloud/testing_util/check_predicate_becomes_false.h"
#include "google/cloud/testing_util/chrono_literals.h"
#include <gtest/gtest.h>
#include <chrono>
#include <thread>

namespace google {
namespace cloud {
namespace bigtable {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

/// Create a grpc::Status with a status code for transient errors.
grpc::Status GrpcTransientError() {
  return grpc::Status(grpc::StatusCode::UNAVAILABLE, "please try again");
}

/// Create a grpc::Status with a status code for permanent errors.
grpc::Status GrpcPermanentError() {
  return grpc::Status(grpc::StatusCode::FAILED_PRECONDITION, "failed");
}

/// Create a Status with a status code for transient errors.
Status TransientError() {
  return Status(StatusCode::kUnavailable, "please try again");
}

/// Create a Status with a status code for permanent errors.
Status PermanentError() {
  return Status(StatusCode::kFailedPrecondition, "failed");
}

using ::google::cloud::testing_util::chrono_literals::operator"" _ms;  // NOLINT

auto const kLimitedTimeTestPeriod = 100_ms;
auto const kLimitedTimeTolerance = 20_ms;

/**
 * @test Verify that a retry policy configured to run for 50ms works correctly.
 *
 * This eliminates some amount of code duplication in the following tests.
 */
void CheckLimitedTime(RPCRetryPolicy& tested) {
  google::cloud::testing_util::CheckPredicateBecomesFalse(
      [&tested] { return tested.OnFailure(GrpcTransientError()); },
      std::chrono::system_clock::now() + kLimitedTimeTestPeriod,
      kLimitedTimeTolerance);
}

void CheckLimitedTime(
    std::unique_ptr<bigtable_admin::BigtableInstanceAdminRetryPolicy> common) {
  google::cloud::testing_util::CheckPredicateBecomesFalse(
      [&common] { return common->OnFailure(TransientError()); },
      std::chrono::system_clock::now() + kLimitedTimeTestPeriod,
      kLimitedTimeTolerance);
}

/// @test A simple test for the LimitedTimeRetryPolicy.
TEST(LimitedTimeRetryPolicy, Simple) {
  LimitedTimeRetryPolicy tested(kLimitedTimeTestPeriod);
  CheckLimitedTime(tested);

  auto common = bigtable_internal::MakeCommonRetryPolicy<
      bigtable_admin::BigtableInstanceAdminRetryPolicy>(tested.clone());
  CheckLimitedTime(std::move(common));
}

/// @test A simple test for grpc::StatusCode::OK is not Permanent Error.
TEST(LimitedTimeRetryPolicy, PermanentFailureCheck) {
  LimitedTimeRetryPolicy tested(kLimitedTimeTestPeriod);
  EXPECT_FALSE(tested.IsPermanentFailure(grpc::Status::OK));
  EXPECT_FALSE(tested.IsPermanentFailure(GrpcTransientError()));
  EXPECT_TRUE(tested.IsPermanentFailure(GrpcPermanentError()));

  auto common = bigtable_internal::MakeCommonRetryPolicy<
      bigtable_admin::BigtableInstanceAdminRetryPolicy>(tested.clone());
  EXPECT_FALSE(common->IsPermanentFailure(Status()));
  EXPECT_FALSE(common->IsPermanentFailure(TransientError()));
  EXPECT_TRUE(common->IsPermanentFailure(PermanentError()));
}

/// @test Test cloning for LimitedTimeRetryPolicy.
TEST(LimitedTimeRetryPolicy, Clone) {
  LimitedTimeRetryPolicy original(kLimitedTimeTestPeriod);
  auto tested = original.clone();
  CheckLimitedTime(*tested);

  auto common = bigtable_internal::MakeCommonRetryPolicy<
      bigtable_admin::BigtableInstanceAdminRetryPolicy>(original.clone());
  CheckLimitedTime(common->clone());
}

/// @test Verify that non-retryable errors cause an immediate failure.
TEST(LimitedTimeRetryPolicy, OnNonRetryable) {
  LimitedTimeRetryPolicy tested(10_ms);
  EXPECT_FALSE(tested.OnFailure(GrpcPermanentError()));

  auto common = bigtable_internal::MakeCommonRetryPolicy<
      bigtable_admin::BigtableInstanceAdminRetryPolicy>(tested.clone());
  EXPECT_FALSE(common->OnFailure(PermanentError()));
}

/// @test A simple test for the LimitedErrorCountRetryPolicy.
TEST(LimitedErrorCountRetryPolicy, Simple) {
  LimitedErrorCountRetryPolicy tested(3);
  EXPECT_FALSE(tested.IsExhausted());
  // Attempt 1
  EXPECT_TRUE(tested.OnFailure(GrpcTransientError()));
  EXPECT_FALSE(tested.IsExhausted());
  // Attempt 2
  EXPECT_TRUE(tested.OnFailure(GrpcTransientError()));
  EXPECT_FALSE(tested.IsExhausted());
  // Attempt 3
  EXPECT_TRUE(tested.OnFailure(GrpcTransientError()));
  EXPECT_FALSE(tested.IsExhausted());
  // Attempt 4
  EXPECT_FALSE(tested.OnFailure(GrpcTransientError()));
  EXPECT_TRUE(tested.IsExhausted());
  // Attempt 5
  EXPECT_FALSE(tested.OnFailure(GrpcTransientError()));
  EXPECT_TRUE(tested.IsExhausted());

  auto common = bigtable_internal::MakeCommonRetryPolicy<
      bigtable_admin::BigtableInstanceAdminRetryPolicy>(tested.clone());
  EXPECT_FALSE(common->IsExhausted());
  // Attempt 1
  EXPECT_TRUE(common->OnFailure(TransientError()));
  EXPECT_FALSE(common->IsExhausted());
  // Attempt 2
  EXPECT_TRUE(common->OnFailure(TransientError()));
  EXPECT_FALSE(common->IsExhausted());
  // Attempt 3
  EXPECT_TRUE(common->OnFailure(TransientError()));
  EXPECT_FALSE(common->IsExhausted());
  // Attempt 4
  EXPECT_FALSE(common->OnFailure(TransientError()));
  EXPECT_TRUE(common->IsExhausted());
  // Attempt 5
  EXPECT_FALSE(common->OnFailure(TransientError()));
  EXPECT_TRUE(common->IsExhausted());
}

/// @test Test cloning for LimitedErrorCountRetryPolicy.
TEST(LimitedErrorCountRetryPolicy, Clone) {
  LimitedErrorCountRetryPolicy original(3);
  auto tested = original.clone();
  EXPECT_TRUE(tested->OnFailure(GrpcTransientError()));
  EXPECT_TRUE(tested->OnFailure(GrpcTransientError()));
  EXPECT_TRUE(tested->OnFailure(GrpcTransientError()));
  EXPECT_FALSE(tested->OnFailure(GrpcTransientError()));
  EXPECT_FALSE(tested->OnFailure(GrpcTransientError()));
}

/// @test Verify that non-retryable errors cause an immediate failure.
TEST(LimitedErrorCountRetryPolicy, OnNonRetryable) {
  LimitedErrorCountRetryPolicy tested(3);
  EXPECT_FALSE(tested.OnFailure(GrpcPermanentError()));

  auto common = bigtable_internal::MakeCommonRetryPolicy<
      bigtable_admin::BigtableInstanceAdminRetryPolicy>(tested.clone());
  EXPECT_FALSE(common->OnFailure(PermanentError()));
}

/**
 * @test Verify that converting to a common policy exhibits best-effort
 * `IsExhausted()` logic.
 *
 * This test simulates the conversion of a user-supplied
 * `bigtable::RPCRetryPolicy`, which will not have an `IsExhausted()` method, to
 * a common policy, which requires the method. If the policy fails without
 * encountering a permanent error, we should say it has been exhausted.
 *
 * For context, the value of `IsExhausted()` is used to determine whether we can
 * exit a loop at the start of an iteration, before making another request. The
 * value also determines whether we report that the retry loop is exhausted, or
 * if the retry loop encountered a permanent error.
 */
TEST(CommonRetryPolicy, IsExhaustedBestEffort) {
  // This class is essentially a `LimitedErrorCountRetryPolicy(0)`. The
  // important thing to note is that it does not override `IsExhausted()`.
  class CustomRetryPolicy : public RPCRetryPolicy {
   public:
    std::unique_ptr<RPCRetryPolicy> clone() const override {
      return absl::make_unique<CustomRetryPolicy>();
    }
    void Setup(grpc::ClientContext&) const override {}
    bool OnFailure(Status const&) override { return false; }
    // TODO(#2344) - remove ::grpc::Status version.
    bool OnFailure(grpc::Status const&) override { return false; }
  };

  CustomRetryPolicy tested;
  auto common = bigtable_internal::MakeCommonRetryPolicy<
      bigtable_admin::BigtableInstanceAdminRetryPolicy>(tested.clone());
  EXPECT_FALSE(common->OnFailure(TransientError()));
  EXPECT_TRUE(common->IsExhausted());
}

/// @test Verify that certain known internal errors are retryable.
TEST(TransientInternalError, RstStreamRetried) {
  EXPECT_FALSE(internal::SafeGrpcRetry::IsTransientFailure(
      Status(StatusCode::kInternal, "non-retryable")));
  EXPECT_TRUE(internal::SafeGrpcRetry::IsTransientFailure(
      Status(StatusCode::kInternal, "RST_STREAM")));
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable
}  // namespace cloud
}  // namespace google
