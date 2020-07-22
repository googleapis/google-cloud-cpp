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

StatusOr<Timestamp> Timestamp::FromRFC3339(std::string const& s) {
  absl::Time t;
  std::string err;
  if (absl::ParseTime(absl::RFC3339_full, s, &t, &err)) return Timestamp(t);
  return InvalidArgument(s + ": " + err);
}

std::string Timestamp::ToRFC3339() const {
  // We want an RFC-3339 compatible format that always ends with 'Z'
  auto constexpr kFormat = "%E4Y-%m-%dT%H:%M:%E*SZ";
  return absl::FormatTime(kFormat, t_, absl::UTCTimeZone());
}

Timestamp Timestamp::FromProto(protobuf::Timestamp const& proto) {
  return Timestamp(google::cloud::internal::ToAbslTime(proto));
}

protobuf::Timestamp Timestamp::ToProto() const {
  return google::cloud::internal::ToProtoTimestamp(t_);
}

StatusOr<Timestamp> Timestamp::FromRatio(std::intmax_t const count,
                                         std::intmax_t const numerator,
                                         std::intmax_t const denominator) {
  constexpr auto kDestType = "google::cloud::spanner::Timestamp";
  auto const period = absl::Seconds(numerator) / denominator;
  auto const unix_duration = count * period;
  auto const t = absl::UnixEpoch() + unix_duration;
  if (t == absl::InfiniteFuture()) return PositiveOverflow(kDestType);
  if (t == absl::InfinitePast()) return NegativeOverflow(kDestType);
  return Timestamp(t);
}

StatusOr<std::intmax_t> Timestamp::ToRatio(std::intmax_t min, std::intmax_t max,
                                           std::intmax_t numerator,
                                           std::intmax_t denominator) const {
  constexpr auto kDestType = "std::chrono::time_point";
  auto const period = absl::Seconds(numerator) / denominator;
  auto const unix_duration = absl::Floor(t_ - absl::UnixEpoch(), period);
  if (unix_duration > max * period) return PositiveOverflow(kDestType);
  if (unix_duration < min * period) return NegativeOverflow(kDestType);
  return unix_duration / period;
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
