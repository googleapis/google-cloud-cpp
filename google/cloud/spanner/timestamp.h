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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_TIMESTAMP_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_TIMESTAMP_H

#include "google/cloud/spanner/version.h"
#include "google/cloud/status_or.h"
#include "absl/time/time.h"
#include <google/protobuf/timestamp.pb.h>
#include <chrono>
#include <cstdint>
#include <limits>
#include <ostream>
#include <string>

namespace google {
namespace cloud {
namespace spanner {
inline namespace SPANNER_CLIENT_NS {

/**
 * Convenience alias. `std::chrono::sys_time` since C++20.
 */
template <typename Duration>
using sys_time = std::chrono::time_point<std::chrono::system_clock, Duration>;

/**
 * A representation of the Spanner TIMESTAMP type: An instant in time.
 *
 * A `Timestamp` represents an absolute point in time (i.e., is independent of
 * any time zone), with at least nanosecond precision, and with a range of
 * 0001-01-01T00:00:00Z to 9999-12-31T23:59:59.999999999Z, inclusive.
 *
 * The `MakeTimestamp(src)` factory function(s) should be used to construct
 * `Timestamp` values from standard representations of absolute time.
 *
 * A `Timestamp` can be converted back to a standard representation using
 * `ts.get<T>()`.
 *
 * @see https://cloud.google.com/spanner/docs/data-types#timestamp_type
 */
class Timestamp {
 public:
  /// Default construction yields 1970-01-01T00:00:00Z.
  Timestamp() : Timestamp(absl::UnixEpoch()) {}

  /// @name Regular value type, supporting copy, assign, move.
  ///@{
  Timestamp(Timestamp&&) = default;
  Timestamp& operator=(Timestamp&&) = default;
  Timestamp(Timestamp const&) = default;
  Timestamp& operator=(Timestamp const&) = default;
  ///@}

  /// @name Relational operators
  ///@{
  friend bool operator==(Timestamp const& a, Timestamp const& b) {
    return a.t_ == b.t_;
  }
  friend bool operator!=(Timestamp const& a, Timestamp const& b) {
    return !(a == b);
  }
  friend bool operator<(Timestamp const& a, Timestamp const& b) {
    return a.t_ < b.t_;
  }
  friend bool operator<=(Timestamp const& a, Timestamp const& b) {
    return !(b < a);
  }
  friend bool operator>=(Timestamp const& a, Timestamp const& b) {
    return !(a < b);
  }
  friend bool operator>(Timestamp const& a, Timestamp const& b) {
    return b < a;
  }
  ///@}

  /// @name Output streaming
  friend std::ostream& operator<<(std::ostream& os, Timestamp ts);

  /**
   * Convert the `Timestamp` to the user-specified template type. Fails if
   * `*this` cannot be represented as a `T`.
   *
   * Supported destination types are:
   *   - `absl::Time` - Since `absl::Time` can represent all possible
   *     `Timestamp` values, `get<absl::Time>()` never returns an error.
   *   - `google::protobuf::Timestamp` - Never returns an error, but any
   *     sub-nanosecond precision will be lost.
   *   - `google::cloud::spanner::sys_time<Duration>` - `Duration::rep` may
   *     not be wider than `std::int64_t`, and `Duration::period` may be no
   *     more precise than `std::nano`.
   *
   * @par Example
   *
   * @code
   *  sys_time<std::chrono::nanoseconds> tp = ...;
   *  Timestamp ts = MakeTimestamp(tp).value();
   *  assert(tp == ts.get<sys_time<std::chrono::nanoseconds>>().value());
   * @endcode
   */
  template <typename T>
  StatusOr<T> get() const {
    return ConvertTo(T{});
  }

 private:
  friend StatusOr<Timestamp> MakeTimestamp(absl::Time);

  StatusOr<std::int64_t> ToRatio(std::int64_t min, std::int64_t max,
                                 std::int64_t num, std::int64_t den) const;

  // Conversion to a `std::chrono::time_point` on the system clock. May
  // produce out-of-range errors, depending on the properties of `Duration`
  // and the `std::chrono::system_clock` epoch.
  template <typename Duration>
  StatusOr<sys_time<Duration>> ConvertTo(sys_time<Duration> const&) const {
    using Rep = typename Duration::rep;
    using Period = typename Duration::period;
    static_assert(std::ratio_greater_equal<Period, std::nano>::value,
                  "Duration must be no more precise than std::nano");
    auto count =
        ToRatio(std::numeric_limits<Rep>::min(),
                std::numeric_limits<Rep>::max(), Period::num, Period::den);
    if (!count) return std::move(count).status();
    auto const unix_epoch = std::chrono::time_point_cast<Duration>(
        sys_time<Duration>::clock::from_time_t(0));
    return unix_epoch + Duration(static_cast<Rep>(*count));
  }

  // Conversion to an `absl::Time`. Can never fail.
  StatusOr<absl::Time> ConvertTo(absl::Time) const { return t_; }

  // Conversion to a `google::protobuf::Timestamp`. Can never fail, but
  // any sub-nanosecond precision will be lost.
  StatusOr<protobuf::Timestamp> ConvertTo(protobuf::Timestamp const&) const;

  explicit Timestamp(absl::Time t) : t_(t) {}

  absl::Time t_;
};

/**
 * Construct a `Timestamp` from an `absl::Time`. May produce out-of-range
 * errors if the given time is beyond the range supported by `Timestamp` (see
 * class comments above).
 */
StatusOr<Timestamp> MakeTimestamp(absl::Time);

/**
 * Construct a `Timestamp` from a `google::protobuf::Timestamp`. May produce
 * out-of-range errors if the given protobuf is beyond the range supported by
 * `Timestamp` (which a valid protobuf never will).
 */
StatusOr<Timestamp> MakeTimestamp(protobuf::Timestamp const&);

/**
 * Construct a `Timestamp` from a `std::chrono::time_point` on the system
 * clock. May produce out-of-range errors, depending on the properties of
 * `Duration` and the `std::chrono::system_clock` epoch. `Duration::rep` may
 * not be wider than `std::int64_t`. Requires that `Duration::period` is no
 * more precise than `std::nano`.
 */
template <typename Duration>
StatusOr<Timestamp> MakeTimestamp(sys_time<Duration> const& tp) {
  using Period = typename Duration::period;
  static_assert(std::ratio_greater_equal<Period, std::nano>::value,
                "Duration must be no more precise than std::nano");
  auto const unix_epoch = std::chrono::time_point_cast<Duration>(
      sys_time<Duration>::clock::from_time_t(0));
  auto const period = absl::Seconds(Period::num) / Period::den;
  auto const count = (tp - unix_epoch).count();
  return MakeTimestamp(absl::UnixEpoch() + count * period);
}

/**
 * A sentinel type used to update a commit timestamp column.
 *
 * @see https://cloud.google.com/spanner/docs/commit-timestamp
 */
struct CommitTimestamp {
  friend bool operator==(CommitTimestamp, CommitTimestamp) { return true; }
  friend bool operator!=(CommitTimestamp, CommitTimestamp) { return false; }
};

namespace internal {

StatusOr<Timestamp> TimestampFromRFC3339(std::string const&);
std::string TimestampToRFC3339(Timestamp);

}  // namespace internal

}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_TIMESTAMP_H
