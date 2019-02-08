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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_TESTING_UTIL_ASSERT_OK_H_
#define GOOGLE_CLOUD_CPP_GOOGLE_TESTING_UTIL_ASSERT_OK_H_

#include "google/cloud/status.h"
#include <gtest/gtest.h>

namespace testing {
namespace internal {

// A unary predicate-formatter for google::cloud::Status.
testing::AssertionResult PredFormatStatus(
    const char* expr, const ::google::cloud::Status& status) {
  if (status.ok()) {
    return testing::AssertionSuccess();
  }
  return testing::AssertionFailure()
         << "Status of \"" << expr
         << "\" is expected to be OK, but evaluates to \"" << status.message()
         << "\" (code " << status.code() << ")";
}

}  // namespace internal
}  // namespace testing

#define ASSERT_OK(val) \
  ASSERT_PRED_FORMAT1(::testing::internal::PredFormatStatus, val)

#define EXPECT_OK(val) \
  EXPECT_PRED_FORMAT1(::testing::internal::PredFormatStatus, val)

#endif /* GOOGLE_CLOUD_CPP_GOOGLE_TESTING_UTIL_ASSERT_OK_H_ */
