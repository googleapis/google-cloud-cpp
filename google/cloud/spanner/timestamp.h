// Copyright 2019 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_SPANNER_GOOGLE_CLOUD_SPANNER_TIMESTAMP_H_
#define GOOGLE_CLOUD_CPP_SPANNER_GOOGLE_CLOUD_SPANNER_TIMESTAMP_H_

#include "google/cloud/spanner/version.h"
#include <chrono>

namespace google {
namespace cloud {
namespace spanner {
inline namespace SPANNER_CLIENT_NS {

/**
 * An instant in time. A std::chrono::time_point using the system (walltime)
 * clock, represented as a signed integer count (>= 64 bits) of nanoseconds
 * from the clock's epoch.
 *
 * Most implementations use an epoch of 1970-01-01T00:00:00+00:00, where the
 * range of values would be about 1677-09-21 through to 2262-04-11.
 *
 * Note that this may differ from std::chrono::system_clock::time_point on a
 * particular platform.
 */
using Timestamp = std::chrono::time_point<std::chrono::system_clock,
                                          std::chrono::nanoseconds>;

}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_SPANNER_GOOGLE_CLOUD_SPANNER_TIMESTAMP_H_
