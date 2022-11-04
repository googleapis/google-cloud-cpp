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

#include "google/cloud/internal/port_platform.h"
#include "absl/time/time.h"
#include <chrono>
#include <ostream>

/**
 * Provide `std::chrono::duration` and `std::chrono::time_point` output
 * streaming for pre-C++20 builds (it is standard in C++20 and beyond).
 * This enables the printing of legible chrono values from googletest,
 * rather than raw bytes, when a match fails.
 */
#if GOOGLE_CLOUD_CPP_CPP_VERSION < 202002L
namespace std {
namespace chrono {

template <class Rep, class Period>
std::ostream& operator<<(
    std::ostream& str, std::chrono::duration<Rep, Period> const& value) {
  return str << absl::FromChrono(value);
}

template <class Clock, class Duration>
std::ostream& operator<<(
    std::ostream& str, std::chrono::time_point<Clock, Duration> const& value) {
  return str << absl::FromChrono(value);
}

}  // namespace chrono
}  // namespace std
#endif  // GOOGLE_CLOUD_CPP_CPP_VERSION < 202002L

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_TESTING_UTIL_CHRONO_MATCHERS_H
