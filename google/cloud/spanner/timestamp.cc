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
#include "google/cloud/status.h"
#include <google/protobuf/util/time_util.h>
#include <string>

namespace google {
namespace cloud {
namespace spanner {
inline namespace SPANNER_CLIENT_NS {

namespace {

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

}  // namespace

StatusOr<protobuf::Timestamp> Timestamp::ConvertTo(
    protobuf::Timestamp const&) const {
  auto constexpr kDestType = "google::protobuf::Timestamp";
  auto const s = absl::ToUnixSeconds(t_);
  if (s > protobuf::util::TimeUtil::kTimestampMaxSeconds)
    return PositiveOverflow(kDestType);
  if (s < protobuf::util::TimeUtil::kTimestampMinSeconds)
    return NegativeOverflow(kDestType);
  auto const ns = absl::ToInt64Nanoseconds(t_ - absl::FromUnixSeconds(s));
  google::protobuf::Timestamp proto;
  proto.set_seconds(s);
  proto.set_nanos(static_cast<std::int32_t>(ns));
  return proto;
}

StatusOr<std::int64_t> Timestamp::ToRatio(std::int64_t min, std::int64_t max,
                                          std::int64_t num,
                                          std::int64_t den) const {
  auto constexpr kDestType = "std::chrono::time_point";
  auto const period = absl::Seconds(num) / den;
  auto const duration = absl::Floor(t_ - absl::UnixEpoch(), period);
  if (duration > max * period) return PositiveOverflow(kDestType);
  if (duration < min * period) return NegativeOverflow(kDestType);
  return duration / period;
}

StatusOr<Timestamp> MakeTimestamp(absl::Time t) {
  auto constexpr kDestType = "google::cloud::spanner::Timestamp";
  auto constexpr kMinTime =
      absl::FromUnixSeconds(protobuf::util::TimeUtil::kTimestampMinSeconds);
  auto constexpr kMaxTime =
      absl::FromUnixSeconds(protobuf::util::TimeUtil::kTimestampMaxSeconds + 1);
  if (t >= kMaxTime) return PositiveOverflow(kDestType);
  if (t < kMinTime) return NegativeOverflow(kDestType);
  return Timestamp(t);
}

StatusOr<Timestamp> MakeTimestamp(protobuf::Timestamp const& proto) {
  return MakeTimestamp(absl::FromUnixSeconds(proto.seconds()) +
                       absl::Nanoseconds(proto.nanos()));
}

std::ostream& operator<<(std::ostream& os, Timestamp ts) {
  return os << internal::TimestampToRFC3339(ts);
}

namespace internal {

// Timestamp objects are always formatted in UTC, and we always format them
// with a trailing 'Z'. However, we're a bit more liberal in the UTC offsets we
// accept, thus the use of '%Ez' in kParseSpec.
auto constexpr kFormatSpec = "%E4Y-%m-%dT%H:%M:%E*SZ";
auto constexpr kParseSpec = "%Y-%m-%dT%H:%M:%E*S%Ez";

StatusOr<Timestamp> TimestampFromRFC3339(std::string const& s) {
  absl::Time t;
  std::string err;
  if (absl::ParseTime(kParseSpec, s, &t, &err)) return MakeTimestamp(t);
  return InvalidArgument(s + ": " + err);
}

std::string TimestampToRFC3339(Timestamp ts) {
  auto const t = ts.get<absl::Time>().value();  // Cannot fail.
  return absl::FormatTime(kFormatSpec, t, absl::UTCTimeZone());
}

}  // namespace internal

}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner
}  // namespace cloud
}  // namespace google
