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

#include "google/cloud/testing_util/assert_ok.h"

namespace testing {
namespace internal {

// A unary predicate-formatter for google::cloud::Status.
testing::AssertionResult IsOkPredFormat(char const* expr,
                                        ::google::cloud::Status const& status) {
  if (status.ok()) {
    return testing::AssertionSuccess();
  }
  return testing::AssertionFailure()
         << "Value of: " << expr << "\nExpected: is OK\nActual: " << status;
}

}  // namespace internal
}  // namespace testing
