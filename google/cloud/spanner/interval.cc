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
#include "google/cloud/internal/absl_str_replace_quiet.h"
#include "google/cloud/internal/make_status.h"
#include "google/cloud/internal/time_utils.h"
#include "absl/strings/ascii.h"
#include "absl/strings/match.h"
#include "absl/strings/numbers.h"
#include "absl/strings/str_format.h"
#include "absl/strings/string_view.h"
#include "absl/strings/strip.h"
#include "absl/time/time.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <functional>
#include <iomanip>
#include <iterator>
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

// ISO8601 duration format.
std::string SerializeInterval(std::int32_t months, std::int32_t days,
                              nanoseconds offset) {
  std::ostringstream ss;
  std::int32_t years = months / 12;
  months %= 12;
  ss << 'P';
  if (years != 0) ss << years << 'Y';
  if (months != 0) ss << months << 'M';
  if (days != 0) ss << days << 'D';
  if (offset == nanoseconds::zero()) {
    if (years == 0 && months == 0 && days == 0) ss << "0D";
  } else {
    ss << 'T';
    auto const* sign = "";
    nanoseconds::rep nanosecond_carry = 0;
    if (offset < nanoseconds::zero()) {
      sign = "-";
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
    if (hour.count() != 0) ss << sign << hour.count() << 'H';
    if (min.count() != 0) ss << sign << min.count() << 'M';
    auto ns = offset.count() + nanosecond_carry;
    if (sec.count() != 0 || ns != 0) {
      ss << sign << sec.count();
      if (ns != 0) {
        ss << '.';
        if (ns % 1000000 == 0) {
          ss << std::setw(3) << ns / 1000000;
        } else if (ns % 1000 == 0) {
          ss << std::setw(6) << ns / 1000;
        } else {
          ss << std::setw(9) << ns;
        }
      }
      ss << 'S';
    }
  }
  return std::move(ss).str();
}

void ConsumeInteger(absl::string_view& s) {
  while (!s.empty() && absl::ascii_isdigit(s.front())) s.remove_prefix(1);
}

absl::string_view ConsumeSign(absl::string_view& s) {
  auto t = s;
  if (!absl::ConsumePrefix(&s, "-")) absl::ConsumePrefix(&s, "+");
  return t.substr(0, t.size() - s.size());
}

bool ParseDouble(absl::string_view& s, double& d, bool allow_sign,
                 bool allow_comma) {
  auto t = s;
  if (allow_sign) ConsumeSign(t);
  ConsumeInteger(t);
  bool has_comma = allow_comma && absl::ConsumePrefix(&t, ",");
  if (!has_comma && !absl::ConsumePrefix(&t, ".")) return false;
  ConsumeInteger(t);
  auto len = s.size() - t.size();
  if (has_comma) {
    auto cs = absl::StrReplaceAll(s.substr(0, len), {{",", "."}});
    if (!absl::SimpleAtod(cs, &d)) return false;
  } else {
    if (!absl::SimpleAtod(s.substr(0, len), &d)) return false;
  }
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

Status SyntaxError(absl::string_view str, absl::string_view suf,
                   internal::ErrorInfoBuilder eib) {
  return internal::InvalidArgumentError(
      absl::StrFormat(R"("%s": Syntax error at "%s" (position %d))",  //
                      str, suf.substr(0, 5), suf.data() - str.data()),
      std::move(eib));
}

/**
 * https://www.iso.org/standard/70907.html
 * https://www.iso.org/standard/70908.html
 *
 * [-+]P[n]Y[n]M[n]W[n]DT[n]H[n]M[n]S
 *
 * where "P" indicates a period, "Y", "M", "W", and "D" represent years,
 * months, weeks, and days respectively, separated by a "T" from "H",
 * "M", and "S" that represent hours, minutes, and seconds respectively.
 * "[n]" gives the value of the following unit. A leading "-" negates all
 * of the unit values.
 *
 * Units may be omitted if their value is zero, however, at least one
 * unit must be present. The smallest unit given may have a decimal
 * fractional value, with the decimal point being either a period or a
 * comma. Otherwise the values are integers.
 */

// Interval factories for ISO8601 date/time quantity units.
struct ISO8601UnitFactory {
  char name;
  std::function<Interval(std::int32_t)> factory;
};
ISO8601UnitFactory const kISO8601DateUnitFactories[] = {
    {'Y', [](auto n) { return Interval(n, 0, 0); }},
    {'M', [](auto n) { return Interval(0, n, 0); }},
    {'W', [](auto n) { return Interval(0, 0, n * 7); }},
    {'D', [](auto n) { return Interval(0, 0, n); }},
};
ISO8601UnitFactory const kISO8601TimeUnitFactories[] = {
    {'H', [](auto n) { return Interval{hours(n)}; }},
    {'M', [](auto n) { return Interval{minutes(n)}; }},
    {'S', [](auto n) { return Interval{seconds(n)}; }},
};

auto NameIs(char name) {
  return [name](ISO8601UnitFactory const& u) { return u.name == name; };
}

}  // namespace

StatusOr<Interval> Interval::ParseISO8601Interval(absl::string_view str) {
  Interval intvl;
  absl::string_view s = str;

  auto const* units = std::begin(kISO8601DateUnitFactories);
  auto const* units_end = std::end(kISO8601DateUnitFactories);
  enum { kValue, kUnit, kNothing } expecting = kValue;
  bool negated = false;

  for (;;) {
    if (units == std::begin(kISO8601DateUnitFactories)) {
      negated = !absl::ConsumePrefix(&s, "+") && absl::ConsumePrefix(&s, "-");
      if (!absl::ConsumePrefix(&s, "P")) break;
    }
    if (units_end == std::end(kISO8601DateUnitFactories)) {
      if (absl::ConsumePrefix(&s, "T")) {
        units = std::begin(kISO8601TimeUnitFactories);
        units_end = std::end(kISO8601TimeUnitFactories);
        expecting = kValue;
      }
    }
    if (units == units_end) break;
    double vf;
    if (ParseDouble(s, vf, true, true)) {
      expecting = kUnit;
      if (s.empty()) break;
      units = std::find_if(units, units_end, NameIs(s.front()));
      if (units == units_end) break;
      intvl += units++->factory(1) * vf;
      expecting = kNothing;
      s.remove_prefix(1);
      break;  // must be last unit
    }
    std::int32_t vn;
    if (!ParseInteger(s, vn, true)) break;
    expecting = kUnit;
    if (s.empty()) break;
    units = std::find_if(units, units_end, NameIs(s.front()));
    if (units == units_end) break;
    intvl += units++->factory(vn);
    expecting = kNothing;
    s.remove_prefix(1);
  }

  if (!s.empty() || expecting != kNothing) {
    return SyntaxError(str, s, GCP_ERROR_INFO());
  }
  return negated ? -intvl : intvl;
}

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
  return Interval::ParseISO8601Interval(s);
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

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace spanner
}  // namespace cloud
}  // namespace google
