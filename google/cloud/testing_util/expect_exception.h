// Copyright 2018 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_TESTING_UTIL_EXPECT_EXCEPTION_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_TESTING_UTIL_EXPECT_EXCEPTION_H

#include "google/cloud/version.h"
#include <gmock/gmock.h>
#include <future>

namespace google {
namespace cloud {
inline namespace GOOGLE_CLOUD_CPP_NS {
namespace testing_util {
/**
 * Verify that a given functor raises `T` and run a test if it does.
 *
 * `EXPECT_THROW()` validates the type of an exception, but does not validate
 * its contents. Most of the time the type is enough, but in some tests need to
 * be more detailed. The common idiom in that case is to write an explicit
 * try/catch to validate the values and then rethrow:
 *
 * @code
 * EXPECT_THROW(
 *     try { something_that_throws(); }
 *     catch(ExpectedException const& ex) {
 *       // Validate the contents of ex, e.g. EXPECT_EQ(..., ex.what());
 *       throw;
 *     }, ExpectedException);
 * @endcode
 *
 * That works, but we need to compile (and run) the tests with
 * `-fno-exceptions`. In that case the test becomes:
 *
 * @code
 * EXPECT_DEATH_IF_SUPPORTED(something_that_throws(), "expected death message");
 * @endcode
 *
 * And, of course, these two cases need a `#%ifdef` to detect if exceptions are
 * enabled. This was getting tedious, so we refactor to this function. The
 * expected use is:
 *
 * @code
 * ExpectException<MyException>(
 *     [&] { something_that_throws(); },
 *     [&](MyException const& ex) {
 *         EXPECT_EQ("everything is terrible", ex.what());
 *     },
 *     "terminating program: everything is terrible");
 * @endcode
 *
 *
 * @param expression the expression (typically a lambda) that is expected to
 *     raise an exception of type `ExpectedException`.
 * @param validator a functor to validate the exception contents.
 * @param expected_message if exceptions are disabled the program is expected
 *     to terminate, and @p expected_message is a regular expression matched
 *     against the program output.
 * @tparam ExpectedException the type of exception.
 */
template <typename ExpectedException>
void ExpectException(
    std::function<void()> const& expression,
    std::function<void(ExpectedException const& ex)> const& validator,
    char const* expected_message) {
#if GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
  EXPECT_THROW(
      try { expression(); } catch (ExpectedException const& ex) {
        validator(ex);
        throw;
      },
      ExpectedException);
  (void)expected_message;  // suppress clang-tidy warning.
#else
  (void)validator;  // suppress clang-tidy warning.
  EXPECT_DEATH_IF_SUPPORTED(expression(), expected_message);
#endif  // GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
}

/**
 * Verify that an expression does not throw.
 *
 * Testing `void` expressions is tedious because `EXPECT_NO_THROW` does not
 * compile when exceptions are disabled. Writing `expression()` in a test does
 * detect exceptions, but does not express the intent.
 *
 * @param expression the expression (typically a lambda) that we want to verify
 *     does not throw.
 */
inline void ExpectNoException(std::function<void()> const& expression) {
#if GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
  EXPECT_NO_THROW(expression());
#else
  EXPECT_NO_FATAL_FAILURE(expression());
#endif  // GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
}

}  // namespace testing_util
}  // namespace GOOGLE_CLOUD_CPP_NS
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_TESTING_UTIL_EXPECT_EXCEPTION_H
