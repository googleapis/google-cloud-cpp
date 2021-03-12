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
#include <string>

namespace google {
namespace cloud {
inline namespace GOOGLE_CLOUD_CPP_NS {
namespace testing_util_internal {

/*
 * Implementation of the StatusIs() matcher for a Status, a StatusOr<T>,
 * or a reference to either of them.
 */
class StatusIsMatcher {
 public:
  template <typename CodeMatcher, typename MessageMatcher>
  StatusIsMatcher(CodeMatcher&& code_matcher, MessageMatcher&& message_matcher)
      : code_matcher_(::testing::MatcherCast<StatusCode>(
            std::forward<CodeMatcher>(code_matcher))),
        message_matcher_(::testing::MatcherCast<std::string const&>(
            std::forward<MessageMatcher>(message_matcher))) {}

  bool MatchAndExplain(Status const& status,
                       ::testing::MatchResultListener* listener) const;

  template <typename T>
  bool MatchAndExplain(StatusOr<T> const& value,
                       ::testing::MatchResultListener* listener) const {
    // Because StatusOr<T> does not have a printer, gMock will render the
    // value using RawBytesPrinter as "N-byte object <...>", which is not
    // very useful. Accordingly, we print the enclosed Status so that a
    // failing expection does not require further explanation.
    Status const& status = value.status();
    *listener << "whose status is " << ::testing::PrintToString(status);

    ::testing::StringMatchResultListener inner_listener;
    auto const match = MatchAndExplain(status, &inner_listener);
    auto const explanation = inner_listener.str();
    if (!explanation.empty()) *listener << ", " << explanation;
    return match;
  }

  void DescribeTo(std::ostream* os) const;
  void DescribeNegationTo(std::ostream* os) const;

 private:
  ::testing::Matcher<StatusCode> const code_matcher_;
  ::testing::Matcher<std::string const&> const message_matcher_;
};

}  // namespace testing_util_internal

namespace testing_util {

/**
 * Returns a gMock matcher that matches a `Status` or `StatusOr<T>` whose
 * code matches `code_matcher` and whose message matches `message_matcher`.
 *
 * @par Example:
 * @code
 *   Status status = ...;
 *   EXPECT_THAT(status, StatusIs(StatusCode::kInvalidArgument,
 *                                testing::HasSubstr("no rows"));
 * @endcode
 */
template <typename CodeMatcher, typename MessageMatcher>
::testing::PolymorphicMatcher<testing_util_internal::StatusIsMatcher> StatusIs(
    CodeMatcher&& code_matcher, MessageMatcher&& message_matcher) {
  return ::testing::MakePolymorphicMatcher(
      testing_util_internal::StatusIsMatcher(
          std::forward<CodeMatcher>(code_matcher),
          std::forward<MessageMatcher>(message_matcher)));
}

/**
 * Returns a gMock matcher that matches a `Status` or `StatusOr<T>` whose
 * code matches `code_matcher` and whose message matches anything.
 *
 * @par Example:
 * @code
 *   Status status = ...;
 *   EXPECT_THAT(status, StatusIs(StatusCode::kInvalidArgument));
 * @endcode
 */
template <typename CodeMatcher>
::testing::PolymorphicMatcher<testing_util_internal::StatusIsMatcher> StatusIs(
    CodeMatcher&& code_matcher) {
  return StatusIs(std::forward<CodeMatcher>(code_matcher), ::testing::_);
}

/**
 * Returns a gMock matcher that matches a `Status` or `StatusOr<T>` whose
 * code is OK and whose message matches anything.
 *
 * @par Example:
 * @code
 *   Status status = ...;
 *   EXPECT_THAT(status, IsOk());
 * @endcode
 */
inline ::testing::PolymorphicMatcher<testing_util_internal::StatusIsMatcher>
IsOk() {
  // We could use ::testing::IsEmpty() here, but historically have not.
  return StatusIs(StatusCode::kOk, ::testing::_);
}

/**
 * Expectations that a `Status` or `StatusOr<T>` has an OK code.
 *
 * @par Example:
 * @code
 *   Status status = ...;
 *   EXPECT_STATUS_OK(status);
 * @endcode
 */
#define EXPECT_STATUS_OK(expression) \
  EXPECT_THAT(expression, ::google::cloud::testing_util::IsOk())
#define ASSERT_STATUS_OK(expression) \
  ASSERT_THAT(expression, ::google::cloud::testing_util::IsOk())

}  // namespace testing_util
}  // namespace GOOGLE_CLOUD_CPP_NS
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_TESTING_UTIL_STATUS_MATCHERS_H
