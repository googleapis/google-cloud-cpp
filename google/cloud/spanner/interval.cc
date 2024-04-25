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

#include "google/cloud/spanner/interval.h"
#include "google/cloud/internal/absl_str_cat_quiet.h"
#include "google/cloud/internal/make_status.h"
#include "absl/strings/ascii.h"
#include "absl/strings/match.h"
#include "absl/strings/numbers.h"
#include "absl/strings/string_view.h"
#include "absl/strings/strip.h"
#include <cmath>
#include <cstdint>
#include <functional>
#include <iomanip>
#include <ostream>
#include <sstream>
#include <string>
#include <utility>

namespace google {
namespace cloud {
namespace spanner {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

namespace {

// Interval factories for quantity units.
struct {
  absl::string_view name;
  std::function<Interval(std::int32_t)> factory;
} const kUnitFactories[] = {
    // "days" is first so that it is chosen when unit is empty.
    {"days", [](auto n) { return Interval(0, 0, n); }},

    // "minutes" comes before other "m"s so it is chosen for, say, "6m".
    {"minutes", [](auto n) { return Interval(absl::Minutes(n)); }},

    {"microseconds", [](auto n) { return Interval(absl::Microseconds(n)); }},
    {"milliseconds", [](auto n) { return Interval(absl::Milliseconds(n)); }},
    {"seconds", [](auto n) { return Interval(absl::Seconds(n)); }},
    {"hours", [](auto n) { return Interval(absl::Hours(n)); }},
    {"weeks", [](auto n) { return Interval(0, 0, n * 7); }},
    {"months", [](auto n) { return Interval(0, n, 0); }},
    {"years", [](auto n) { return Interval(n, 0, 0); }},
    {"decades", [](auto n) { return Interval(n * 10, 0, 0); }},
    {"century", [](auto n) { return Interval(n * 100, 0, 0); }},
    {"centuries", [](auto n) { return Interval(n * 100, 0, 0); }},
    {"millennium", [](auto n) { return Interval(n * 1000, 0, 0); }},
    {"millennia", [](auto n) { return Interval(n * 1000, 0, 0); }},
};

// Interval comparison is done by logically combining the fields into a
// single value by assuming that 1 month == 30 days and 1 day == 24 hours,
// and by rounding to a microsecond boundary.
void Normalize(std::int64_t& months, std::int64_t& days,
               absl::Duration& offset) {
  auto const precision = absl::Microseconds(1);
  offset += ((offset < absl::ZeroDuration()) ? -precision : precision) / 2;
  offset = absl::Trunc(offset, precision);
  days += absl::IDivDuration(offset, absl::Hours(24), &offset);
  if (offset < absl::ZeroDuration()) {
    offset += absl::Hours(24);
    days -= 1;
  }
  months += days / 30;
  days %= 30;
  if (days < 0) {
    days += 30;
    months -= 1;
  }
}

template <template <typename> class C>
bool Cmp(std::int64_t a_months, std::int64_t a_days, absl::Duration a_offset,
         std::int64_t b_months, std::int64_t b_days, absl::Duration b_offset) {
  Normalize(a_months, a_days, a_offset);
  Normalize(b_months, b_days, b_offset);
  if (a_months != b_months) return C<std::int64_t>()(a_months, b_months);
  if (a_days != b_days) return C<std::int64_t>()(a_days, b_days);
  return C<absl::Duration>()(a_offset, b_offset);
}

// "y years m months d days H:M:S.F"
std::string Stringify(std::int32_t months, std::int32_t days,
                      absl::Duration offset) {
  std::ostringstream ss;
  std::int32_t years = months / 12;
  months %= 12;
  if (months < 0) {
    months += 12;
    years -= 1;
  }
  auto plural = [](std::int32_t v) { return std::abs(v) == 1 ? "" : "s"; };
  char const* sep = "";
  if (years != 0) {
    ss << sep << years << " year" << plural(years);
    sep = " ";
  }
  if (months != 0) {
    ss << sep << months << " month" << plural(months);
    sep = " ";
  }
  if (days != 0) {
    ss << sep << days << " day" << plural(days);
    sep = " ";
  }
  if (offset == absl::ZeroDuration()) {
    if (*sep == '\0') ss << "0 days";
  } else {
    char const* sign = (offset < absl::ZeroDuration()) ? "-" : "";
    offset = absl::AbsDuration(offset);
    auto hour = absl::IDivDuration(offset, absl::Hours(1), &offset);
    auto min = absl::IDivDuration(offset, absl::Minutes(1), &offset);
    auto sec = absl::IDivDuration(offset, absl::Seconds(1), &offset);
    ss << sep << sign << std::setfill('0') << std::setw(2) << hour;
    ss << ':' << std::setfill('0') << std::setw(2) << min;
    ss << ':' << std::setfill('0') << std::setw(2) << sec;
    if (auto ns = absl::ToInt64Nanoseconds(offset)) {
      ss << '.' << std::setfill('0');
      if (ns % 1000000 == 0) {
        ss << std::setw(3) << ns / 1000000;
      } else if (ns % 1000 == 0) {
        ss << std::setw(6) << ns / 1000;
      } else {
        ss << std::setw(9) << ns;
      }
    }
  }
  return std::move(ss).str();
}

class Parser {
 public:
  explicit Parser(std::string s) : s_(std::move(s)) {}

