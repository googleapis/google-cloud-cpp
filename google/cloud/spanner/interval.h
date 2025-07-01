// Copyright 2024 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_INTERVAL_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_INTERVAL_H

#include "google/cloud/spanner/timestamp.h"
#include "google/cloud/spanner/version.h"
#include "google/cloud/status_or.h"
#include "absl/strings/string_view.h"
#include <chrono>
#include <cstdint>
#include <iosfwd>
#include <string>

namespace google {
namespace cloud {
namespace spanner {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

/**
 * A representation of the Spanner INTERVAL type: The difference between
 * two date/time values.
 *
 * @see https://cloud.google.com/spanner/docs/data-types#interval_type
 */
class Interval {
  friend class IntervalTest;

 public:
  /// Default construction yields a zero-length interval.
  Interval() : Interval(0, 0, 0) {}

  Interval(std::int32_t years, std::int32_t months, std::int32_t days,
           std::chrono::nanoseconds offset = std::chrono::nanoseconds::zero())
      : Interval((years * 12) + months, days, offset) {}

  explicit Interval(std::chrono::nanoseconds offset) : Interval(0, 0, offset) {}

  /// @name Regular value type, supporting copy, assign, move.
  ///@{
  Interval(Interval&&) = default;
  Interval& operator=(Interval&&) = default;
  Interval(Interval const&) = default;
  Interval& operator=(Interval const&) = default;
  ///@}

  /// @name Relational operators
  ///
  /// Beware: Interval comparisons assume that 1 month == 30 days, and
  /// 1 day == 24*60*60 seconds. This may lead to counterintuitive results.
  /// It also means different Interval representations can compare equal,
  /// e.g., Interval(0, 0, 1) == Interval(std::chrono::hours(24)). Also
  /// note that offsets are rounded to 1-microsecond boundaries during
  /// comparisons (halfway cases rounding away from zero).
  ///@{
  friend bool operator==(Interval const& a, Interval const& b);
  friend bool operator!=(Interval const& a, Interval const& b) {
    return !(a == b);
  }
  friend bool operator<(Interval const& a, Interval const& b);
  friend bool operator<=(Interval const& a, Interval const& b) {
    return !(b < a);
  }
  friend bool operator>=(Interval const& a, Interval const& b) {
    return !(a < b);
  }
  friend bool operator>(Interval const& a, Interval const& b) {
    return (b < a);
  }
  ///@}

  /// @name Conversion to a string using ISO8601 duration format.
  explicit operator std::string() const;

  /// @name Output streaming
  friend std::ostream& operator<<(std::ostream& os, Interval intvl) {
    return os << std::string(intvl);
  }

  /// @name Negation
  Interval operator-() const;

 private:
  Interval(std::int32_t months, std::int32_t days,
           std::chrono::nanoseconds offset)
      : months_(months), days_(days), offset_(offset) {}

  static StatusOr<Interval> ParseISO8601Interval(absl::string_view str);
  static StatusOr<Interval> ParsePostgreSqlInterval(absl::string_view str);

  friend Interval JustifyDays(Interval);
  friend Interval JustifyHours(Interval);
  friend StatusOr<Interval> MakeInterval(absl::string_view);
  friend class IntervalTest;

  // Beware: Fractional multiplication similarly assumes 30-days months
  // and 24-hour days, and may lead to counterintuitive results.
  Interval& operator+=(Interval const&);
  Interval& operator-=(Interval const& intvl) {
    *this += -intvl;
    return *this;
  }
  Interval& operator*=(double);
  Interval operator+(Interval rhs) const { return Interval(*this) += rhs; }
  Interval operator-(Interval rhs) const { return Interval(*this) -= rhs; }
  Interval operator*(double rhs) const { return Interval(*this) *= rhs; }

  std::int32_t months_;
  std::int32_t days_;
  std::chrono::nanoseconds offset_;
};

/**
 * Construct an `Interval` from a string. At least handles the format
 * produced by `Interval::operator std::string()`.
 */
StatusOr<Interval> MakeInterval(absl::string_view);

/**
 * Adjust the interval so that 30-day periods are represented as months.
 * For example, maps "35 days" to "1 month 5 days".
 */
Interval JustifyDays(Interval);

/**
 * Adjust the interval so that 24-hour periods are represented as days.
 * For example, maps "27 hours" to "1 day 3 hours".
 */
Interval JustifyHours(Interval);

/**
 * Adjust the interval using both JustifyDays() and JustifyHours().
 * For example, maps "1 month -1 hour" to "29 days 23 hours".
 */
Interval JustifyInterval(Interval);

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace spanner
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_INTERVAL_H
