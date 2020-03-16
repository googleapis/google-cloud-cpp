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

#include "google/cloud/spanner/date.h"
#include "google/cloud/spanner/internal/date.h"
#include <array>

namespace google {
namespace cloud {
namespace spanner {
inline namespace SPANNER_CLIENT_NS {

namespace {

bool IsLeapYear(std::int64_t y) {
  return y % 4 == 0 && (y % 100 != 0 || y % 400 == 0);
}

int DaysPerMonth(std::int64_t y, int m) {
  static std::array<int, 1 + 12> const kKDaysPerMonth = {{
      -1, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31  // non leap year
  }};
  return kKDaysPerMonth[m] + (m == 2 && IsLeapYear(y) ? 1 : 0);
}

}  // namespace

Date::Date(std::int64_t year, int month, int day)
    : year_(year), month_(month), day_(day) {
  year_ += month_ / 12;  // 12 months == 1 year
  month_ %= 12;
  if (month_ <= 0) {
    year_ -= 1;
    month_ += 12;
  }
  year_ += (day_ / 146097) * 400;  // 146097 days == 400 years
  day_ %= 146097;
  if (day_ <= 0) {
    year_ -= 400;
    day_ += 146097;
  }
  for (int n = DaysPerMonth(year_, month_); day_ > n;
       n = DaysPerMonth(year_, month_)) {
    day_ -= n;
    if (++month_ > 12) {
      year_ += 1;
      month_ = 1;
    }
  }
}

std::ostream& operator<<(std::ostream& os, Date const& date) {
  return os << internal::DateToString(date);
}

}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner
}  // namespace cloud
}  // namespace google
