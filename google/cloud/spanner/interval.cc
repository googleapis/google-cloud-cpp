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
#include "google/cloud/internal/time_utils.h"
#include "absl/strings/ascii.h"
#include "absl/strings/match.h"
#include "absl/strings/numbers.h"
#include "absl/strings/string_view.h"
#include "absl/strings/strip.h"
#include "absl/time/time.h"
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

using std::chrono::duration;
using std::chrono::duration_cast;
using std::chrono::hours;
using std::chrono::microseconds;
using std::chrono::milliseconds;
using std::chrono::minutes;
using std::chrono::nanoseconds;
using std::chrono::seconds;

// Interval factories for quantity units.
struct {
  absl::string_view name;  // de-pluralized forms
  std::function<Interval(std::int32_t)> factory;
} const kUnitFactories[] = {
    // "day" is first so that it is chosen when unit is empty.
    {"day", [](auto n) { return Interval(0, 0, n); }},

    // "minute" comes before other "m"s so it is chosen for, say, "6m".
    {"minute", [](auto n) { return Interval{minutes(n)}; }},

    {"microsecond", [](auto n) { return Interval{microseconds(n)}; }},
    {"millisecond", [](auto n) { return Interval{milliseconds(n)}; }},
    {"second", [](auto n) { return Interval{seconds(n)}; }},
    {"hour", [](auto n) { return Interval{hours(n)}; }},
    {"week", [](auto n) { return Interval(0, 0, n * 7); }},
    {"month", [](auto n) { return Interval(0, n, 0); }},
    {"year", [](auto n) { return Interval(n, 0, 0); }},
    {"decade", [](auto n) { return Interval(n * 10, 0, 0); }},
    {"century", [](auto n) { return Interval(n * 100, 0, 0); }},
    {"centurie", [](auto n) { return Interval(n * 100, 0, 0); }},
    {"millennium", [](auto n) { return Interval(n * 1000, 0, 0); }},
    {"millennia", [](auto n) { return Interval(n * 1000, 0, 0); }},
};

// Round d to a microsecond boundary (to even in halfway cases).
microseconds Round(nanoseconds d) {
  auto trunc = duration_cast<microseconds>(d);
  auto diff = d < nanoseconds::zero() ? trunc - d : d - trunc;
  if (diff < nanoseconds(500)) return trunc;
  auto after = trunc + microseconds(d < nanoseconds::zero() ? -1 : 1);
  if (diff > nanoseconds(500)) return after;
  return (after.count() & 1) ? trunc : after;
}

// Interval comparison is done by logically combining the fields into a
// single value by assuming that 1 month == 30 days and 1 day == 24 hours,
// and by rounding to a microsecond boundary.
microseconds Normalize(std::int64_t& months, std::int64_t& days,
                       nanoseconds offset) {
  auto rounded_offset = Round(offset);
  auto carry_days = rounded_offset / hours(24);
  days += carry_days;
  rounded_offset -= hours(carry_days * 24);
  if (rounded_offset < microseconds::zero()) {
    rounded_offset += hours(24);
    days -= 1;
  }
  months += days / 30;
  days %= 30;
  if (days < 0) {
    days += 30;
    months -= 1;
  }
  return rounded_offset;
}

template <template <typename> class C>
bool Cmp(std::int64_t a_months, std::int64_t a_days, nanoseconds a_offset,
         std::int64_t b_months, std::int64_t b_days, nanoseconds b_offset) {
  auto rounded_a_offset = Normalize(a_months, a_days, a_offset);
  auto rounded_b_offset = Normalize(b_months, b_days, b_offset);
  if (a_months != b_months) return C<std::int64_t>()(a_months, b_months);
  if (a_days != b_days) return C<std::int64_t>()(a_days, b_days);
  return C<microseconds>()(rounded_a_offset, rounded_b_offset);
}

