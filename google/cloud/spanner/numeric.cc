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
#include <cmath>
#include <cstddef>
#include <sstream>
#include <string>
#include <utility>

namespace google {
namespace cloud {
namespace spanner_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

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

bool IsNaN(std::string const& rep) {
  char const* np = "nan";
  for (auto const ch : rep) {
    if (*np == '\0' || static_cast<char>(std::tolower(ch)) != *np++) {
      return false;
    }
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

Status DataLoss(std::string message) {
  return Status(StatusCode::kDataLoss, std::move(message));
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace spanner_internal

namespace spanner {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

template <>
const std::size_t Decimal<DecimalMode::kGoogleSQL>::kIntPrecision = 29;
template <>
const std::size_t Decimal<DecimalMode::kGoogleSQL>::kFracPrecision = 9;

template <>
const std::size_t Decimal<DecimalMode::kPostgreSQL>::kIntPrecision = 131072;
template <>
const std::size_t Decimal<DecimalMode::kPostgreSQL>::kFracPrecision = 16383;

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace spanner
}  // namespace cloud
}  // namespace google
