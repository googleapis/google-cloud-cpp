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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_NUMERIC_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_NUMERIC_H

#include "google/cloud/spanner/version.h"
#include "google/cloud/status.h"
#include "google/cloud/status_or.h"
#include "absl/numeric/int128.h"
#include "absl/strings/string_view.h"
#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <limits>
#include <ostream>
#include <string>
#include <type_traits>
#include <utility>

namespace google {
namespace cloud {
namespace spanner {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

// The supported `Decimal` modes.
enum class DecimalMode {
  // kGoogleSQL mode supports:
  //   - 29 decimal digits of integer precision
  //   - 9 decimal digits of fractional precision
  kGoogleSQL,

  // kPostgreSQL mode supports:
  //   - 131072 decimal digits of integer precision
  //   - 16383 decimal digits of fractional precision
  //   - NaN (not a number)
  kPostgreSQL,
};

template <DecimalMode>
class Decimal;  // defined below

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace spanner

namespace spanner_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
template <spanner::DecimalMode Mode>
StatusOr<spanner::Decimal<Mode>> MakeDecimal(std::string);
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace spanner_internal

namespace spanner {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

/**
 * A representation of the Spanner NUMERIC type: an exact decimal value with
 * a maximum integer precision (kIntPrecision) and rounding to a maximum
 * fractional precision (kFracPrecision).
 *
 * A `Decimal` can be constructed from, and converted to a `std::string`, a
 * `double`, or any integral type. See the `MakeDecimal()` factory functions,
 * the `ToString()` member function, and the `ToDouble()`/`ToInteger()`
 * free functions.
 *
 * `Decimal` values can be copied/assigned/moved, compared for equality, and
 * streamed.
 *
 * @par Example
 *
 * @code
 * auto d = spanner::MakeDecimal<spanner::DecimalMode::kGoogleSQL>(42).value();
 * assert(d.ToString() == "42");
 * assert(spanner::ToInteger<int>(d).value() == 42);
 * @endcode
 */
template <DecimalMode>
class Decimal {
 public:
  /// Integer and fractional precision of a `Decimal` value of the mode.
  ///@{
  static std::size_t const kIntPrecision;
  static std::size_t const kFracPrecision;
  ///@}

  /// Whether `DecimalMode` supports NaN values.
  static bool const kHasNaN;

  /// @deprecated These backwards-compatibility constants only apply to
  /// kGoogleSQL mode, and are no longer used in the implementation.
  ///@{
  static constexpr std::size_t kIntPrec = 29;
  static constexpr std::size_t kFracPrec = 9;
  ///@}

  /// A zero value.
  Decimal() : rep_("0") {}

  /// Regular value type, supporting copy, assign, move.
  ///@{
  Decimal(Decimal&&) noexcept = default;
  Decimal& operator=(Decimal&&) noexcept = default;
  Decimal(Decimal const&) = default;
  Decimal& operator=(Decimal const&) = default;
  ///@}

  /**
   * Conversion to a decimal-string representation of the `Decimal` in one
   * of the following forms:
   *
   *  - 0                             // value == 0
   *  - -?0.[0-9]*[1-9]               // 0 < abs(value) < 1
   *  - -?[1-9][0-9]*(.[0-9]*[1-9])?  // abs(value) >= 1
   *  - NaN                           // "not a number" for kPostgreSQL mode
   *
   * Note: The string never includes an exponent field.
   */
  ///@{
  std::string const& ToString() const& { return rep_; }
  std::string&& ToString() && { return std::move(rep_); }
  ///@}

  /// Relational operators
  ///@{
  friend bool operator==(Decimal const& a, Decimal const& b) {
    // Decimal-value equality, which only depends on the canonical
    // representation, not the mode. The representation may be "NaN"
    // in kPostgreSQL mode, but unlike typical NaN implementations,
    // PostgreSQL considers NaN values as equal, so that they may be
    // sorted. We do the same.
    return a.rep_ == b.rep_;
  }
  friend bool operator!=(Decimal const& a, Decimal const& b) {
    return !(a == b);
  }
  ///@}

  /// Outputs string representation of the `Decimal` to the provided stream.
  friend std::ostream& operator<<(std::ostream& os, Decimal const& d) {
    return os << d.ToString();
  }

 private:
  template <DecimalMode Mode>
  friend StatusOr<Decimal<Mode>> spanner_internal::
#if defined(__GNUC__) && !defined(__clang__)
      GOOGLE_CLOUD_CPP_NS::
#endif
          MakeDecimal(std::string);

