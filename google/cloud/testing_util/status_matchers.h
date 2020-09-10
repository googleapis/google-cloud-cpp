// Copyright 2020 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_TESTING_UTIL_STATUS_MATCHERS_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_TESTING_UTIL_STATUS_MATCHERS_H

#include "google/cloud/status.h"
#include "google/cloud/status_or.h"
#include "google/cloud/version.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
inline namespace GOOGLE_CLOUD_CPP_NS {
namespace testing_util_internal {
// Allows the matchers to work with `Status` or `StatusOr<T>`
inline ::google::cloud::Status const& GetStatus(
    ::google::cloud::Status const& status) {
  return status;
}

template <typename T>
inline ::google::cloud::Status const& GetStatus(
    ::google::cloud::StatusOr<T> const& status) {
  return status.status();
}
}  // namespace testing_util_internal

namespace testing_util {

/**
 * Match the `code` and `message` of a `google::cloud::Status`.
 *
 * example:
 *
 * ```
 * Status status = ...;
 * EXPECT_THAT(status, StatusIs(StatusCode::kInvalidArgument,
 *                              testing::HasSubstr("message substring"));
 * ```
 */
// NOLINTNEXTLINE(readability-redundant-string-init)
MATCHER_P2(StatusIs, code_matcher, message_matcher, "") {
  ::testing::StringMatchResultListener code_listener;
  auto const& status = ::google::cloud::testing_util_internal::GetStatus(arg);
  bool result = true;
  if (!::testing::MatcherCast<::google::cloud::StatusCode const&>(code_matcher)
           .MatchAndExplain(status.code(), &code_listener)) {
    std::string code_explanation = code_listener.str();
    if (code_explanation.empty()) {
      code_explanation = "does not match expected value";
    }
    result = false;
    *result_listener << "code " << status.code() << " " << code_explanation;
  }
  ::testing::StringMatchResultListener message_listener;
  if (!::testing::MatcherCast<std::string const&>(message_matcher)
           .MatchAndExplain(status.message(), &message_listener)) {
    if (!result) {
      *result_listener << "; ";
    }
    result = false;
    std::string message_explanation = message_listener.str();
    if (message_explanation.empty()) {
      message_explanation = "does not match expected value";
    }
    *result_listener << "message '" << status.message() << "' "
                     << message_explanation;
  }
  return result;
}

/// Match the `code` of a `google::cloud::Status`, disregarding the message
// NOLINTNEXTLINE(readability-redundant-string-init)
MATCHER_P(StatusIs, code_matcher, "") {
  auto const& status = ::google::cloud::testing_util_internal::GetStatus(arg);
  return ::testing::MatcherCast<::google::cloud::Status const&>(
             StatusIs(code_matcher, ::testing::_))
      .MatchAndExplain(status, result_listener);
}

/// Shorthand for `StatusIs(StatusCode::kOk)`
// NOLINTNEXTLINE(readability-redundant-string-init)
MATCHER(IsOk, "") {
  auto const& status = ::google::cloud::testing_util_internal::GetStatus(arg);
  return ::testing::MatcherCast<::google::cloud::Status const&>(
             StatusIs(::google::cloud::StatusCode::kOk, ::testing::_))
      .MatchAndExplain(status, result_listener);
}

}  // namespace testing_util
}  // namespace GOOGLE_CLOUD_CPP_NS
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_TESTING_UTIL_STATUS_MATCHERS_H
