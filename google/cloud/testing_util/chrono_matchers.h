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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_TESTING_UTIL_CHRONO_MATCHERS_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_TESTING_UTIL_CHRONO_MATCHERS_H

#include "google/cloud/version.h"
#include "absl/time/time.h"  // NOLINT
#include <gmock/gmock.h>
#include <chrono>

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace testing_util_internal {

bool FormatChronoError(::testing::MatchResultListener& listener, absl::Time arg,
                       char const* compare, absl::Time value);
bool FormatChronoError(::testing::MatchResultListener& listener,
                       absl::Duration arg, char const* compare,
                       absl::Duration value);

}  // namespace testing_util_internal

namespace testing_util {

MATCHER_P(IsChronoEq, value, "Checks whether time points are equal") {
  if (arg == value) return true;
  return testing_util_internal::FormatChronoError(
      *result_listener, absl::FromChrono(arg), "!=", absl::FromChrono(value));
}

MATCHER_P(IsChronoNe, value, "Checks whether time points are not equal") {
  if (arg != value) return true;
  return testing_util_internal::FormatChronoError(
      *result_listener, absl::FromChrono(arg), "==", absl::FromChrono(value));
}

MATCHER_P(
    IsChronoGe, value,
    "Checks whether a time point is greater or equal than a expected value") {
  if (arg >= value) return true;
  return testing_util_internal::FormatChronoError(
      *result_listener, absl::FromChrono(arg), "<", absl::FromChrono(value));
}

MATCHER_P(IsChronoGt, value,
          "Checks whether a time point is greater than a expected value") {
  if (arg > value) return true;
  return testing_util_internal::FormatChronoError(
      *result_listener, absl::FromChrono(arg), "<=", absl::FromChrono(value));
}

MATCHER_P(
    IsChronoLe, value,
    "Checks whether a time point is less than or equal than a expected value") {
  if (arg <= value) return true;
  return testing_util_internal::FormatChronoError(
      *result_listener, absl::FromChrono(arg), ">", absl::FromChrono(value));
}

MATCHER_P(IsChronoLt, value,
          "Checks whether a time point is less than than a expected value") {
  if (arg < value) return true;
  return testing_util_internal::FormatChronoError(
      *result_listener, absl::FromChrono(arg), ">=", absl::FromChrono(value));
}

MATCHER_P(IsChronoDurationEq, value,
          "Checks whether chrono durations are equal") {
  if (arg == value) return true;
  return testing_util_internal::FormatChronoError(
      *result_listener, absl::FromChrono(arg), "!=", absl::FromChrono(value));
}

MATCHER_P(IsChronoDurationNe, value,
          "Checks whether chrono durations are not equal") {
  if (arg != value) return true;
  return testing_util_internal::FormatChronoError(
      *result_listener, absl::FromChrono(arg), "==", absl::FromChrono(value));
}

MATCHER_P(IsChronoDurationGe, value,
          "Checks whether a chrono duration is greater or equal than a "
          "expected value") {
  if (arg >= value) return true;
  return testing_util_internal::FormatChronoError(
      *result_listener, absl::FromChrono(arg), "<", absl::FromChrono(value));
}

MATCHER_P(IsChronoDurationGt, value,
          "Checks whether a chrono duration is greater than a expected value") {
  if (arg > value) return true;
  return testing_util_internal::FormatChronoError(
      *result_listener, absl::FromChrono(arg), "<=", absl::FromChrono(value));
}

MATCHER_P(
    IsChronoDurationLe, value,
    "Checks whether a chrono duration is less or equal than a expected value") {
  if (arg <= value) return true;
  return testing_util_internal::FormatChronoError(
      *result_listener, absl::FromChrono(arg), ">", absl::FromChrono(value));
}

MATCHER_P(IsChronoDurationLt, value,
          "Checks whether a chrono duration is less than a expected value") {
  if (arg < value) return true;
  return testing_util_internal::FormatChronoError(
      *result_listener, absl::FromChrono(arg), ">=", absl::FromChrono(value));
}

}  // namespace testing_util
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_TESTING_UTIL_CHRONO_MATCHERS_H