  explicit Decimal(std::string rep) : rep_(std::move(rep)) {}

  std::string rep_;  // a valid and canonical decimal representation
};

template <DecimalMode Mode>
constexpr std::size_t Decimal<Mode>::kIntPrec;
template <DecimalMode Mode>
constexpr std::size_t Decimal<Mode>::kFracPrec;

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace spanner

// Internal implementation details that callers should not use.
namespace spanner_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

Status DataLoss(std::string message);

// Like `std::to_string`, but also supports Abseil 128-bit integers.
template <typename T>
std::string ToString(T&& value) {
  return std::to_string(std::forward<T>(value));
}
std::string ToString(absl::int128 value);
std::string ToString(absl::uint128 value);

StatusOr<std::string> MakeDecimalRep(std::string s, bool has_nan,
                                     std::size_t int_prec,
                                     std::size_t frac_prec);
StatusOr<std::string> MakeDecimalRep(double d);

template <spanner::DecimalMode Mode>
StatusOr<spanner::Decimal<Mode>> MakeDecimal(std::string s) {
  auto rep = MakeDecimalRep(std::move(s), spanner::Decimal<Mode>::kHasNaN,
                            spanner::Decimal<Mode>::kIntPrecision,
                            spanner::Decimal<Mode>::kFracPrecision);
  if (!rep) return std::move(rep).status();
  return spanner::Decimal<Mode>(*std::move(rep));
}

// Like `MakeDecimal(s)`, but with an out-of-band exponent.
template <spanner::DecimalMode Mode>
StatusOr<spanner::Decimal<Mode>> MakeDecimal(std::string s, int exponent) {
  if (exponent != 0) s += 'e' + std::to_string(exponent);
  return MakeDecimal<Mode>(std::move(s));
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace spanner_internal

namespace spanner {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

/**
 * Construction from a string, in decimal fixed- or floating-point formats.
 *
 *  - [-+]?[0-9]+(.[0-9]*)?([eE][-+]?[0-9]+)?
 *  - [-+]?.[0-9]+([eE][-+]?[0-9]+)?
 *  - [Nn][Aa][Nn]                  // "not a number" for kPostgreSQL mode
 *
 * For example, "0", "-999", "3.141592654", "299792458", "6.02214076e23", etc.
 * There must be digits either before or after any decimal point.
 *
 * Fails on syntax errors or if the conversion would yield a value outside
 * the NUMERIC range. If the argument has more than `kFracPrecision` digits
 * after the decimal point it will be rounded, with halfway cases rounding
 * away from zero.
 */
template <DecimalMode Mode>
StatusOr<Decimal<Mode>> MakeDecimal(std::string s) {
  return spanner_internal::MakeDecimal<Mode>(std::move(s));
}

/**
 * Construction from a double.
 *
 * Fails on NaN or any argument outside the NUMERIC value range (including
 * infinities). If the argument has more than `kFracPrecision` digits after
 * the decimal point it will be rounded, with halfway cases rounding away
 * from zero.
 */
template <DecimalMode Mode>
StatusOr<Decimal<Mode>> MakeDecimal(double d) {
  auto rep = spanner_internal::MakeDecimalRep(d);
  if (!rep) return std::move(rep).status();
  return spanner_internal::MakeDecimal<Mode>(*std::move(rep));
}

/**
 * Construction from an integer `i`, scaled by 10^`exponent`.
 *
 * Fails on any (scaled) argument outside the NUMERIC value range.
 */
template <
    typename T, DecimalMode Mode,
    typename std::enable_if<std::numeric_limits<T>::is_integer, int>::type = 0>
StatusOr<Decimal<Mode>> MakeDecimal(T i, int exponent = 0) {
  return spanner_internal::MakeDecimal<Mode>(spanner_internal::ToString(i),
                                             exponent);
}

/**
 * Conversion to the closest double value, with possible loss of precision.
 *
 * Always succeeds (i.e., can never overflow, assuming a double can hold
 * values up to 10^(kIntPrecision+1)).
 */
template <DecimalMode Mode>
double ToDouble(Decimal<Mode> const& d) {
  return std::atof(d.ToString().c_str());
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
 * auto d =
 *     spanner::MakeDecimal<spanner::DecimalMode::kGoogleSQL>(123456789, -2)
 *         .value();
 * assert(d.ToString() == "1234567.89");
 * assert(spanner::ToInteger<int>(d).value() == 1234568);
 * assert(spanner::ToInteger<int>(d, 2).value() == 123456789);
 * @endcode
 */
///@{
template <typename T, DecimalMode Mode,
          typename std::enable_if<std::numeric_limits<T>::is_integer &&
                                      !std::numeric_limits<T>::is_signed,
                                  int>::type = 0>
StatusOr<T> ToInteger(  // NOLINT(misc-no-recursion)
    Decimal<Mode> const& d, int exponent = 0) {
  std::string const& rep = d.ToString();
  if (exponent != 0) {
    auto const en = spanner_internal::MakeDecimal<Mode>(rep, exponent);
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
        if (v == kMax) return spanner_internal::DataLoss(rep);
        v = static_cast<T>(v + 1);
      }
      break;
    }
    if (dp == nullptr) {
      if (ch == '-') return spanner_internal::DataLoss(rep);
      state = kFracPart;  // ch == '.'
    } else {
      if (v > kMax / 10) return spanner_internal::DataLoss(rep);
      v = static_cast<T>(v * 10);
      auto dv = static_cast<T>(dp - kDigits);
      if (v > kMax - dv) return spanner_internal::DataLoss(rep);
      v = static_cast<T>(v + dv);
    }
  }
  return v;
}
template <typename T, DecimalMode Mode,
          typename std::enable_if<std::numeric_limits<T>::is_integer &&
                                      std::numeric_limits<T>::is_signed,
                                  int>::type = 0>
StatusOr<T> ToInteger(  // NOLINT(misc-no-recursion)
    Decimal<Mode> const& d, int exponent = 0) {
  std::string const& rep = d.ToString();
  if (exponent != 0) {
    auto const en = spanner_internal::MakeDecimal<Mode>(rep, exponent);
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
        if (v == kMin) return spanner_internal::DataLoss(rep);
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
      if (v < kMin / 10) return spanner_internal::DataLoss(rep);
      v = static_cast<T>(v * 10);
      auto dv = static_cast<T>(dp - kDigits);
      if (v < kMin + dv) return spanner_internal::DataLoss(rep);
      v = static_cast<T>(v - dv);
    }
  }
  if (!negate) return v;
  constexpr auto kMax = (std::numeric_limits<T>::max)();
  if (kMin != -kMax && v == kMin) return spanner_internal::DataLoss(rep);
  return static_cast<T>(-v);
}
///@}

/**
 * Most users only need the `Numeric` or `PgNumeric` specializations of
 * `Decimal`. For example:
 *
 * @code
 * auto n = spanner::MakeNumeric(42).value();
 * assert(n.ToString() == "42");
 * assert(spanner::ToInteger<int>(n).value() == 42);
 * @endcode
 */
///@{
using Numeric = Decimal<DecimalMode::kGoogleSQL>;
using PgNumeric = Decimal<DecimalMode::kPostgreSQL>;
///@}

/**
 * `MakeNumeric()` factory functions for `Numeric`.
 */
///@{
inline StatusOr<Numeric> MakeNumeric(std::string s) {
  return MakeDecimal<DecimalMode::kGoogleSQL>(std::move(s));
}
inline StatusOr<Numeric> MakeNumeric(double d) {
  return MakeDecimal<DecimalMode::kGoogleSQL>(d);
}
template <typename T, typename std::enable_if<
                          std::numeric_limits<T>::is_integer, int>::type = 0>
StatusOr<Numeric> MakeNumeric(T i, int exponent = 0) {
  return MakeDecimal<T, DecimalMode::kGoogleSQL>(i, exponent);
}
///@}

/**
 * `MakePgNumeric()` factory functions for `PgNumeric`.
 */
///@{
inline StatusOr<PgNumeric> MakePgNumeric(std::string s) {
  return MakeDecimal<DecimalMode::kPostgreSQL>(std::move(s));
}
inline StatusOr<PgNumeric> MakePgNumeric(double d) {
  return MakeDecimal<DecimalMode::kPostgreSQL>(d);
}
template <typename T, typename std::enable_if<
                          std::numeric_limits<T>::is_integer, int>::type = 0>
StatusOr<PgNumeric> MakePgNumeric(T i, int exponent = 0) {
  return MakeDecimal<T, DecimalMode::kPostgreSQL>(i, exponent);
}
///@}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace spanner
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_NUMERIC_H