  /**
   * https://www.postgresql.org/docs/current/datatype-datetime.html
   *
   * [@] quantity unit [quantity unit...] [direction]
   *
   * where quantity is a number (possibly signed); unit is microsecond,
   * millisecond, second, minute, hour, day, week, month, year, decade,
   * century, millennium, or abbreviations or plurals of these units;
   * direction can be ago or empty. The at sign (@) is optional noise.
   *
   * The amounts of the different units are implicitly added with
   * appropriate sign accounting. ago negates all the fields.
   *
   * Quantities of days, hours, minutes, and seconds can be specified
   * without explicit unit markings. For example, '1 12:59:10' is read
   * the same as '1 day 12 hours 59 min 10 sec'.
   *
   * Also, a combination of years and months can be specified with a
   * dash; for example '200-10' is read the same as '200 years 10 months'.
   */
  StatusOr<Interval> Parse();

 private:
  static void ConsumeSpaces(absl::string_view& s) {
    while (!s.empty() && absl::ascii_isspace(s.front())) s.remove_prefix(1);
  }

  static void ConsumeInteger(absl::string_view& s) {
    while (!s.empty() && absl::ascii_isdigit(s.front())) s.remove_prefix(1);
  }

  static absl::string_view ConsumeSign(absl::string_view& s) {
    auto t = s;
    if (!absl::ConsumePrefix(&s, "-")) absl::ConsumePrefix(&s, "+");
    return t.substr(0, t.size() - s.size());
  }

  static absl::string_view ConsumeWord(absl::string_view& s) {
    auto t = s;
    while (!s.empty() && absl::ascii_isalpha(s.front())) s.remove_prefix(1);
    return t.substr(0, t.size() - s.size());
  }

  static bool ParseDouble(absl::string_view& s, double& d, bool allow_sign) {
    auto t = s;
    if (allow_sign) ConsumeSign(t);
    ConsumeInteger(t);
    if (!absl::ConsumePrefix(&t, ".")) return false;
    ConsumeInteger(t);
    auto len = s.size() - t.size();
    if (!absl::SimpleAtod(s.substr(0, len), &d)) return false;
    s.remove_prefix(len);
    return true;
  }

  template <typename T>
  static bool ParseInteger(absl::string_view& s, T& n, bool allow_sign) {
    auto t = s;
    if (allow_sign) ConsumeSign(t);
    ConsumeInteger(t);
    auto len = s.size() - t.size();
    if (!absl::SimpleAtoi(s.substr(0, len), &n)) return false;
    s.remove_prefix(len);
    return true;
  }

  static bool ParseYearMonth(absl::string_view& s, Interval& intvl) {
    auto t = s;
    std::int32_t year;
    if (!ParseInteger(t, year, true)) return false;
    if (!absl::ConsumePrefix(&t, "-")) return false;
    std::int32_t month;
    if (!ParseInteger(t, month, false)) return false;
    intvl = Interval(year, month, 0);
    s.remove_prefix(s.size() - t.size());
    return true;
  }

