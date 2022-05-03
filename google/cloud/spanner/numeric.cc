// Copyright 2020 Google LLC
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

#include "google/cloud/spanner/numeric.h"
#include <algorithm>
#include <cctype>
#include <cerrno>
#include <cmath>
#include <cstddef>
#include <iomanip>
#include <locale>
#include <sstream>
#include <string>
#include <utility>

namespace google {
namespace cloud {
namespace spanner_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

namespace {

inline bool IsDigit(char ch) {
  return std::isdigit(static_cast<unsigned char>(ch)) != 0;
}

inline bool IsSpace(char ch) {
  return std::isspace(static_cast<unsigned char>(ch)) != 0;
}

inline char ToLower(char ch) { return static_cast<char>(std::tolower(ch)); }

bool IsNaN(std::string const& rep) {
  char const* np = "nan";
  for (auto const ch : rep) {
    if (*np == '\0' || ToLower(ch) != *np++) return false;
  }
  return *np == '\0';
}

// Do the pieces form a canonical, in-range value, with no rounding required?
bool IsCanonical(absl::string_view sign_part, absl::string_view int_part,
                 absl::string_view frac_part, std::size_t int_prec,
                 std::size_t frac_prec) {
  if (int_part.empty()) return false;
  if (int_part.size() > int_prec) return false;
  if (frac_part.size() > 1 + frac_prec) return false;
  if (int_part.size() == 1 && int_part.front() == '0') {
    if (frac_part.empty()) {  // Should match "0".
      if (!sign_part.empty()) return false;
    } else {  // Should match "-?0.[0-9]*[1-9]".
      if (!sign_part.empty() && sign_part.front() == '+') return false;
      if (frac_part.size() == 1 || frac_part.back() == '0') return false;
    }
  } else {  // Should match "-?[1-9][0-9]*(.[0-9]*[1-9])?".
    if (!sign_part.empty() && sign_part.front() == '+') return false;
    if (int_part.front() == '0') return false;
    if (frac_part.size() == 1) return false;
    if (!frac_part.empty() && frac_part.back() == '0') return false;
  }
  return true;
}

// Round the value to `frac_prec` digits after the decimal point, with halfway
// cases rounding away from zero.
void Round(std::deque<char>& int_rep, std::deque<char>& frac_rep,
           std::size_t frac_prec) {
  static constexpr auto kDigits = "0123456789";

  auto it = frac_rep.begin() + (std::min)(frac_prec, frac_rep.size());
  if (frac_rep.size() <= frac_prec || std::strchr(kDigits, *it) - kDigits < 5) {
    // Round towards zero.
    while (it != frac_rep.begin() && *(it - 1) == '0') --it;
    frac_rep.erase(it, frac_rep.end());
    return;
  }

  // Round away from zero (requires add and carry).
  while (it != frac_rep.begin()) {
    if (*--it != '9') {
      *it = *(std::strchr(kDigits, *it) + 1);
      frac_rep.erase(++it, frac_rep.end());
      return;
    }
  }

  // Carry into the integer part.
  frac_rep.clear();
  int_rep.push_front('0');
  it = int_rep.end();
  while (*--it == '9') *it = '0';
  *it = *(std::strchr(kDigits, *it) + 1);
}

Status InvalidArgument(std::string message) {
  return Status(StatusCode::kInvalidArgument, std::move(message));
}

Status OutOfRange(std::string message) {
  return Status(StatusCode::kOutOfRange, std::move(message));
}

}  // namespace

Status DataLoss(std::string message) {
  return Status(StatusCode::kDataLoss, std::move(message));
}

std::string ToString(absl::int128 value) {
  std::ostringstream ss;
  ss << value;
  return std::move(ss).str();
}

std::string ToString(absl::uint128 value) {
  std::ostringstream ss;
  ss << value;
  return std::move(ss).str();
}

StatusOr<std::string> MakeDecimalRep(std::string s, bool const has_nan,
                                     std::size_t const int_prec,
                                     std::size_t const frac_prec) {
  if (IsNaN(s)) {
    if (has_nan) return std::string{"NaN"};
    return InvalidArgument(std::move(s));
  }

  char const* p = s.c_str();
  char const* e = p + s.size();

  // Consume any sign part.
  auto sign_part = absl::string_view(p, 0);
  if (p != e && (*p == '+' || *p == '-')) {
    sign_part = absl::string_view(p++, 1);
  }

  // Consume any integral part.
  char const* ip = p;
  for (; p != e && IsDigit(*p); ++p) continue;
  auto int_part = absl::string_view(ip, p - ip);

  // Consume any fractional part.
  auto frac_part = absl::string_view(p, 0);
  if (p != e && *p == '.') {
    char const* fp = p++;
    for (; p != e && IsDigit(*p); ++p) continue;
    frac_part = absl::string_view(fp, p - fp);
  }

  if (p == e) {
    // This is the expected case, and avoids any allocations.
    if (IsCanonical(sign_part, int_part, frac_part, int_prec, frac_prec)) {
      return s;
    }
  }

  // Consume any exponent part.
  long exponent = 0;  // NOLINT(google-runtime-int)
  if (p != e && (*p == 'e' || *p == 'E')) {
    if (p + 1 != e && !IsSpace(*(p + 1))) {
      errno = 0;
      char* ep = nullptr;
      exponent = std::strtol(p + 1, &ep, 10);
      if (ep != p + 1) {
        if (errno != 0) return OutOfRange(std::move(s));
        p = ep;
      }
    }
  }

  // That must have consumed everything.
  if (p != e) return InvalidArgument(std::move(s));

  // There must be at least one digit.
  if (int_part.empty() && frac_part.size() <= 1) {
    return InvalidArgument(std::move(s));
  }

  auto int_rep = std::deque<char>(int_part.begin(), int_part.end());
  auto frac_rep = std::deque<char>(frac_part.begin(), frac_part.end());
  if (!frac_rep.empty()) frac_rep.pop_front();  // remove the decimal point

  // Symbolically multiply "int_rep.frac_rep" by 10^exponent.
  if (exponent >= 0) {
    auto shift = std::min<std::size_t>(exponent, frac_rep.size());
    int_rep.insert(int_rep.end(), frac_rep.begin(), frac_rep.begin() + shift);
    int_rep.insert(int_rep.end(), exponent - shift, '0');
    frac_rep.erase(frac_rep.begin(), frac_rep.begin() + shift);
  } else {
    auto shift = std::min<std::size_t>(-exponent, int_rep.size());
    frac_rep.insert(frac_rep.begin(), int_rep.end() - shift, int_rep.end());
    frac_rep.insert(frac_rep.begin(), -exponent - shift, '0');
    int_rep.erase(int_rep.end() - shift, int_rep.end());
  }

  // Round/canonicalize the fractional part.
  Round(int_rep, frac_rep, frac_prec);

  // Canonicalize and range check the integer part.
  while (!int_rep.empty() && int_rep.front() == '0') int_rep.pop_front();
  if (int_rep.size() > int_prec) {
    return OutOfRange(std::move(s));
  }

  // Add any sign and decimal point.
  bool empty = int_rep.empty() && frac_rep.empty();
  bool negate = !empty && !sign_part.empty() && sign_part.front() == '-';
  if (int_rep.empty()) int_rep.push_front('0');
  if (negate) int_rep.push_front('-');
  if (!frac_rep.empty()) frac_rep.push_front('.');

  // Construct the final value using the canonical representation.
  std::string rep;
  rep.reserve(int_rep.size() + frac_rep.size());
  rep.append(int_rep.begin(), int_rep.end());
  rep.append(frac_rep.begin(), frac_rep.end());
  return rep;
}

StatusOr<std::string> MakeDecimalRep(double d) {
  std::ostringstream ss;
  ss.imbue(std::locale::classic());
  ss << std::setprecision(std::numeric_limits<double>::digits10 + 1) << d;
  std::string rep = std::move(ss).str();
  if (std::isinf(d)) return OutOfRange(std::move(rep));
  return rep;
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace spanner_internal

namespace spanner {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

template <>
std::size_t const Decimal<DecimalMode::kGoogleSQL>::kIntPrecision = 29;
template <>
std::size_t const Decimal<DecimalMode::kGoogleSQL>::kFracPrecision = 9;
template <>
bool const Decimal<DecimalMode::kGoogleSQL>::kHasNaN = false;

template <>
std::size_t const Decimal<DecimalMode::kPostgreSQL>::kIntPrecision = 131072;
template <>
std::size_t const Decimal<DecimalMode::kPostgreSQL>::kFracPrecision = 16383;
template <>
bool const Decimal<DecimalMode::kPostgreSQL>::kHasNaN = true;

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace spanner
}  // namespace cloud
}  // namespace google
