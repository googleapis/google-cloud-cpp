// Copyright 2020 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_NUMERIC_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_NUMERIC_H

#include "google/cloud/spanner/version.h"
#include "google/cloud/status.h"
#include "google/cloud/status_or.h"
#include "absl/numeric/int128.h"
#include <cstdlib>
#include <cstring>
#include <limits>
#include <ostream>
#include <string>
#include <type_traits>
#include <utility>

namespace google {
namespace cloud {
namespace spanner {
inline namespace SPANNER_CLIENT_NS {

class Numeric;  // defined below

// Internal forward declarations to befriend.
namespace internal {
StatusOr<Numeric> MakeNumeric(std::string s);
}  // namespace internal

/**
 * A representation of the Spanner NUMERIC type: an exact numeric value with
 * 29 decimal digits of integer precision (kIntPrec) and 9 decimal digits of
 * fractional precision (kFracPrec).
 *
 * So, the range of a `Numeric` is -99999999999999999999999999999.999999999
 * to 99999999999999999999999999999.999999999.
 *
 * A `Numeric` can be constructed from, and converted to a `std::string`, a
 * `double`, or any integral type. See the `MakeNumeric()` factory functions,
 * the `ToString()` member function, and the `ToDouble()`/`ToInteger()`
 * free functions.
 *
 * `Numeric` values can be copied/assigned/moved, compared for equality, and
 * streamed.
 *
 * @par Example
 *
 * @code
 * spanner::Numeric n = spanner::MakeNumeric(1234).value();
 * assert(n.ToString() == "1234");
 * assert(spanner::ToInteger<int>(n).value() == 1234);
 * @endcode
 */
class Numeric {
 public:
  /// Decimal integer and fractional precision of a `Numeric` value.
  ///@{
  static constexpr std::size_t kIntPrec = 29;
  static constexpr std::size_t kFracPrec = 9;
  ///@}

  /// A zero value.
  Numeric();

  /// Regular value type, supporting copy, assign, move.
  ///@{
  Numeric(Numeric&&) = default;
  Numeric& operator=(Numeric&&) = default;
  Numeric(Numeric const&) = default;
  Numeric& operator=(Numeric const&) = default;
  ///@}

  /**
   * Conversion to a decimal-string representation of the `Numeric` in one
   * of the following forms:
   *
   *  - 0                                      // value == 0
   *  - -?0.[0-9]{0,8}[1-9]                    // 0 < abs(value) < 1
   *  - -?[1-9][0-9]{0,28}(.[0-9]{0,8}[1-9])?  // abs(value) >= 1
   *
   * Note: The string never includes an exponent field.
   */
  ///@{
  std::string const& ToString() const& { return rep_; }
  std::string&& ToString() && { return std::move(rep_); }
  ///@}

  /// Relational operators
  ///@{
  friend bool operator==(Numeric const& a, Numeric const& b) {
    return a.rep_ == b.rep_;
  }
  friend bool operator!=(Numeric const& a, Numeric const& b) {
    return !(a == b);
  }
  ///@}

  /// Outputs string representation of the `Numeric` to the provided stream.
  friend std::ostream& operator<<(std::ostream& os, Numeric const& n) {
    return os << n.ToString();
  }

 private:
  friend StatusOr<Numeric> internal::MakeNumeric(std::string s);
  explicit Numeric(std::string rep) : rep_(std::move(rep)) {}
  std::string rep_;  // a valid and canonical NUMERIC representation
};

namespace internal {

// Like `std::to_string`, but also supports Abseil 128-bit integers.
template <typename T>
std::string ToString(T&& value) {
  return std::to_string(std::forward<T>(value));
}
std::string ToString(absl::int128 value);
std::string ToString(absl::uint128 value);

// Forward declarations.
Status DataLoss(std::string message);
StatusOr<Numeric> MakeNumeric(std::string s, int exponent);

}  // namespace internal

/**
 * Construction from a string, in decimal fixed- or floating-point formats.
 *
 *  - [-+]?[0-9]+(.[0-9]*)?([eE][-+]?[0-9]+)?
 *  - [-+]?.[0-9]+([eE][-+]?[0-9]+)?
 *
 * For example, "0", "-999", "3.141592654", "299792458", "6.02214076e23", etc.
 * There must be digits either before or after any decimal point.
 *
 * Fails on syntax errors or if the conversion would yield a value outside
 * the NUMERIC range. If the argument has more than `kFracPrec` digits after
 * the decimal point it will be rounded, with halfway cases rounding away
 * from zero.
 */
StatusOr<Numeric> MakeNumeric(std::string s);

/**
 * Construction from a double.
 *
 * Fails on NaN or any argument outside the NUMERIC value range (including
 * infinities). If the argument has more than `kFracPrec` digits after the
 * decimal point it will be rounded, with halfway cases rounding away from
 * zero.
 */
StatusOr<Numeric> MakeNumeric(double d);

/**
 * Construction from an integer `i`, scaled by 10^`exponent`.
 *
 * Fails on any (scaled) argument outside the NUMERIC value range.
 */
template <typename T, typename std::enable_if<
                          std::numeric_limits<T>::is_integer, int>::type = 0>
StatusOr<Numeric> MakeNumeric(T i, int exponent = 0) {
  return internal::MakeNumeric(internal::ToString(i), exponent);
}

/**
 * Conversion to the closest double value, with possible loss of precision.
 *
 * Always succeeds (i.e., can never overflow, assuming a double can hold
 * values up to 10^(kIntPrec+1)).
 */
inline double ToDouble(Numeric const& n) {
  return std::atof(n.ToString().c_str());
}

/**
 * Conversion to the nearest integer value, scaled by 10^`exponent`.
 *
 * Rounds halfway cases away from zero. Fails when the destination type
 * cannot hold that value.
 *
 * @par Example
 *
 * @code
 * spanner::Numeric n = spanner::MakeNumeric(123456789, -2).value();
 * assert(n.ToString() == "1234567.89");
 * assert(spanner::ToInteger<int>(n).value() == 1234568);
 * assert(spanner::ToInteger<int>(n, 2).value() == 123456789);
 * @endcode
 */
///@{
template <typename T,
          typename std::enable_if<std::numeric_limits<T>::is_integer &&
                                      !std::numeric_limits<T>::is_signed,
                                  int>::type = 0>
StatusOr<T> ToInteger(  // NOLINT(misc-no-recursion)
    Numeric const& n, int exponent = 0) {
  std::string const& rep = n.ToString();
  if (exponent != 0) {
    auto const en = internal::MakeNumeric(rep, exponent);
    return en ? ToInteger<T>(*en, 0) : en.status();
  }
  T v = 0;
  constexpr auto kDigits = "0123456789";
  constexpr auto kMax = (std::numeric_limits<T>::max)();
  enum { kIntPart, kFracPart } state = kIntPart;
  for (auto const ch : rep) {
    auto const* dp = std::strchr(kDigits, ch);
    if (state == kFracPart) {
      if (dp - kDigits >= 5) {  // dp != nullptr
        if (v == kMax) return internal::DataLoss(rep);
        v = static_cast<T>(v + 1);
      }
      break;
    }
    if (dp == nullptr) {
      if (ch == '-') return internal::DataLoss(rep);
      state = kFracPart;  // ch == '.'
    } else {
      if (v > kMax / 10) return internal::DataLoss(rep);
      v = static_cast<T>(v * 10);
      auto d = static_cast<T>(dp - kDigits);
      if (v > kMax - d) return internal::DataLoss(rep);
      v = static_cast<T>(v + d);
    }
  }
  return v;
}

template <typename T,
          typename std::enable_if<std::numeric_limits<T>::is_integer &&
                                      std::numeric_limits<T>::is_signed,
                                  int>::type = 0>
StatusOr<T> ToInteger(  // NOLINT(misc-no-recursion)
    Numeric const& n, int exponent = 0) {
  std::string const& rep = n.ToString();
  if (exponent != 0) {
    auto const en = internal::MakeNumeric(rep, exponent);
    return en ? ToInteger<T>(*en, 0) : en.status();
  }
  T v = 0;
  constexpr auto kDigits = "0123456789";
  constexpr auto kMin = (std::numeric_limits<T>::min)();
  bool negate = true;
  enum { kIntPart, kFracPart } state = kIntPart;
  for (auto const ch : rep) {
    auto const* dp = std::strchr(kDigits, ch);
    if (state == kFracPart) {
      if (dp - kDigits >= 5) {  // dp != nullptr
        if (v == kMin) return internal::DataLoss(rep);
        v = static_cast<T>(v - 1);
      }
      break;
    }
    if (dp == nullptr) {
      if (ch == '-') {
        negate = false;
      } else {
        state = kFracPart;  // ch == '.'
      }
    } else {
      if (v < kMin / 10) return internal::DataLoss(rep);
      v = static_cast<T>(v * 10);
      auto d = static_cast<T>(dp - kDigits);
      if (v < kMin + d) return internal::DataLoss(rep);
      v = static_cast<T>(v - d);
    }
  }
  if (!negate) return v;
  constexpr auto kMax = (std::numeric_limits<T>::max)();
  if (kMin != -kMax && v == kMin) return internal::DataLoss(rep);
  return static_cast<T>(-v);
}
///@}

}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_NUMERIC_H
