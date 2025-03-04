// Copyright 2022 Google LLC
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

#include "google/cloud/internal/retry_loop_helpers.h"
#include "google/cloud/internal/make_status.h"
#include "google/cloud/testing_util/mock_backoff_policy.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {
namespace {

using ::google::cloud::testing_util::IsOkAndHolds;
using ::google::cloud::testing_util::MockBackoffPolicy;
using ::google::cloud::testing_util::StatusIs;
using ::testing::Contains;
using ::testing::HasSubstr;
using ::testing::IsSupersetOf;
using ::testing::Pair;
using ::testing::Return;

ErrorInfo TestErrorInfo() {
  return ErrorInfo("conditionNotMet", "global",
                   {{"locationType", "header"},
                    {"location", "If-Match"},
                    {"http_status_code", "412"}});
}

TEST(RetryLoopHelpers, RetryLoopNonIdempotentError) {
  auto const code = StatusCode::kFailedPrecondition;
  auto const message = std::string{
      "At least one of the pre-conditions you specified did not hold."};
  auto const ei = TestErrorInfo();
  auto actual =
      RetryLoopNonIdempotentError(Status(code, message, ei), "SomeFunction");
  EXPECT_THAT(actual, StatusIs(code, HasSubstr(message)));
  EXPECT_THAT(actual.error_info().reason(), ei.reason());
  EXPECT_THAT(actual.error_info().domain(), ei.domain());
  EXPECT_THAT(actual.error_info().metadata(), IsSupersetOf(ei.metadata()));
  EXPECT_THAT(actual.error_info().metadata(),
              Contains(Pair("gcloud-cpp.retry.original-message", message)));
  EXPECT_THAT(actual.error_info().metadata(),
              Contains(Pair("gcloud-cpp.retry.function", "SomeFunction")));
  EXPECT_THAT(actual.error_info().metadata(),
              Contains(Pair("gcloud-cpp.retry.reason", "non-idempotent")));
}

TEST(RetryLoopHelpers, RetryLoopPolicyErrorExhausted) {
  auto const code = StatusCode::kFailedPrecondition;
  auto const message = std::string{
      "At least one of the pre-conditions you specified did not hold."};
  auto const ei = TestErrorInfo();
  auto actual = RetryLoopError(Status(code, message, ei), "SomeFunction",
                               /*exhausted=*/true);
  EXPECT_THAT(actual, StatusIs(code, HasSubstr(message)));
  EXPECT_THAT(actual.error_info().reason(), ei.reason());
  EXPECT_THAT(actual.error_info().domain(), ei.domain());
  EXPECT_THAT(actual.error_info().metadata(), IsSupersetOf(ei.metadata()));
  EXPECT_THAT(actual.error_info().metadata(),
              Contains(Pair("gcloud-cpp.retry.original-message", message)));
  EXPECT_THAT(actual.error_info().metadata(),
              Contains(Pair("gcloud-cpp.retry.function", "SomeFunction")));
  EXPECT_THAT(
      actual.error_info().metadata(),
      Contains(Pair("gcloud-cpp.retry.reason", "retry-policy-exhausted")));
  EXPECT_THAT(actual.error_info().metadata(),
              Contains(Pair("gcloud-cpp.retry.on-entry", "false")));
}

TEST(RetryLoopHelpers, RetryLoopPolicyErrorExhaustedButOkay) {
  auto const ei = TestErrorInfo();
  auto actual = RetryLoopError(Status(), "SomeFunction",
                               /*exhausted=*/true);
  EXPECT_THAT(actual, StatusIs(StatusCode::kDeadlineExceeded));
  EXPECT_THAT(actual.error_info().metadata(),
              Contains(Pair("gcloud-cpp.retry.function", "SomeFunction")));
  EXPECT_THAT(
      actual.error_info().metadata(),
      Contains(Pair("gcloud-cpp.retry.reason", "retry-policy-exhausted")));
  EXPECT_THAT(actual.error_info().metadata(),
              Contains(Pair("gcloud-cpp.retry.on-entry", "true")));
}

TEST(RetryLoopHelpers, RetryLoopPolicyErrorNotExhausted) {
  auto const code = StatusCode::kFailedPrecondition;
  auto const message = std::string{
      "At least one of the pre-conditions you specified did not hold."};
  auto const ei = TestErrorInfo();
  auto actual = RetryLoopError(Status(code, message, ei), "SomeFunction",
                               /*exhausted=*/false);
  EXPECT_THAT(actual, StatusIs(code, HasSubstr(message)));
  EXPECT_THAT(actual.error_info().reason(), ei.reason());
  EXPECT_THAT(actual.error_info().domain(), ei.domain());
  EXPECT_THAT(actual.error_info().metadata(), IsSupersetOf(ei.metadata()));
  EXPECT_THAT(actual.error_info().metadata(),
              Contains(Pair("gcloud-cpp.retry.original-message", message)));
  EXPECT_THAT(actual.error_info().metadata(),
              Contains(Pair("gcloud-cpp.retry.function", "SomeFunction")));
  EXPECT_THAT(actual.error_info().metadata(),
              Contains(Pair("gcloud-cpp.retry.reason", "permanent-error")));
}

TEST(RetryLoopHelpers, RetryLoopPolicyErrorNotExhaustedButOkay) {
  auto actual = RetryLoopPermanentError(Status(), "SomeFunction");
  EXPECT_THAT(actual, StatusIs(StatusCode::kUnknown));
  EXPECT_THAT(actual.error_info().metadata(),
              Contains(Pair("gcloud-cpp.retry.function", "SomeFunction")));
  EXPECT_THAT(actual.error_info().metadata(),
              Contains(Pair("gcloud-cpp.retry.reason", "permanent-error")));
}

TEST(RetryLoopHelpers, RetryLoopErrorCancelled) {
  auto const code = StatusCode::kFailedPrecondition;
  auto const message = std::string{
      "At least one of the pre-conditions you specified did not hold."};
  auto const ei = TestErrorInfo();
  auto actual = RetryLoopCancelled(Status(code, message, ei), "SomeFunction");
  EXPECT_THAT(actual, StatusIs(code, HasSubstr(message)));
  EXPECT_THAT(actual.error_info().reason(), ei.reason());
  EXPECT_THAT(actual.error_info().domain(), ei.domain());
  EXPECT_THAT(actual.error_info().metadata(), IsSupersetOf(ei.metadata()));
  EXPECT_THAT(actual.error_info().metadata(),
              Contains(Pair("gcloud-cpp.retry.original-message", message)));
  EXPECT_THAT(actual.error_info().metadata(),
              Contains(Pair("gcloud-cpp.retry.function", "SomeFunction")));
  EXPECT_THAT(actual.error_info().metadata(),
              Contains(Pair("gcloud-cpp.retry.reason", "cancelled")));
}

TEST(RetryLoopHelpers, RetryLoopErrorCancelledButOkay) {
  auto actual = RetryLoopCancelled(Status(), "SomeFunction");
  EXPECT_THAT(actual, StatusIs(StatusCode::kCancelled));
  EXPECT_THAT(actual.error_info().metadata(),
              Contains(Pair("gcloud-cpp.retry.function", "SomeFunction")));
  EXPECT_THAT(actual.error_info().metadata(),
              Contains(Pair("gcloud-cpp.retry.reason", "cancelled")));
}

class MockRetryPolicy : public RetryPolicy {
 public:
  MOCK_METHOD(bool, OnFailure, (Status const&), (override));
  MOCK_METHOD(bool, IsExhausted, (), (const, override));
  MOCK_METHOD(bool, IsPermanentFailure, (Status const&), (const, override));
  MOCK_METHOD(std::unique_ptr<RetryPolicy>, clone, (), (const, override));
};

TEST(Backoff, HeedsRetryInfo) {
  auto const retry_delay = std::chrono::minutes(5);
  auto status = internal::ResourceExhaustedError("try again");
  internal::SetRetryInfo(status, internal::RetryInfo{retry_delay});

  auto mock_r = std::make_unique<MockRetryPolicy>();
  EXPECT_CALL(*mock_r, OnFailure(status)).WillOnce(Return(false));
  EXPECT_CALL(*mock_r, IsExhausted).WillOnce(Return(false));
  auto mock_b = std::make_unique<MockBackoffPolicy>();
  EXPECT_CALL(*mock_b, OnCompletion);

  auto actual =
      Backoff(status, "SomeFunction", *mock_r, *mock_b,
              Idempotency::kNonIdempotent, /*enable_server_retries=*/true);
  EXPECT_THAT(actual, IsOkAndHolds(retry_delay));
}

TEST(Backoff, NoRetriesIfPolicyExhausted) {
  auto const retry_delay = std::chrono::minutes(5);
  auto status = internal::ResourceExhaustedError("try again");
  internal::SetRetryInfo(status, internal::RetryInfo{retry_delay});

  auto mock_r = std::make_unique<MockRetryPolicy>();
  EXPECT_CALL(*mock_r, OnFailure(status)).WillOnce(Return(false));
  EXPECT_CALL(*mock_r, IsExhausted).WillOnce(Return(true));
  auto mock_b = std::make_unique<MockBackoffPolicy>();
  EXPECT_CALL(*mock_b, OnCompletion).Times(0);

  auto actual =
      Backoff(status, "SomeFunction", *mock_r, *mock_b,
              Idempotency::kIdempotent, /*enable_server_retries=*/true);
  EXPECT_THAT(actual, StatusIs(StatusCode::kResourceExhausted,
                               HasSubstr("policy exhausted")));
}

TEST(Backoff, PermanentFailureWhenIgnoringRetryInfo) {
  auto const retry_delay = std::chrono::minutes(5);
  auto status = internal::ResourceExhaustedError("try again");
  internal::SetRetryInfo(status, internal::RetryInfo{retry_delay});

  auto mock_r = std::make_unique<MockRetryPolicy>();
  EXPECT_CALL(*mock_r, OnFailure).WillOnce(Return(false));
  auto mock_b = std::make_unique<MockBackoffPolicy>();
  EXPECT_CALL(*mock_b, OnCompletion).Times(0);

  auto actual =
      Backoff(status, "SomeFunction", *mock_r, *mock_b,
              Idempotency::kIdempotent, /*enable_server_retries=*/false);
  EXPECT_THAT(actual, StatusIs(StatusCode::kResourceExhausted,
                               HasSubstr("Permanent error")));
}

TEST(Backoff, TransientFailureWhenIgnoringRetryInfo) {
  auto const retry_delay = std::chrono::minutes(5);
  auto status = internal::UnavailableError("try again");
  internal::SetRetryInfo(status, internal::RetryInfo{retry_delay});

  auto mock_r = std::make_unique<MockRetryPolicy>();
  EXPECT_CALL(*mock_r, OnFailure).WillOnce(Return(true));
  auto mock_b = std::make_unique<MockBackoffPolicy>();
  EXPECT_CALL(*mock_b, OnCompletion)
      .WillOnce(Return(std::chrono::milliseconds(10)));

  auto actual =
      Backoff(status, "SomeFunction", *mock_r, *mock_b,
              Idempotency::kIdempotent, /*enable_server_retries=*/false);
  EXPECT_THAT(actual, IsOkAndHolds(std::chrono::milliseconds(10)));
}

TEST(Backoff, NonIdempotentFailureWhenIgnoringRetryInfo) {
  auto const retry_delay = std::chrono::minutes(5);
  auto status = internal::ResourceExhaustedError("try again");
  internal::SetRetryInfo(status, internal::RetryInfo{retry_delay});

  auto mock_r = std::make_unique<MockRetryPolicy>();
  ON_CALL(*mock_r, OnFailure).WillByDefault(Return(true));
  auto mock_b = std::make_unique<MockBackoffPolicy>();
  EXPECT_CALL(*mock_b, OnCompletion).Times(0);

  auto actual =
      Backoff(status, "SomeFunction", *mock_r, *mock_b,
              Idempotency::kNonIdempotent, /*enable_server_retries=*/false);
  EXPECT_THAT(actual, StatusIs(StatusCode::kResourceExhausted,
                               HasSubstr("non-idempotent")));
}

}  // namespace
}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google
