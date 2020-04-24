// Copyright 2019 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_TESTING_UTIL_ASSERT_OK_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_TESTING_UTIL_ASSERT_OK_H

#include "google/cloud/status.h"
#include "google/cloud/status_or.h"
#include <gtest/gtest.h>

namespace testing {
namespace internal {

// A unary predicate-formatter for google::cloud::Status.
testing::AssertionResult IsOkPredFormat(char const* expr,
                                        ::google::cloud::Status const& status);

// A unary predicate-formatter for google::cloud::StatusOr<T>.
template <typename T>
testing::AssertionResult IsOkPredFormat(
    char const* expr, ::google::cloud::StatusOr<T> const& status_or) {
  if (status_or) {
    return testing::AssertionSuccess();
  }
  return IsOkPredFormat(expr, status_or.status());
}

}  // namespace internal
}  // namespace testing

#define ASSERT_STATUS_OK(val) \
  ASSERT_PRED_FORMAT1(::testing::internal::IsOkPredFormat, val)

#define EXPECT_STATUS_OK(val) \
  EXPECT_PRED_FORMAT1(::testing::internal::IsOkPredFormat, val)

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_TESTING_UTIL_ASSERT_OK_H
