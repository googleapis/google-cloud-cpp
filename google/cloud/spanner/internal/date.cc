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

#include "google/cloud/spanner/internal/date.h"
#include <array>
#include <cinttypes>
#include <cstdio>

namespace google {
namespace cloud {
namespace spanner {
inline namespace SPANNER_CLIENT_NS {
namespace internal {

std::string DateToString(Date d) {
  std::array<char, sizeof "-9223372036854775808-01-01"> buf;
  std::snprintf(buf.data(), buf.size(), "%04" PRId64 "-%02d-%02d", d.year(),
                d.month(), d.day());
  return std::string(buf.data());
}

#include "google/cloud/internal/disable_msvc_crt_secure_warnings.inc"
StatusOr<Date> DateFromString(std::string const& s) {
  std::int64_t year;
  int month;
  int day;
  char c;
  switch (sscanf(s.c_str(), "%" SCNd64 "-%d-%d%c", &year, &month, &day, &c)) {
    case 3:
      break;
    case 4:
      return Status(StatusCode::kInvalidArgument,
                    s + ": Extra data after RFC3339 full-date");
    default:
      return Status(StatusCode::kInvalidArgument,
                    s + ": Failed to match RFC3339 full-date");
  }
  Date date(year, month, day);
  if (date.month() != month || date.day() != day) {
    return Status(StatusCode::kInvalidArgument,
                  s + ": RFC3339 full-date field out of range");
  }
  return date;
}
#include "google/cloud/internal/diagnostics_pop.inc"

}  // namespace internal
}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner
}  // namespace cloud
}  // namespace google
