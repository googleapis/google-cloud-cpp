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

#include "google/cloud/spanner/timestamp.h"
#include "google/cloud/spanner/internal/time_format.h"
#include "google/cloud/status.h"
#include <array>
#include <cstdlib>
#include <cstring>
#include <iomanip>
#include <limits>
#include <sstream>
#include <string>

namespace google {
namespace cloud {
namespace spanner {
inline namespace SPANNER_CLIENT_NS {

namespace {

constexpr std::int64_t kSecsPerMinute = 60;
constexpr std::int64_t kSecsPerHour = 60 * kSecsPerMinute;
constexpr std::int64_t kSecsPerDay = 24 * kSecsPerHour;
constexpr std::int32_t kNanosPerSecond = 1000 * 1000 * 1000;
constexpr auto kDigits = "0123456789";

Status InvalidArgument(std::string message) {
  return Status(StatusCode::kInvalidArgument, std::move(message));
}

Status OutOfRange(std::string message) {
  return Status(StatusCode::kOutOfRange, std::move(message));
}

Status PositiveOverflow(std::string const& type) {
  return OutOfRange(type + " positive overflow");
}

Status NegativeOverflow(std::string const& type) {
  return OutOfRange(type + " negative overflow");
}

// Convert a seconds-since-epoch into a Zulu std::tm.
//
// See http://howardhinnant.github.io/date_algorithms.html for an explanation
// of the calendrical arithmetic in ZTime() and TimeZ().  For quick reference,
// March 1st is used as the first day of the year (so that any leap day occurs
// at year's end), there are 719468 days between 0000-03-01 and 1970-01-01,
// and there are 146097 days in the 400-year Gregorian cycle (an era).
//
// All the civil-time code assumes the proleptic Gregorian calendar, with
// 24-hour days divided into 60-minute hours and 60-second minutes.
std::tm ZTime(std::int64_t s) {
  auto day = s / kSecsPerDay;
  auto sec = s % kSecsPerDay;
  if (sec < 0) {
    sec += kSecsPerDay;
    day -= 1;
  }
  auto hour = sec / kSecsPerHour;
  sec -= hour * kSecsPerHour;
  auto min = sec / kSecsPerMinute;
  sec -= min * kSecsPerMinute;

  auto const aday = day + 719468;
  auto const era = (aday >= 0 ? aday : aday - 146096) / 146097;
  auto const doe = aday - era * 146097;
  auto const yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
  auto const y = yoe + era * 400;
  auto const doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
  auto const mp = (5 * doy + 2) / 153;
  auto const d = doy - (153 * mp + 2) / 5 + 1;
  auto const m = mp + (mp < 10 ? 3 : -9);

  std::tm tm;
  // Note: Potential negative overflow of rhs and narrowing into lhs.
  tm.tm_year = static_cast<int>(y - (1900 - (m <= 2 ? 1 : 0)));
  tm.tm_mon = static_cast<int>(m - 1);
  tm.tm_mday = static_cast<int>(d);
  tm.tm_hour = static_cast<int>(hour);
  tm.tm_min = static_cast<int>(min);
  tm.tm_sec = static_cast<int>(sec);
  return tm;
}

// Convert a Zulu std::tm into a seconds-since-epoch.
std::int64_t TimeZ(std::tm const& tm) {
  // Note: Potential overflow of rhs (when int at least 64 bits).
  std::int64_t const y = static_cast<std::int64_t>(tm.tm_year) + 1900;
  std::int64_t const m = tm.tm_mon + 1;
  std::int64_t const d = tm.tm_mday;

  auto const eyear = (m <= 2) ? y - 1 : y;
  auto const era = (eyear >= 0 ? eyear : eyear - 399) / 400;
  auto const yoe = eyear - era * 400;
  auto const doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
  auto const doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  auto const aday = era * 146097 + doe - 719468;

  std::int64_t s = aday * kSecsPerDay;
  s += tm.tm_hour * kSecsPerHour;
  s += tm.tm_min * kSecsPerMinute;
  s += tm.tm_sec;
  return s;
}

}  // namespace

// TODO(#145): Reconcile this implementation with ParseRfc3339() in
// google/cloud/internal/parse_rfc3339.h in google-cloud-cpp.
StatusOr<Timestamp> Timestamp::FromRFC3339(std::string const& s) {
  std::tm tm;
  auto const len = s.size();

  // Parse full-date "T" time-hour ":" time-minute ":" time-second.
  // Note: ParseTime() fails when the requested time is outside the
  // range of a std::tm (to wit, the "int tm_year" field).
  auto pos = internal::ParseTime(s, &tm);
  if (pos == std::string::npos) {
    return InvalidArgument(s + ": Failed to match RFC3339 date-time");
  }

  // Parse time-secfrac.
  std::int64_t nanos = 0;
  if (pos != len && s[pos] == '.') {
    std::int64_t v = 0;
    auto scale = kNanosPerSecond;
    auto fpos = pos + 1;  // start of fractional digits
    while (++pos != len) {
      char const* dp = std::strchr(kDigits, s[pos]);
      if (dp == nullptr || *dp == '\0') break;  // non-digit
      if (scale == 1) continue;                 // drop insignificant digits
      scale /= 10;
      v *= 10;
      v += static_cast<std::int64_t>(dp - kDigits);
    }
    if (pos == fpos) {
      return InvalidArgument(s + ": RFC3339 time-secfrac must include a digit");
    }
    nanos = scale * v;
  }

  // Parse time-offset.
  std::int64_t utc_offset_secs = 0;
  bool offset_error = (pos == len);
  if (!offset_error) {
    switch (s[pos]) {
      case 'Z':  // Zulu time
      case 'z': {
        ++pos;
        break;
      }
      case '+':  // [-+]HH:MM
      case '-': {
        // Parse colon-separated hours, minutes, but not (yet) seconds.
        std::array<std::int64_t, 2> fields = {{0, 0}};
        auto it = fields.begin();
        auto min_it = it + 1;  // how far we must reach for success (minutes)
        int const sign = (s[pos] == '-') ? -1 : 1;
        auto ipos = pos + 1;  // start of integer digits
        while (++pos != len) {
          if (s[pos] == ':') {
            if (++it == fields.end()) break;  // too many fields
            if (pos == ipos) break;           // missing digit
            ipos = pos + 1;
          } else {
            char const* dp = std::strchr(kDigits, s[pos]);
            if (dp == nullptr || *dp == '\0') break;  // non-digit
            *it *= 10;
            *it += static_cast<std::int64_t>(dp - kDigits);
            if (*it >= 100) break;  // avoid overflow using overall bound
          }
        }
        if (pos == ipos || it < min_it || fields[0] >= 24 || fields[1] >= 60) {
          // Missing digit, not enough fields, or a field out of range.
          offset_error = true;
        } else {
          utc_offset_secs += fields[0] * kSecsPerHour;
          utc_offset_secs += fields[1] * kSecsPerMinute;
          utc_offset_secs *= sign;
        }
        break;
      }
      default: {
        offset_error = true;
        break;
      }
    }
  }
  if (offset_error) {
    return InvalidArgument(s + ": Failed to match RFC3339 time-offset");
  }

  if (pos != len) {
    return InvalidArgument(s + ": Extra data after RFC3339 date-time");
  }

  std::intmax_t const sec = TimeZ(tm);
  constexpr auto kDestType = "UTC offset";
  // Note: These overflow conditions are unreachable when tm.tm_year is only
  // 32 bits (as is typically the case) as the max/min possible `sec` value
  // plus/minus the max/min possible `utc_offset_secs` cannot oveflow 64 bits.
  if (utc_offset_secs >= 0) {
    if (sec > std::numeric_limits<std::intmax_t>::max() - utc_offset_secs) {
      return PositiveOverflow(kDestType);
    }
  } else {
    if (sec < std::numeric_limits<std::intmax_t>::min() - utc_offset_secs) {
      return NegativeOverflow(kDestType);
    }
  }
  return FromCounts(sec + utc_offset_secs, nanos);
}

// TODO(#145): Reconcile this implementation with FormatRfc3339() in
// google/cloud/internal/format_time_point.h in google-cloud-cpp.
std::string Timestamp::ToRFC3339() const {
  std::ostringstream output;

  // Spanner always uses "Z" but we leave support for a non-zero UTC offset
  // for later refactoring of this code to more general scenarios.
  constexpr std::int64_t kUtcOffsetSecs = 0;

  // Note: FormatTime(ZTime()) can only do the right thing when the requested
  // time is within the range of a std::tm (to wit, the "int tm_year" field).
  output << internal::FormatTime(ZTime(sec_ + kUtcOffsetSecs));

  if (auto ss = nsec_) {
    int width = 0;
    auto scale = kNanosPerSecond;
    while (scale != 1) {
      scale /= 10;
      width += 1;
    }
    while (ss % 10 == 0) {
      ss /= 10;
      width -= 1;
    }
    output << '.' << std::setfill('0') << std::setw(width) << ss;
  }

  if (kUtcOffsetSecs != 0) {
    // Format colon-separated hours, minutes, but not (yet) seconds.
    output << std::setfill('0') << std::internal << std::showpos
           << std::setw(1 + 2) << (kUtcOffsetSecs / kSecsPerHour) << ':'
           << std::noshowpos << std::setw(2)
           << std::abs(kUtcOffsetSecs) % kSecsPerHour / kSecsPerMinute;
  } else {
    output << 'Z';
  }
  return output.str();
}

Timestamp Timestamp::FromProto(protobuf::Timestamp const& proto) {
  auto ts = FromCounts(proto.seconds(), proto.nanos());
  if (!ts) {
    // If the proto cannot be normalized (proto.nanos() would need to be
    // outside its documented [0..999999999] range and have the same sign
    // as proto.seconds()), then we saturate.
    return proto.seconds() >= 0
               ? Timestamp{std::numeric_limits<std::int64_t>::max(),
                           kNanosPerSecond - 1}
               : Timestamp{std::numeric_limits<std::int64_t>::min(), 0};
  }
  return *ts;
}

protobuf::Timestamp Timestamp::ToProto() const {
  // May produce a protobuf::Timestamp outside the documented range of
  // 0001-01-01T00:00:00Z to 9999-12-31T23:59:59.999999999Z inclusive,
  // but so be it.
  protobuf::Timestamp proto;
  proto.set_seconds(sec_);
  proto.set_nanos(nsec_);
  return proto;
}

StatusOr<Timestamp> Timestamp::FromCounts(std::intmax_t sec,
                                          std::intmax_t nsec) {
  constexpr auto kDestType = "google::cloud::spanner::Timestamp";
  std::intmax_t carry = nsec / kNanosPerSecond;
  std::int32_t nanos = nsec % kNanosPerSecond;
  if (nanos < 0) {
    nanos += kNanosPerSecond;
    carry -= 1;
  }
  if (sec >= 0) {
    if (carry > 0 && sec > std::numeric_limits<std::int64_t>::max() - carry) {
      return PositiveOverflow(kDestType);
    }
  } else {
    if (carry < 0 && sec < std::numeric_limits<std::int64_t>::min() - carry) {
      return NegativeOverflow(kDestType);
    }
  }
  return Timestamp(static_cast<std::int64_t>(sec + carry), nanos);
}

// (count * numerator/denominator) seconds => [sec, nsec]
//
// Only designed to handle the ratios of the std::chrono::duration helper
// aliases, where either the numerator or the denominator is 1, and where
// subsecond ratios are powers of 10.
StatusOr<Timestamp> Timestamp::FromRatio(std::intmax_t const count,
                                         std::intmax_t const numerator,
                                         std::intmax_t const denominator) {
  constexpr auto kDestType = "google::cloud::spanner::Timestamp";

  auto sec = count / denominator;
  if (sec >= 0) {
    if (sec > std::numeric_limits<std::intmax_t>::max() / numerator) {
      return PositiveOverflow(kDestType);
    }
  } else {
    if (sec < std::numeric_limits<std::intmax_t>::min() / numerator) {
      return NegativeOverflow(kDestType);
    }
  }
  sec *= numerator;

  auto sscount = count - sec * denominator / numerator;
  if (denominator > kNanosPerSecond) {
    auto const divider = denominator / kNanosPerSecond;
    if (sscount < 0) sscount -= divider - 1;  // floor
    return FromCounts(sec, sscount / divider * numerator);
  }
  auto const multiplier = kNanosPerSecond / denominator;
  return FromCounts(sec, sscount * multiplier * numerator);
}

// [sec, nsec] => bounded (count * numerator/denominator) seconds.
//
// Only designed to handle the ratios of the std::chrono::duration helper
// aliases, where either the numerator or the denominator is 1, and where
// subsecond ratios are powers of 10.
StatusOr<std::intmax_t> Timestamp::ToRatio(
    std::intmax_t const min, std::intmax_t const max,
    std::intmax_t const numerator, std::intmax_t const denominator) const {
  constexpr auto kDestType = "std::chrono::time_point";

  auto count = sec_ / numerator;
  if (count >= 0) {
    if (count > std::numeric_limits<std::intmax_t>::max() / denominator) {
      return PositiveOverflow(kDestType);
    }
  } else {
    if (count < std::numeric_limits<std::intmax_t>::min() / denominator) {
      // Might be premature to declare overflow on an intermediate value.
      return NegativeOverflow(kDestType);
    }
  }
  count *= denominator;

  auto ncount = nsec_ / numerator;
  if (denominator < kNanosPerSecond) {
    ncount /= kNanosPerSecond / denominator;
  } else {
    ncount *= denominator / kNanosPerSecond;
  }

  if (count > std::numeric_limits<std::intmax_t>::max() - ncount) {
    return PositiveOverflow(kDestType);
  }
  count += ncount;

  if (count > max) return PositiveOverflow(kDestType);
  if (count < min) return NegativeOverflow(kDestType);
  return count;
}

namespace internal {

StatusOr<Timestamp> TimestampFromRFC3339(std::string const& s) {
  return Timestamp::FromRFC3339(s);
}

std::string TimestampToRFC3339(Timestamp ts) { return ts.ToRFC3339(); }

Timestamp TimestampFromProto(protobuf::Timestamp const& proto) {
  return Timestamp::FromProto(proto);
}

protobuf::Timestamp TimestampToProto(Timestamp ts) { return ts.ToProto(); }

}  // namespace internal

}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner
}  // namespace cloud
}  // namespace google
