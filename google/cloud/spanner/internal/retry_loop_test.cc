// Copyright 2019 Google LLC
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

#include "google/cloud/spanner/internal/retry_loop.h"
#include "google/cloud/testing_util/assert_ok.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace spanner {
inline namespace SPANNER_CLIENT_NS {
namespace internal {
namespace {

using ::testing::HasSubstr;
using ::testing::Return;

std::unique_ptr<RetryPolicy> TestRetryPolicy() {
  return LimitedErrorCountRetryPolicy(5).clone();
}

std::unique_ptr<BackoffPolicy> TestBackoffPolicy() {
  return ExponentialBackoffPolicy(std::chrono::microseconds(1),
                                  std::chrono::microseconds(5), 2.0)
      .clone();
}

TEST(RetryLoopTest, Success) {
  grpc::ClientContext context;
  StatusOr<int> actual = RetryLoop(
      TestRetryPolicy(), TestBackoffPolicy(), true,
      [](grpc::ClientContext&, int request) {
        return StatusOr<int>(2 * request);
      },
      context, 42, "error message");
  EXPECT_STATUS_OK(actual);
  EXPECT_EQ(84, *actual);
}

TEST(RetryLoopTest, TransientThenSuccess) {
  grpc::ClientContext context;
  int counter = 0;
  StatusOr<int> actual = RetryLoop(
      TestRetryPolicy(), TestBackoffPolicy(), true,
      [&counter](grpc::ClientContext&, int request) {
        if (++counter < 3) {
          return StatusOr<int>(Status(StatusCode::kUnavailable, "try again"));
        }
        return StatusOr<int>(2 * request);
      },
      context, 42, "error message");
  EXPECT_STATUS_OK(actual);
  EXPECT_EQ(84, *actual);
}

TEST(RetryLoopTest, ReturnJustStatus) {
  grpc::ClientContext context;
  int counter = 0;
  Status actual = RetryLoop(
      TestRetryPolicy(), TestBackoffPolicy(), true,
      [&counter](grpc::ClientContext&, int) {
        if (++counter <= 3) {
          return Status(StatusCode::kAborted, "nothing done");
        }
        return Status();
      },
      context, 42, "error message");
  EXPECT_STATUS_OK(actual);
}

// gmock makes clang-tidy very angry, disable a few warnings that we have no
// control over.
// NOLINTNEXTLINE(clang-analyzer-optin.cplusplus.VirtualCall)
class MockBackoffPolicy : public BackoffPolicy {
 public:
  // NOLINTNEXTLINE(misc-non-private-member-variables-in-classes)
  MOCK_CONST_METHOD0(clone, std::unique_ptr<BackoffPolicy>());
  // NOLINTNEXTLINE(misc-non-private-member-variables-in-classes)
  MOCK_METHOD0(OnCompletion, std::chrono::milliseconds());
};

/**
 * @test Verify the backoff policy is queried after each failure.
 *
 * Note that this is not testing if the
 */
TEST(RetryLoopTest, UsesBackoffPolicy) {
  using ms = std::chrono::milliseconds;

  std::unique_ptr<MockBackoffPolicy> mock(new MockBackoffPolicy);
  EXPECT_CALL(*mock, OnCompletion())
      .WillOnce(Return(ms(10)))
      .WillOnce(Return(ms(20)))
      .WillOnce(Return(ms(30)));

  grpc::ClientContext context;
  int counter = 0;
  std::vector<ms> sleep_for;
  StatusOr<int> actual = RetryLoopImpl(
      TestRetryPolicy(), std::move(mock),  // NOLINT
      true,
      [&counter](grpc::ClientContext&, int request) {
        if (++counter <= 3) {
          return StatusOr<int>(Status(StatusCode::kUnavailable, "try again"));
        }
        return StatusOr<int>(2 * request);
      },
      context, 42, "error message",
      [&sleep_for](ms p) { sleep_for.push_back(p); });
  EXPECT_STATUS_OK(actual);
  EXPECT_EQ(84, *actual);
  EXPECT_THAT(sleep_for, ::testing::ElementsAre(
                             ms(10), std::chrono::milliseconds(20), ms(30)));
}

TEST(RetryLoopTest, TransientFailureNonIdempotent) {
  grpc::ClientContext context;
  StatusOr<int> actual = RetryLoop(
      TestRetryPolicy(), TestBackoffPolicy(), false,
      [](grpc::ClientContext&, int) {
        return StatusOr<int>(Status(StatusCode::kUnavailable, "try again"));
      },
      context, 42, "the answer to everything");
  EXPECT_EQ(StatusCode::kUnavailable, actual.status().code());
  EXPECT_THAT(actual.status().message(), HasSubstr("try again"));
  EXPECT_THAT(actual.status().message(), HasSubstr("the answer to everything"));
  EXPECT_THAT(actual.status().message(), HasSubstr("Error in non-idempotent"));
}

TEST(RetryLoopTest, PermanentFailureFailureIdempotent) {
  grpc::ClientContext context;
  StatusOr<int> actual = RetryLoop(
      TestRetryPolicy(), TestBackoffPolicy(), true,
      [](grpc::ClientContext&, int) {
        return StatusOr<int>(Status(StatusCode::kPermissionDenied, "uh oh"));
      },
      context, 42, "the answer to everything");
  EXPECT_EQ(StatusCode::kPermissionDenied, actual.status().code());
  EXPECT_THAT(actual.status().message(), HasSubstr("uh oh"));
  EXPECT_THAT(actual.status().message(), HasSubstr("the answer to everything"));
  EXPECT_THAT(actual.status().message(), HasSubstr("Permanent error"));
}

TEST(RetryLoopTest, TooManyTransientFailuresIdempotent) {
  grpc::ClientContext context;
  StatusOr<int> actual = RetryLoop(
      TestRetryPolicy(), TestBackoffPolicy(), true,
      [](grpc::ClientContext&, int) {
        return StatusOr<int>(Status(StatusCode::kUnavailable, "try again"));
      },
      context, 42, "the answer to everything");
  EXPECT_EQ(StatusCode::kUnavailable, actual.status().code());
  EXPECT_THAT(actual.status().message(), HasSubstr("try again"));
  EXPECT_THAT(actual.status().message(), HasSubstr("the answer to everything"));
  EXPECT_THAT(actual.status().message(), HasSubstr("Retry policy exhausted"));
}

}  // namespace
}  // namespace internal
}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner
}  // namespace cloud
}  // namespace google
