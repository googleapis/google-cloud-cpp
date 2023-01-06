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
using ::testing::HasSubstr;

TEST(RetryLoopHelpersTest, IncludeErrorInfo) {
  auto const code = StatusCode::kFailedPrecondition;
  auto const message = std::string{
      "At least one of the pre-conditions you specified did not hold."};
  auto const error_info = ErrorInfo("conditionNotMet", "global",
                                    {{"locationType", "header"},
                                     {"location", "If-Match"},
                                     {"http_status_code", "412"}});
  auto input = Status(code, message, error_info);
  auto actual = RetryLoopError("permanent error", "SomeFunction", input);
  EXPECT_THAT(actual, StatusIs(code, HasSubstr(message)));
  EXPECT_THAT(actual.message(), HasSubstr("permanent error"));
  EXPECT_THAT(actual.message(), HasSubstr("SomeFunction"));
  EXPECT_EQ(actual.error_info(), error_info);
}

}  // namespace
}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google
