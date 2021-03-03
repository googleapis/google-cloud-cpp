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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_TESTING_UTIL_EXPECT_FUTURE_ERROR_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_TESTING_UTIL_EXPECT_FUTURE_ERROR_H

#include "google/cloud/version.h"
#include <gmock/gmock.h>
#include <future>
#include <string>

namespace google {
namespace cloud {
inline namespace GOOGLE_CLOUD_CPP_NS {
namespace testing_util {
/**
 * Verify that a given functor raises `std::future_error` with the right error
 * code.
 *
 * @param functor the functor to call, typically a lambda that will perform the
 *     operation under test.
 * @param code the expected error code.
 * @tparam Functor the type of @p functor.
 */
template <typename Functor>
void ExpectFutureError(Functor functor, std::future_errc code) {
#if GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
  EXPECT_THROW(
      try { functor(); } catch (std::future_error const& ex) {
        EXPECT_EQ(code, ex.code());
        throw;
      },
      std::future_error);
#else
  std::string expected = "future_error\\[";
  expected += std::make_error_code(code).message();
  expected += "\\]";
  EXPECT_DEATH_IF_SUPPORTED(functor(), expected);
#endif  // GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
}

}  // namespace testing_util
}  // namespace GOOGLE_CLOUD_CPP_NS
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_TESTING_UTIL_EXPECT_FUTURE_ERROR_H
