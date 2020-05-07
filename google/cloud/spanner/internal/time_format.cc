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

// GCC did not support std::get_time() or std::put_time() until version 5,
// so we fall back to using POSIX XSI strptime() and strftime() instead.
#if !defined(__clang__) && defined(__GNUC__) && __GNUC__ < 5
#if !defined(_XOPEN_SOURCE)
#define _XOPEN_SOURCE
#endif
#endif

#include "google/cloud/spanner/internal/time_format.h"
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <limits>
#if !defined(__clang__) && defined(__GNUC__) && __GNUC__ < 5
#include <time.h>  // <ctime> doesn't have to declare strptime()
#else
#include <iomanip>
#include <sstream>
#endif

namespace google {
namespace cloud {
namespace spanner {
inline namespace SPANNER_CLIENT_NS {
namespace internal {

namespace {

constexpr auto kDigits = "0123456789";

// Range of supported years (determined by tm_year and its -1900 bias).
const std::intmax_t kYearMin = std::numeric_limits<int>::min() + 1900;
const std::intmax_t kYearMax =
    std::min<std::intmax_t>(std::numeric_limits<int>::max(),
                            std::numeric_limits<std::intmax_t>::max() - 1900) +
    1900;

// Formats [0 .. 99] as %02d.
inline char* Format02d(char* ep, int v) {
  *--ep = kDigits[v % 10];
  *--ep = kDigits[(v / 10) % 10];
  return ep;
}

// We eschew std::strtol() for reasons of over-generality (whitespace skip,
// plus-sign parsing, locale-specific formats).
template <typename T>
char const* ParseInt(char const* dp, T min, T max, T* vp) {
  T value = 0;
  bool neg = false;
  if (*dp == '-') {
    neg = true;
    ++dp;
  }
  char const* const bp = dp;
  constexpr T kMin = std::numeric_limits<T>::min();
  while (char const* cp = std::strchr(kDigits, *dp)) {
    int d = static_cast<int>(cp - kDigits);
    if (d >= 10) break;  // non-digit
    if (value < kMin / 10) return nullptr;
    value *= 10;
    if (value < kMin + d) return nullptr;
    value -= d;
    ++dp;
  }
  if (dp == bp) return nullptr;
  if (!neg) {
    if (value == kMin) return nullptr;
    value = -value;  // make positive
  }
  if (value < min || max < value) return nullptr;
  *vp = value;
  return dp;
}

inline bool LeapYear(int y) {
  return y % 4 == 0 && (y % 100 != 0 || y % 400 == 0);
}

// Note: tm.tm_year and tm.tm_mon are unadjusted (i.e., have true values).
bool ValidDay(std::tm const& tm) {
  static constexpr std::array<int, 1 + 12> kMonthDays = {
      {-1, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31}  // non leap year
  };
  if (tm.tm_mon == 2 && LeapYear(tm.tm_year)) {
    return tm.tm_mday <= 29;
  }
  return tm.tm_mday <= kMonthDays[tm.tm_mon];
}

}  // namespace

std::string FormatTime(char const* fmt, std::tm const& tm) {
#if !defined(__clang__) && defined(__GNUC__) && __GNUC__ < 5
  std::string s;
  s.resize(64);
  for (;;) {
    if (auto len = strftime(&s[0], s.size(), fmt, &tm)) {
      s.resize(len);
      break;
    }
    s.resize(s.size() * 2);
  }
  return s;
#else
  std::ostringstream output;
  output << std::put_time(&tm, fmt);
  return output.str();
#endif
}

std::string FormatTime(std::tm const& tm) {
  std::array<char, sizeof "-9223372036854775808-01-01T23:59:59"> buf;
  char* ep = buf.data() + buf.size();
  *--ep = '\0';
  ep = Format02d(ep, tm.tm_sec);
  *--ep = ':';
  ep = Format02d(ep, tm.tm_min);
  *--ep = ':';
  ep = Format02d(ep, tm.tm_hour);
  *--ep = 'T';
  ep = Format02d(ep, tm.tm_mday);
  *--ep = '-';
  ep = Format02d(ep, tm.tm_mon + 1);
  *--ep = '-';
  // Produce a 4-char tm_year rendering when possible.
  auto year = tm.tm_year + 1900LL;
  char sign = '\0';
  if (year < 0) {
    sign = '-';
    year = -year;
  }
  ep = Format02d(ep, static_cast<int>(year % 100));
  year /= 100;
  if (sign != '\0') {
    *--ep = kDigits[year % 10];
    year /= 10;
  } else {
    ep = Format02d(ep, static_cast<int>(year % 100));
    year /= 100;
  }
  while (year != 0) {
    *--ep = kDigits[year % 10];
    year /= 10;
  }
  if (sign != '\0') {
    *--ep = sign;
  }
  return ep;
}

std::size_t ParseTime(char const* fmt, std::string const& s, std::tm* tm) {
#if !defined(__clang__) && defined(__GNUC__) && __GNUC__ < 5
  char const* const bp = s.c_str();
  if (char* const ep = strptime(bp, fmt, tm)) {
    return ep - bp;
  }
  return std::string::npos;
#else
  std::istringstream input(s);
  input >> std::get_time(tm, fmt);
  if (!input) return std::string::npos;
  auto const pos = input.tellg();
  if (pos >= 0) return pos;
  return s.size();
#endif
}

std::size_t ParseTime(std::string const& s, std::tm* tm) {
  std::tm tmp{};
  std::intmax_t year;
  char const* dp = ParseInt(s.c_str(), kYearMin, kYearMax, &year);
  if (dp != nullptr && *dp == '-') {
    dp = ParseInt(dp + 1, 1, 12, &tmp.tm_mon);
    if (dp != nullptr && *dp == '-') {
      dp = ParseInt(dp + 1, 1, 31, &tmp.tm_mday);  // refine range next
      if (dp != nullptr && ValidDay(tmp) && (*dp == 'T' || *dp == 't')) {
        dp = ParseInt(dp + 1, 0, 23, &tmp.tm_hour);
        if (dp != nullptr && *dp == ':') {
          dp = ParseInt(dp + 1, 0, 59, &tmp.tm_min);
          if (dp != nullptr && *dp == ':') {
            // The tm_sec range allows for a positive leap second.  The true
            // maximum for a particular minute depends on leap-second rules,
            // which we don't have, and can't predict.
            dp = ParseInt(dp + 1, 0, 60, &tmp.tm_sec);
            if (dp != nullptr) {
              tmp.tm_year = static_cast<int>(year - 1900);
              tmp.tm_mon -= 1;
              *tm = tmp;
              return dp - s.c_str();
            }
          }
        }
      }
    }
  }
  return std::string::npos;
}

}  // namespace internal
}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner
}  // namespace cloud
}  // namespace google
