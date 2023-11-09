// Copyright 2020 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// https://www.apache.org/licenses/LICENSE-2.0
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
#include <type_traits>

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
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
    // failing expectation does not require further explanation.
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

/*
 * Monomorphic implementation of the IsOkAndHolds() matcher.
 */
template <typename S>
class IsOkAndHoldsMatcherImpl : public ::testing::MatcherInterface<S> {
 public:
  using status_or_type = std::remove_reference_t<S>;
  using value_type = typename status_or_type::value_type;

  template <typename ValueMatcher>
  // NOLINTNEXTLINE(bugprone-forwarding-reference-overload)
  explicit IsOkAndHoldsMatcherImpl(ValueMatcher&& value_matcher)
      : value_matcher_(::testing::MatcherCast<value_type const&>(
            std::forward<ValueMatcher>(value_matcher))) {}

  bool MatchAndExplain(
      status_or_type const& value,
      ::testing::MatchResultListener* listener) const override {
    // Because StatusOr<T> does not have a printer, gMock will render the
    // value using RawBytesPrinter as "N-byte object <...>", which is not
    // very useful. Accordingly, we print the enclosed Status so that a
    // failing expectation does not require further explanation.
    Status const& status = value.status();
    *listener << "whose status is " << ::testing::PrintToString(status);

    if (!listener->IsInterested()) {
      return value && value_matcher_.Matches(*value);
    }

    auto* os = listener->stream();
    *os << ", with a code that " << (value ? "is" : "isn't") << " equal to OK";
    if (!value) return false;

    ::testing::StringMatchResultListener value_listener;
    auto const match = value_matcher_.MatchAndExplain(*value, &value_listener);
    auto const explanation = value_listener.str();
    *os << ", " << (match ? "and" : "but") << " a value ";
    if (!explanation.empty()) {
      *os << explanation;
    } else {
      *os << "that ";
      if (match) {
        value_matcher_.DescribeTo(os);
      } else {
        value_matcher_.DescribeNegationTo(os);
      }
    }
    return match;
  }

  void DescribeTo(std::ostream* os) const override {
    *os << "code is equal to OK and value ";
    value_matcher_.DescribeTo(os);
  }

  void DescribeNegationTo(std::ostream* os) const override {
    *os << "code isn't equal to OK or value ";
    value_matcher_.DescribeNegationTo(os);
  }

 private:
  ::testing::Matcher<value_type const&> const value_matcher_;
};

/*
 * Implementation of the IsOkAndHolds() matcher for a StatusOr<T>, or a
 * reference to one.
 */
template <typename ValueMatcher>
class IsOkAndHoldsMatcher {
 public:
  explicit IsOkAndHoldsMatcher(ValueMatcher value_matcher)
      : value_matcher_(std::move(value_matcher)) {}

  // Converts to a monomorphic matcher of the given StatusOr type.
  template <typename S>
  // NOLINTNEXTLINE(bugprone-forwarding-reference-overload)
  // NOLINTNEXTLINE(google-explicit-constructor)
  operator ::testing::Matcher<S>() const {
    return ::testing::Matcher<S>(
        new IsOkAndHoldsMatcherImpl<S const&>(value_matcher_));
  }

 private:
  ValueMatcher const value_matcher_;
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
 * Returns a gMock matcher that matches a `StatusOr<T>` whose code is OK and
 * whose value matches the `value_matcher`.
 *
 * @par Example:
 * @code
 *   StatusOr<T> v = ...;
 *   EXPECT_THAT(v, IsOkAndHolds(T{...}));
 * @endcode
 */
template <typename ValueMatcher>
testing_util_internal::IsOkAndHoldsMatcher<typename std::decay_t<ValueMatcher>>
IsOkAndHolds(ValueMatcher&& value_matcher) {
  return testing_util_internal::IsOkAndHoldsMatcher<
      typename std::decay_t<ValueMatcher>>(
      std::forward<ValueMatcher>(value_matcher));
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
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_TESTING_UTIL_STATUS_MATCHERS_H