  static bool ParseHourMinuteSecond(absl::string_view& s, Interval& intvl) {
    auto t = s;
    auto d = absl::ZeroDuration();
    auto sign = ConsumeSign(t);
    std::int64_t hour;
    if (!ParseInteger(t, hour, false)) return false;
    d += absl::Hours(hour);
    if (!absl::ConsumePrefix(&t, ":")) return false;
    std::int64_t minute;
    if (!ParseInteger(t, minute, false)) return false;
    d += absl::Minutes(minute);
    if (!absl::ConsumePrefix(&t, ":")) return false;
    double second;
    if (ParseDouble(t, second, false)) {
      d += absl::Seconds(second);
    } else {
      std::int64_t second;
      if (!ParseInteger(t, second, false)) return false;
      d += absl::Seconds(second);
    }
    intvl = Interval(sign == "-" ? -d : d);
    s.remove_prefix(s.size() - t.size());
    return true;
  }

  static Interval Unit(absl::string_view& s, std::int32_t n) {
    auto t = s;
    auto unit = ConsumeWord(t);
    if (unit.size() > 1) absl::ConsumeSuffix(&unit, "s");  // unpluralize
    for (auto const& factory : kUnitFactories) {
      if (absl::StartsWith(factory.name, unit)) {
        s.remove_prefix(s.size() - t.size());
        return factory.factory(n);
      }
    }
    return Interval(0, 0, n);  // default to days without removing unit
  }

  std::string s_;
};

StatusOr<Interval> Parser::Parse() {
  Interval intvl;
  auto s = absl::string_view(s_);

  ConsumeSpaces(s);
  if (absl::ConsumePrefix(&s, "@")) {
    ConsumeSpaces(s);
  }

  for (;; ConsumeSpaces(s)) {
    Interval vi;
    if (ParseYearMonth(s, vi)) {
      intvl += vi;
      continue;
    }
    if (ParseHourMinuteSecond(s, vi)) {
      intvl += vi;
      continue;
    }
    double vf;
    if (ParseDouble(s, vf, true)) {
      ConsumeSpaces(s);
      intvl += Unit(s, 1) * vf;
      continue;
    }
    std::int32_t vn;
    if (ParseInteger(s, vn, true)) {
      ConsumeSpaces(s);
      intvl += Unit(s, vn);
      continue;
    }
    break;  // no match
  }

  if (absl::ConsumePrefix(&s, "ago")) {
    ConsumeSpaces(s);
    intvl = -intvl;
  }

  if (!s.empty()) {
    return internal::InvalidArgumentError(
        absl::StrCat("\"", s_, "\": Syntax error at \"", s.substr(0, 5), "\""),
        GCP_ERROR_INFO());
  }
  return intvl;
}

}  // namespace

bool operator==(Interval const& a, Interval const& b) {
  return Cmp<std::equal_to>(a.months_, a.days_, a.offset_,  //
                            b.months_, b.days_, b.offset_);
}

bool operator<(Interval const& a, Interval const& b) {
  return Cmp<std::less>(a.months_, a.days_, a.offset_,  //
                        b.months_, b.days_, b.offset_);
}

Interval Interval::operator-() const {
  Interval intvl;
  intvl.months_ = -months_;
  intvl.days_ = -days_;
  intvl.offset_ = -offset_;
  return intvl;
}

Interval& Interval::operator+=(Interval const& intvl) {
  months_ += intvl.months_;
  days_ += intvl.days_;
  offset_ += intvl.offset_;
  return *this;
}

Interval& Interval::operator*=(double d) {
  // Fractional results only flow down into smaller units. Nothing ever
  // carries up into larger units. This means that '1 month' / 2 becomes
  // '15 days, but '1 month 15 days' * 3 is '3 months 45 days'.
  double i_months;
  double f_months = std::modf(months_ * d, &i_months);
  months_ = static_cast<std::int32_t>(i_months);
  double i_days;
  double f_days = std::modf(days_ * d + f_months * 30, &i_days);
  days_ = static_cast<std::int32_t>(i_days);
  offset_ *= d;
  offset_ += absl::Hours(f_days * 24);
  return *this;
}

Interval::operator std::string() const {
  return Stringify(months_, days_, offset_);
}

StatusOr<Interval> MakeInterval(absl::string_view s) {
  return Parser(absl::AsciiStrToLower(s)).Parse();
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace spanner
}  // namespace cloud
}  // namespace google