// "y years m months d days H:M:S.F"
std::string SerializeInterval(std::int32_t months, std::int32_t days,
                              nanoseconds offset) {
  std::ostringstream ss;
  std::int32_t years = months / 12;
  months %= 12;
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
  if (offset == nanoseconds::zero()) {
    if (*sep == '\0') ss << "0 days";
  } else {
    ss << sep;
    nanoseconds::rep nanosecond_carry = 0;
    if (offset < nanoseconds::zero()) {
      ss << "-";
      if (offset == nanoseconds::min()) {
        // Handle the inability to negate the most negative value.
        // This works because no power of 2 is a multiple of 10,
        // so the carry will always remain within the same second.
        offset += nanoseconds(1);
        nanosecond_carry = 1;
      }
      offset = -offset;
    }
    auto hour = duration_cast<hours>(offset);
    offset -= hour;
    auto min = duration_cast<minutes>(offset);
    offset -= min;
    auto sec = duration_cast<seconds>(offset);
    offset -= sec;
    ss << std::setfill('0') << std::setw(2) << hour.count();
    ss << ':' << std::setfill('0') << std::setw(2) << min.count();
    ss << ':' << std::setfill('0') << std::setw(2) << sec.count();
    if (auto ns = offset.count() + nanosecond_carry) {
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

void ConsumeSpaces(absl::string_view& s) {
  while (!s.empty() && absl::ascii_isspace(s.front())) s.remove_prefix(1);
}

void ConsumeInteger(absl::string_view& s) {
  while (!s.empty() && absl::ascii_isdigit(s.front())) s.remove_prefix(1);
}

absl::string_view ConsumeSign(absl::string_view& s) {
  auto t = s;
  if (!absl::ConsumePrefix(&s, "-")) absl::ConsumePrefix(&s, "+");
  return t.substr(0, t.size() - s.size());
}

absl::string_view ConsumeWord(absl::string_view& s) {
  auto t = s;
  while (!s.empty() && absl::ascii_isalpha(s.front())) s.remove_prefix(1);
  return t.substr(0, t.size() - s.size());
}

bool ParseDouble(absl::string_view& s, double& d, bool allow_sign) {
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
bool ParseInteger(absl::string_view& s, T& n, bool allow_sign) {
  auto t = s;
  if (allow_sign) ConsumeSign(t);
  ConsumeInteger(t);
  auto len = s.size() - t.size();
  if (!absl::SimpleAtoi(s.substr(0, len), &n)) return false;
  s.remove_prefix(len);
  return true;
}

bool ParseYearMonth(absl::string_view& s, Interval& intvl) {
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

bool ParseHourMinuteSecond(absl::string_view& s, Interval& intvl) {
  auto t = s;
  auto d = nanoseconds::zero();
  auto sign = ConsumeSign(t);
  std::int64_t hour;
  if (!ParseInteger(t, hour, false)) return false;
  d += hours(hour);
  if (!absl::ConsumePrefix(&t, ":")) return false;
  std::int64_t minute;
  if (!ParseInteger(t, minute, false)) return false;
  d += minutes(minute);
  if (!absl::ConsumePrefix(&t, ":")) return false;
  double second;
  if (ParseDouble(t, second, false)) {
    d += duration_cast<nanoseconds>(duration<double>(second));
  } else {
    std::int64_t second;
    if (!ParseInteger(t, second, false)) return false;
    d += seconds(second);
  }
  intvl = Interval(sign == "-" ? -d : d);
  s.remove_prefix(s.size() - t.size());
  return true;
}

Interval Unit(absl::string_view& s, std::int32_t n) {
  auto t = s;
  auto unit = ConsumeWord(t);
  if (unit.size() > 1) absl::ConsumeSuffix(&unit, "s");  // de-pluralize
  for (auto const& factory : kUnitFactories) {
    if (absl::StartsWith(factory.name, unit)) {
      s.remove_prefix(s.size() - t.size());
      return factory.factory(n);
    }
  }
  return Interval(0, 0, n);  // default to days without removing unit
}

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
StatusOr<Interval> ParseInterval(std::string const& str) {
  Interval intvl;
  absl::string_view s = str;

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
        absl::StrCat("\"", str, "\": Syntax error at \"", s.substr(0, 5), "\""),
        GCP_ERROR_INFO());
  }
  return intvl;
}

StatusOr<absl::TimeZone> LoadTimeZone(absl::string_view name) {
  absl::TimeZone tz;
  if (!absl::LoadTimeZone(name, &tz)) {
    return internal::InvalidArgumentError(
        absl::StrCat(name, ": Invalid time zone"), GCP_ERROR_INFO());
  }
  return tz;
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
  // What should we do when any field is at its most
  // negative and therefore cannot be negated?
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
  // '15 days', but '1 month 15 days' * 3 is '3 months 45 days'.
  double i_months;
  double f_months = std::modf(months_ * d, &i_months);
  months_ = static_cast<std::int32_t>(i_months);
  double i_days;
  double f_days = std::modf(days_ * d + f_months * 30, &i_days);
  days_ = static_cast<std::int32_t>(i_days);
  auto offset = duration<double, std::nano>(offset_) * d;
  offset += duration<double, std::ratio<3600>>(24) * f_days;
  offset_ = duration_cast<nanoseconds>(offset);
  return *this;
}

Interval::operator std::string() const {
  return SerializeInterval(months_, days_, offset_);
}

StatusOr<Interval> MakeInterval(absl::string_view s) {
  return ParseInterval(absl::AsciiStrToLower(s));
}

Interval JustifyDays(Interval intvl) {
  intvl.months_ += intvl.days_ / 30;
  intvl.days_ %= 30;
  if (intvl.days_ < 0) {
    intvl.days_ += 30;
    intvl.months_ -= 1;
  }
  return intvl;
}

Interval JustifyHours(Interval intvl) {
  auto days = intvl.offset_ / hours(24);
  intvl.days_ += static_cast<std::int32_t>(days);
  intvl.offset_ -= hours(days * 24);
  if (intvl.offset_ < nanoseconds::zero()) {
    intvl.offset_ += hours(24);
    intvl.days_ -= 1;
  }
  return intvl;
}

Interval JustifyInterval(Interval intvl) {
  return JustifyDays(JustifyHours(intvl));
}

StatusOr<Timestamp> Add(Timestamp const& ts, Interval const& intvl,
                        absl::string_view time_zone) {
  auto tz = LoadTimeZone(time_zone);
  if (!tz) return tz.status();
  auto ci = tz->At(*ts.get<absl::Time>());
  auto cs = absl::CivilSecond(  // add the civil-time parts
      ci.cs.year(), ci.cs.month() + intvl.months_, ci.cs.day() + intvl.days_,
      ci.cs.hour(), ci.cs.minute(), ci.cs.second());
  return *MakeTimestamp(internal::ToProtoTimestamp(  // overflow saturates
      absl::FromCivil(cs, *tz) + ci.subsecond +
      absl::FromChrono(intvl.offset_)));
}

StatusOr<Interval> Diff(Timestamp const& ts1, Timestamp const& ts2,
                        absl::string_view time_zone) {
  auto tz = LoadTimeZone(time_zone);
  if (!tz) return tz.status();
  auto ci1 = tz->At(*ts1.get<absl::Time>());
  auto ci2 = tz->At(*ts2.get<absl::Time>());
  auto days = absl::CivilDay(ci1.cs) - absl::CivilDay(ci2.cs);
  auto offset = absl::Hours(ci1.cs.hour() - ci2.cs.hour()) +
                absl::Minutes(ci1.cs.minute() - ci2.cs.minute()) +
                absl::Seconds(ci1.cs.second() - ci2.cs.second()) +
                (ci1.subsecond - ci2.subsecond);
  return JustifyHours(Interval(0, 0, static_cast<std::int32_t>(days),
                               absl::ToChronoNanoseconds(offset)));
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace spanner
}  // namespace cloud
}  // namespace google
