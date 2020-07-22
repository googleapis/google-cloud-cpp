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
#include "google/cloud/internal/time_utils.h"
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

// Timestamp objects are always formatted in UTC, and we always format them
// with a trailing 'Z'. However, we're a bit more liberal in the UTC offsets we
// accept, thus the use of '%Ez' in kParseSpec.
auto constexpr kFormatSpec = "%E4Y-%m-%dT%H:%M:%E*SZ";
auto constexpr kParseSpec = "%Y-%m-%dT%H:%M:%E*S%Ez";

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

Status BadDuration(std::intmax_t num, std::intmax_t den) {
  return OutOfRange("unsupported duration ratio: " + std::to_string(num) + "/" +
                    std::to_string(den));
}

}  // namespace

StatusOr<Timestamp> Timestamp::FromRFC3339(std::string const& s) {
  absl::Time t;
  std::string err;
  if (absl::ParseTime(kParseSpec, s, &t, &err)) return MakeTimestamp(t);
  return InvalidArgument(s + ": " + err);
}

std::string Timestamp::ToRFC3339() const {
  return absl::FormatTime(kFormatSpec, t_, absl::UTCTimeZone());
}

// Note: This function requires the `proto` argument to be within the
// documented valid range for `protobuf::Timestamp`.
Timestamp Timestamp::FromProto(protobuf::Timestamp const& proto) {
  return MakeTimestamp(google::cloud::internal::ToAbslTime(proto)).value();
}

protobuf::Timestamp Timestamp::ToProto() const {
  return google::cloud::internal::ToProtoTimestamp(t_);
}

StatusOr<Timestamp> Timestamp::FromRatio(std::intmax_t const count,
                                         std::intmax_t const num,
                                         std::intmax_t const den) {
  auto const period = absl::Seconds(num) / den;
  if (period == absl::ZeroDuration()) return BadDuration(num, den);
  return MakeTimestamp(absl::UnixEpoch() + count * period);
}

StatusOr<std::intmax_t> Timestamp::ToRatio(std::intmax_t min, std::intmax_t max,
                                           std::intmax_t num,
                                           std::intmax_t den) const {
  constexpr auto kDestType = "std::chrono::time_point";
  auto const period = absl::Seconds(num) / den;
  if (period == absl::ZeroDuration()) return BadDuration(num, den);
  auto const duration = absl::Floor(t_ - absl::UnixEpoch(), period);
  if (duration > max * period) return PositiveOverflow(kDestType);
  if (duration < min * period) return NegativeOverflow(kDestType);
  return duration / period;
}

StatusOr<Timestamp> MakeTimestamp(absl::Time t) {
  constexpr auto kDestType = "google::cloud::spanner::Timestamp";
  // The min/max values that are allowed to in a Timestamp:
  // ["0001-01-01T00:00:00Z", "9999-12-31T23:59:59.999999999Z"]
  // Note: These values can be computed with `date +%s --date="YYYY-MM-...Z"`
  auto constexpr kMinTime = absl::FromUnixSeconds(-62135596800);
  auto const kMaxTime =  // NOLINT(readability-identifier-naming)
      absl::FromUnixSeconds(253402300799) + absl::Nanoseconds(999999999);
  if (t > kMaxTime) return PositiveOverflow(kDestType);
  if (t < kMinTime) return NegativeOverflow(kDestType);
  return Timestamp(t);
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
