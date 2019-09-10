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

#include "google/cloud/spanner/internal/time.h"
#include "google/cloud/spanner/internal/time_format.h"
#include <chrono>
#include <cstring>
#include <ctime>
#include <iomanip>
#include <ratio>
#include <sstream>
#include <utility>

namespace google {
namespace cloud {
namespace spanner {
inline namespace SPANNER_CLIENT_NS {
namespace internal {

//
// Duration
//

google::protobuf::Duration ToProto(std::chrono::nanoseconds ns) {
  auto s = std::chrono::duration_cast<std::chrono::seconds>(ns);
  ns -= std::chrono::duration_cast<std::chrono::nanoseconds>(s);
  google::protobuf::Duration proto;
  proto.set_seconds(s.count());
  proto.set_nanos(static_cast<int>(ns.count()));
  return proto;
}

std::chrono::nanoseconds FromProto(google::protobuf::Duration const& proto) {
  return std::chrono::seconds(proto.seconds()) +
         std::chrono::nanoseconds(proto.nanos());
}

//
// Timestamp
//

namespace {

// RFC3339 "date-time" prefix (no "time-secfrac" or "time-offset").
constexpr auto kTimeFormat = "%Y-%m-%dT%H:%M:%S";

// Split a Timestamp into a seconds-since-epoch and a (>=0) subsecond.
std::pair<std::chrono::seconds, Timestamp::duration> SplitTime(Timestamp ts) {
  auto e = std::chrono::system_clock::from_time_t(0);
  auto d = ts - std::chrono::time_point_cast<Timestamp::duration>(e);
  auto s = std::chrono::duration_cast<std::chrono::seconds>(d);
  auto ss = d - std::chrono::duration_cast<Timestamp::duration>(s);
  if (ss < Timestamp::duration(0)) {
    s -= std::chrono::seconds(1);
    ss += std::chrono::seconds(1);
  }
  return {s, ss};
}

// Combine a seconds-since-epoch and a subsecond into a Timestamp.
Timestamp CombineTime(std::chrono::seconds s, Timestamp::duration ss) {
  auto e = std::chrono::system_clock::from_time_t(0);
  return std::chrono::time_point_cast<Timestamp::duration>(e) + (s + ss);
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
std::tm ZTime(std::chrono::seconds s) {
  using rep = std::chrono::seconds::rep;
  constexpr auto kSecsPerDay = std::chrono::hours::period::num * 24;
  using days = std::chrono::duration<rep, std::ratio<kSecsPerDay>>;

  auto day = std::chrono::duration_cast<days>(s);
  auto sec = s - std::chrono::duration_cast<std::chrono::seconds>(day);
  if (sec < std::chrono::seconds(0)) {
    sec += days(1);
    day -= days(1);
  }
  auto hour = std::chrono::duration_cast<std::chrono::hours>(sec);
  sec -= std::chrono::duration_cast<std::chrono::seconds>(hour);
  auto min = std::chrono::duration_cast<std::chrono::minutes>(sec);
  sec -= std::chrono::duration_cast<std::chrono::seconds>(min);

  rep const aday = day.count() + 719468;
  rep const era = (aday >= 0 ? aday : aday - 146096) / 146097;
  rep const doe = aday - era * 146097;
  rep const yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
  rep const y = yoe + era * 400;
  rep const doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
  rep const mp = (5 * doy + 2) / 153;
  rep const d = doy - (153 * mp + 2) / 5 + 1;
  rep const m = mp + (mp < 10 ? 3 : -9);

  std::tm tm;
  tm.tm_year = static_cast<int>(y + (m <= 2 ? 1 : 0) - 1900);  // narrowing
  tm.tm_mon = static_cast<int>(m - 1);
  tm.tm_mday = static_cast<int>(d);
  tm.tm_hour = static_cast<int>(hour.count());
  tm.tm_min = static_cast<int>(min.count());
  tm.tm_sec = static_cast<int>(sec.count());
  return tm;
}

// Convert a Zulu std::tm into a seconds-since-epoch.
std::chrono::seconds TimeZ(std::tm const& tm) {
  using rep = std::chrono::seconds::rep;
  rep const y = tm.tm_year + static_cast<rep>(1900);
  rep const m = tm.tm_mon + 1;
  rep const d = tm.tm_mday;

  rep const eyear = (m <= 2) ? y - 1 : y;
  rep const era = (eyear >= 0 ? eyear : eyear - 399) / 400;
  rep const yoe = eyear - era * 400;
  rep const doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
  rep const doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  rep const aday = era * 146097 + doe - 719468;

  return std::chrono::hours(aday * 24) + std::chrono::hours(tm.tm_hour) +
         std::chrono::minutes(tm.tm_min) + std::chrono::seconds(tm.tm_sec);
}

}  // namespace

google::protobuf::Timestamp ToProto(Timestamp ts) {
  auto p = SplitTime(ts);
  auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(p.second);
  google::protobuf::Timestamp proto;
  proto.set_seconds(p.first.count());
  proto.set_nanos(static_cast<int>(ns.count()));
  return proto;
}

Timestamp FromProto(google::protobuf::Timestamp const& proto) {
  auto ns = std::chrono::nanoseconds(proto.nanos());
  return CombineTime(std::chrono::seconds(proto.seconds()),
                     std::chrono::duration_cast<Timestamp::duration>(ns));
}

// TODO(#145): Reconcile this implementation with FormatRfc3339() in
// google/cloud/internal/format_time_point.h in google-cloud-cpp.
std::string TimestampToString(Timestamp ts) {
  std::ostringstream output;
  auto p = SplitTime(ts);
  output << FormatTime(kTimeFormat, ZTime(p.first));

  if (auto ss = p.second.count()) {
    int width = 0;
    auto scale = Timestamp::duration::period::den;
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

  output << 'Z';
  return output.str();
}

// TODO(#145): Reconcile this implementation with ParseRfc3339() in
// google/cloud/internal/parse_rfc3339.h in google-cloud-cpp.
StatusOr<Timestamp> TimestampFromString(std::string const& s) {
  std::tm tm;
  auto const len = s.size();
  auto pos = ParseTime(kTimeFormat, s, &tm);
  if (pos == std::string::npos || pos == len) {
    return Status(StatusCode::kInvalidArgument,
                  s + ": Failed to match RFC3339 date-time");
  }

  Timestamp::duration ss(0);
  if (s[pos] == '.') {
    Timestamp::duration::rep v = 0;
    auto scale = Timestamp::duration::period::den;
    auto fpos = pos + 1;  // start of fractional part
    while (++pos != len) {
      static constexpr auto kDigits = "0123456789";
      char const* dp = std::strchr(kDigits, s[pos]);
      if (dp == nullptr || *dp == '\0') break;  // non-digit
      if (scale == 1) continue;                 // drop insignificant digits
      scale /= 10;
      v *= 10;
      v += dp - kDigits;
    }
    if (pos == fpos) {
      return Status(StatusCode::kInvalidArgument,
                    s + ": RFC3339 time-secfrac must include a digit");
    }
    ss = Timestamp::duration(v * scale);
  }

  if (pos == len || s[pos] != 'Z') {
    return Status(StatusCode::kInvalidArgument,
                  s + ": Missing RFC3339 time-offset 'Z'");
  }
  if (++pos != len) {
    return Status(StatusCode::kInvalidArgument,
                  s + ": Extra data after RFC3339 date-time");
  }

  return CombineTime(TimeZ(tm), ss);
}

}  // namespace internal
}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner
}  // namespace cloud
}  // namespace google
