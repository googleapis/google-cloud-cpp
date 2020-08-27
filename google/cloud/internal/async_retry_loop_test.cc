// Copyright 2020 Google LLC
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

#include "google/cloud/internal/async_retry_loop.h"
#include "google/cloud/internal/background_threads_impl.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
inline namespace GOOGLE_CLOUD_CPP_NS {
namespace internal {
namespace {

using ::google::cloud::testing_util::StatusIs;
using ::testing::AllOf;
using ::testing::HasSubstr;
using ::testing::Return;

struct TestRetryablePolicy {
  static bool IsPermanentFailure(google::cloud::Status const& s) {
    return !s.ok() &&
           (s.code() == google::cloud::StatusCode::kPermissionDenied);
  }
};

auto constexpr kMaxRetries = 5;
std::unique_ptr<RetryPolicy> TestRetryPolicy() {
  return LimitedErrorCountRetryPolicy<TestRetryablePolicy>(kMaxRetries).clone();
}

std::unique_ptr<BackoffPolicy> TestBackoffPolicy() {
  return ExponentialBackoffPolicy(std::chrono::microseconds(1),
                                  std::chrono::microseconds(5), 2.0)
      .clone();
}

TEST(AsyncRetryLoopTest, Success) {
  AutomaticallyCreatedBackgroundThreads background;
  StatusOr<int> actual =
      AsyncRetryLoop(
          TestRetryPolicy(), TestBackoffPolicy(), true, background.cq(),
          [](google::cloud::CompletionQueue&,
             std::unique_ptr<grpc::ClientContext>,
             int request) -> future<StatusOr<int>> {
            return make_ready_future(StatusOr<int>(2 * request));
          },
          42, "error message")
          .get();
  ASSERT_THAT(actual.status(), StatusIs(StatusCode::kOk));
  EXPECT_EQ(84, *actual);
}

TEST(AsyncRetryLoopTest, TransientThenSuccess) {
  int counter = 0;
  AutomaticallyCreatedBackgroundThreads background;
  StatusOr<int> actual =
      AsyncRetryLoop(
          TestRetryPolicy(), TestBackoffPolicy(), true, background.cq(),
          [&](google::cloud::CompletionQueue&,
              std::unique_ptr<grpc::ClientContext>, int request) {
            if (++counter < 3) {
              return make_ready_future(
                  StatusOr<int>(Status(StatusCode::kUnavailable, "try again")));
            }
            return make_ready_future(StatusOr<int>(2 * request));
          },
          42, "error message")
          .get();
  ASSERT_THAT(actual.status(), StatusIs(StatusCode::kOk));
  EXPECT_EQ(84, *actual);
}

TEST(AsyncRetryLoopTest, ReturnJustStatus) {
  int counter = 0;
  AutomaticallyCreatedBackgroundThreads background;
  Status actual =
      AsyncRetryLoop(
          TestRetryPolicy(), TestBackoffPolicy(), true, background.cq(),
          [&](google::cloud::CompletionQueue&,
              std::unique_ptr<grpc::ClientContext>, int) {
            if (++counter <= 3) {
              return make_ready_future(
                  Status(StatusCode::kResourceExhausted, "slow-down"));
            }
            return make_ready_future(Status());
          },
          42, "error message")
          .get();
  ASSERT_THAT(actual, StatusIs(StatusCode::kOk));
}

class MockBackoffPolicy : public BackoffPolicy {
 public:
  MOCK_CONST_METHOD0(clone, std::unique_ptr<BackoffPolicy>());
  MOCK_METHOD0(OnCompletion, std::chrono::milliseconds());
};

/// @test Verify the backoff policy is queried after each failure.
TEST(AsyncRetryLoopTest, UsesBackoffPolicy) {
  using ms = std::chrono::milliseconds;

  std::unique_ptr<MockBackoffPolicy> mock(new MockBackoffPolicy);
  EXPECT_CALL(*mock, OnCompletion()).Times(3).WillRepeatedly(Return(ms(1)));

  int counter = 0;
  std::vector<ms> sleep_for;
  AutomaticallyCreatedBackgroundThreads background;
  StatusOr<int> actual =
      AsyncRetryLoop(
          TestRetryPolicy(), std::move(mock), true, background.cq(),
          [&](google::cloud::CompletionQueue&,
              std::unique_ptr<grpc::ClientContext>, int request) {
            if (++counter <= 3) {
              return make_ready_future(
                  StatusOr<int>(Status(StatusCode::kUnavailable, "try again")));
            }
            return make_ready_future(StatusOr<int>(2 * request));
          },
          42, "error message")
          .get();
  ASSERT_THAT(actual.status(), StatusIs(StatusCode::kOk));
  EXPECT_EQ(84, *actual);
}

TEST(AsyncRetryLoopTest, TransientFailureNonIdempotent) {
  AutomaticallyCreatedBackgroundThreads background;
  StatusOr<int> actual =
      AsyncRetryLoop(
          TestRetryPolicy(), TestBackoffPolicy(), false, background.cq(),
          [](google::cloud::CompletionQueue&,
             std::unique_ptr<grpc::ClientContext>, int) {
            return make_ready_future(StatusOr<int>(
                Status(StatusCode::kUnavailable, "test-message-try-again")));
          },
          42, "test-location")
          .get();
  EXPECT_THAT(actual.status(),
              StatusIs(StatusCode::kUnavailable,
                       AllOf(HasSubstr("test-message-try-again"),
                             HasSubstr("Error in non-idempotent"),
                             HasSubstr("test-location"))));
}

TEST(AsyncRetryLoopTest, PermanentFailureIdempotent) {
  AutomaticallyCreatedBackgroundThreads background;
  StatusOr<int> actual =
      AsyncRetryLoop(
          TestRetryPolicy(), TestBackoffPolicy(), true, background.cq(),
          [](google::cloud::CompletionQueue&,
             std::unique_ptr<grpc::ClientContext>, int) {
            return make_ready_future(StatusOr<int>(
                Status(StatusCode::kPermissionDenied, "test-message-uh-oh")));
          },
          42, "test-location")
          .get();
  EXPECT_THAT(actual.status(), StatusIs(StatusCode::kPermissionDenied,
                                        AllOf(HasSubstr("test-message-uh-oh"),
                                              HasSubstr("Permanent error in"),
                                              HasSubstr("test-location"))));
}

TEST(AsyncRetryLoopTest, TooManyTransientFailuresIdempotent) {
  AutomaticallyCreatedBackgroundThreads background;
  StatusOr<int> actual =
      AsyncRetryLoop(
          TestRetryPolicy(), TestBackoffPolicy(), true, background.cq(),
          [](google::cloud::CompletionQueue&,
             std::unique_ptr<grpc::ClientContext>, int) {
            return make_ready_future(StatusOr<int>(
                Status(StatusCode::kUnavailable, "test-message-try-again")));
          },
          42, "test-location")
          .get();
  EXPECT_THAT(actual.status(),
              StatusIs(StatusCode::kUnavailable,
                       AllOf(HasSubstr("test-message-try-again"),
                             HasSubstr("Retry policy exhausted"),
                             HasSubstr("test-location"))));
}

TEST(AsyncRetryLoopTest, ExhaustedDuringBackoff) {
  using ms = std::chrono::milliseconds;
  AutomaticallyCreatedBackgroundThreads background;
  StatusOr<int> actual =
      AsyncRetryLoop(
          LimitedTimeRetryPolicy<TestRetryablePolicy>(ms(10)).clone(),
          ExponentialBackoffPolicy(ms(20), ms(20), 2.0).clone(), true,
          background.cq(),
          [](google::cloud::CompletionQueue&,
             std::unique_ptr<grpc::ClientContext>, int) {
            return make_ready_future(StatusOr<int>(
                Status(StatusCode::kUnavailable, "test-message-try-again")));
          },
          42, "test-location")
          .get();
  EXPECT_THAT(actual.status(),
              StatusIs(StatusCode::kUnavailable,
                       AllOf(HasSubstr("test-message-try-again"),
                             HasSubstr("Retry policy exhausted"),
                             HasSubstr("test-location"))));
}

}  // namespace
}  // namespace internal
}  // namespace GOOGLE_CLOUD_CPP_NS
}  // namespace cloud
}  // namespace google
