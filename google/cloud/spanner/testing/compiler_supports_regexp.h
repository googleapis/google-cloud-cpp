// Copyright 2020 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_TESTING_COMPILER_SUPPORTS_REGEXP_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_TESTING_COMPILER_SUPPORTS_REGEXP_H

#include "google/cloud/spanner/version.h"

namespace google {
namespace cloud {
namespace spanner_testing {
inline namespace SPANNER_CLIENT_NS {
inline bool CompilerSupportsRegexp() {
#if !defined(__clang__) && defined(__GNUC__) && \
    (__GNUC__ < 4 || (__GNUC__ == 4 && __GNUC_MINOR__ < 9))
  // gcc-4.8 ships with a broken regexp library, it compiles, but does not match
  // correctly:
  //    https://stackoverflow.com/questions/12530406/is-gcc-4-8-or-earlier-buggy-about-regular-expressions
  return false;
#else
  return true;
#endif
}

}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner_testing
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_TESTING_COMPILER_SUPPORTS_REGEXP_H
