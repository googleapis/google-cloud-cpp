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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_DATE_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_DATE_H

#include "google/cloud/spanner/version.h"
#include "absl/time/civil_time.h"

namespace google {
namespace cloud {
namespace spanner {
inline namespace SPANNER_CLIENT_NS {

/**
 * Represents a date in the proleptic Gregorian calendar as a triple of
 * year, month (1-12), and day (1-31).
 *
 * We use `absl::CivilDay` to represent a "date". Direct use of
 * `absl::CivilDay` is recommended. This alias exists for backward
 * compatibility.
 *
 * @see https://abseil.io/docs/cpp/guides/time#civil-times
 */
using Date = ::absl::CivilDay;

}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_DATE_H
