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
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {
namespace {

using ::google::cloud::testing_util::StatusIs;
using ::testing::Contains;
using ::testing::HasSubstr;
using ::testing::IsSupersetOf;
using ::testing::Pair;

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

}  // namespace
}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google
