// Copyright 2017 Google Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_TESTING_UTIL_CHRONO_LITERALS_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_TESTING_UTIL_CHRONO_LITERALS_H

#include "google/cloud/version.h"
#include <chrono>

// TODO(#109) - these are generally useful, consider submitting to abseil.io
namespace google {
namespace cloud {
inline namespace GOOGLE_CLOUD_CPP_NS {
namespace testing_util {
namespace chrono_literals {
std::chrono::hours constexpr operator"" _h(unsigned long long h) {
  return std::chrono::hours(h);
}

std::chrono::minutes constexpr operator"" _min(unsigned long long m) {
  return std::chrono::minutes(m);
}

std::chrono::seconds constexpr operator"" _s(unsigned long long s) {
  return std::chrono::seconds(s);
}

std::chrono::milliseconds constexpr operator"" _ms(unsigned long long ms) {
  return std::chrono::milliseconds(ms);
}

std::chrono::microseconds constexpr operator"" _us(unsigned long long us) {
  return std::chrono::microseconds(us);
}

std::chrono::nanoseconds constexpr operator"" _ns(unsigned long long ns) {
  return std::chrono::nanoseconds(ns);
}

}  // namespace chrono_literals
}  // namespace testing_util
}  // namespace GOOGLE_CLOUD_CPP_NS
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_TESTING_UTIL_CHRONO_LITERALS_H
