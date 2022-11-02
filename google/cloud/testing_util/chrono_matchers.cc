// Copyright 2022 Google LLC
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

#include "google/cloud/testing_util/chrono_matchers.h"
#include <iostream>

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace testing_util_internal {

bool FormatChronoError(testing::MatchResultListener& listener, absl::Time arg,
                       char const* compare, absl::Time value) {
  listener << absl::FormatTime(arg) << " " << compare << " "
           << absl::FormatTime(value);
  return false;
}

bool FormatChronoError(testing::MatchResultListener& listener,
                       absl::Duration arg, char const* compare,
                       absl::Duration value) {
  listener << absl::FormatDuration(arg) << " " << compare << " "
           << absl::FormatDuration(value);
  return false;
}

}  // namespace testing_util_internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google
