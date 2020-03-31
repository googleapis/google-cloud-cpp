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
#include <cstdint>
#include <ostream>
#include <tuple>

namespace google {
namespace cloud {
namespace spanner {
inline namespace SPANNER_CLIENT_NS {

/**
 * Represents a date in the proleptic Gregorian calendar as a triple of
 * year, month (1-12), and day (1-31).
 *
 * Standard C++11 lacks a good "date" abstraction, so we supply a simple
 * implementation of a YMD triple.
 */
class Date {
 public:
  Date(std::int64_t year, int month, int day);  // Fields will be normalized.
  Date() : Date(1970, 1, 1) {}

  std::int64_t year() const { return year_; }
  int month() const { return month_; }
  int day() const { return day_; }

 private:
  std::int64_t year_;
  int month_;
  int day_;
};

std::ostream& operator<<(std::ostream& os, Date const& date);

inline bool operator==(Date const& a, Date const& b) {
  return std::make_tuple(a.year(), a.month(), a.day()) ==
         std::make_tuple(b.year(), b.month(), b.day());
}

inline bool operator!=(Date const& a, Date const& b) { return !(a == b); }

inline bool operator<(Date const& a, Date const& b) {
  return std::make_tuple(a.year(), a.month(), a.day()) <
         std::make_tuple(b.year(), b.month(), b.day());
}

inline bool operator<=(Date const& a, Date const& b) { return !(b < a); }

inline bool operator>=(Date const& a, Date const& b) { return !(a < b); }

inline bool operator>(Date const& a, Date const& b) { return b < a; }

}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_DATE_H
