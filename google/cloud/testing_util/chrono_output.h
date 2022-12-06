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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_TESTING_UTIL_CHRONO_OUTPUT_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_TESTING_UTIL_CHRONO_OUTPUT_H

#include "google/cloud/internal/type_traits.h"
#include "google/cloud/version.h"
#include "absl/time/time.h"
#include <chrono>
#include <ostream>

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {

template <class Rep, class Period>
constexpr bool kIsChronoDurationOstreamable =
    google::cloud::internal::IsOStreamable<
        std::chrono::duration<Rep, Period>>::value;

template <class Clock, class Duration>
constexpr bool kIsChronoTimePointOstreamable =
    google::cloud::internal::IsOStreamable<
        std::chrono::time_point<Clock, Duration>>::value;

}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google

/**
 * Provide `std::chrono::duration` and `std::chrono::time_point` output
 * streaming when otherwise unsupported.
 *
 * It is strictly undefined behavior to add to any `std` namespace, but
 * it works, and, in this test-only case, is well worth it.
 */
namespace std {
namespace chrono {

template <class Rep, class Period,
          typename = typename std::enable_if<
              !google::cloud::internal::kIsChronoDurationOstreamable<
                  Rep, Period>>::type>
std::ostream& operator<<(  //
    std::ostream& str, std::chrono::duration<Rep, Period> const& value) {
  return str << absl::FromChrono(value);
}

template <class Clock, class Duration,
          typename = typename std::enable_if<
              !google::cloud::internal::kIsChronoTimePointOstreamable<
                  Clock, Duration>>::type>
std::ostream& operator<<(
    std::ostream& str, std::chrono::time_point<Clock, Duration> const& value) {
  return str << absl::FromChrono(value);
}

}  // namespace chrono
}  // namespace std

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_TESTING_UTIL_CHRONO_OUTPUT_H
