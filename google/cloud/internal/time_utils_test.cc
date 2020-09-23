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

#include "google/cloud/internal/time_utils.h"
#include "google/cloud/testing_util/assert_ok.h"
#include "google/cloud/testing_util/is_proto_equal.h"
#include <gmock/gmock.h>
#include <chrono>

namespace google {
namespace cloud {
inline namespace GOOGLE_CLOUD_CPP_NS {
namespace internal {
namespace {

using ::google::cloud::testing_util::IsProtoEqual;

// A helper to create a Timestamp proto to make the tests in this file more
// succinct and readable.
google::protobuf::Timestamp MakeProto(std::int64_t sec, std::int32_t nsec) {
  google::protobuf::Timestamp proto;
  proto.set_seconds(sec);
  proto.set_nanos(nsec);
  return proto;
}

// A helper to create an `absl::Time` from an RFC-3339 string.
StatusOr<absl::Time> TimeFromString(std::string const& s) {
  absl::Time t;
  std::string err;
  if (absl::ParseTime(absl::RFC3339_full, s, &t, &err)) return t;
  return Status(StatusCode::kInvalidArgument, err);
}

TEST(TimeUtils, AbseilTime) {
  // Define a few constants and lambdas to make the test cases below read
  // nicely with minimal wrapping.
  auto const epoch = absl::FromUnixSeconds(0);
  auto const sec = [](std::int64_t n) { return absl::Seconds(n); };
  auto const ns = [](std::int64_t n) { return absl::Nanoseconds(n); };

  // The min/max values that are allowed to be encoded in a Timestamp proto:
  // ["0001-01-01T00:00:00Z", "9999-12-31T23:59:59.999999999Z"]
  // Note: These values can be computed with `date +%s --date="YYYY-MM-...Z"`
  auto const min_proto = MakeProto(-62135596800, 0);
  auto const max_proto = MakeProto(253402300799, 999999999);

  auto const min_time = TimeFromString("0001-01-01T00:00:00Z").value();
  auto const max_time =
      TimeFromString("9999-12-31T23:59:59.999999999Z").value();
  auto const contemporary =
      TimeFromString("2020-07-21T11:33:03.123456789Z").value();

  struct {
    absl::Time t;
    google::protobuf::Timestamp expected;
  } round_trip_cases[] = {
      {min_time, min_proto},
      {epoch - sec(1), MakeProto(-1, 0)},
      {epoch - sec(1) + ns(1), MakeProto(-1, 1)},
      {epoch - ns(1), MakeProto(-1, 999999999)},
      {epoch, MakeProto(0, 0)},
      {epoch + ns(1), MakeProto(0, 1)},
      {epoch + sec(1), MakeProto(1, 0)},
      {epoch + sec(1) + ns(1), MakeProto(1, 1)},
      {epoch + sec(123) + ns(456), MakeProto(123, 456)},
      {contemporary, MakeProto(1595331183, 123456789)},
      {max_time, max_proto},
  };

  for (auto const& tc : round_trip_cases) {
    SCOPED_TRACE("Time: " + absl::FormatTime(tc.t));
    auto const p = ToProtoTimestamp(tc.t);
    EXPECT_THAT(p, IsProtoEqual(tc.expected));
    auto const t = ToAbslTime(p);
    EXPECT_EQ(t, tc.t);
  }

  // Tests that times before the min time are capped at the min time.
  EXPECT_THAT(min_proto, IsProtoEqual(ToProtoTimestamp(absl::InfinitePast())));
  EXPECT_THAT(min_proto, IsProtoEqual(ToProtoTimestamp(min_time - ns(1))));

  // Tests that times after the max time are capped at the max time.
  EXPECT_THAT(max_proto, IsProtoEqual(ToProtoTimestamp(max_time + ns(1))));
  EXPECT_THAT(max_proto,
              IsProtoEqual(ToProtoTimestamp(absl::InfiniteFuture())));
}

TEST(TimeUtils, ChronoTime) {
  // Define a few constants and lambdas to make the test cases below read
  // nicely with minimal wrapping.
  // Note: we use microseconds rather than nanoseconds here because we test on
  // some platforms where the system clock only has microsecond resolution.
  auto const epoch = std::chrono::system_clock::from_time_t(0);
  auto const sec = [](std::int64_t n) { return std::chrono::seconds(n); };
  auto const us = [](std::int64_t n) { return std::chrono::microseconds(n); };
  auto const contemporary = absl::ToChronoTime(
      TimeFromString("2020-07-21T11:33:03.123456000Z").value());
  // Note: we do not test the proto min/max values here because we don't know
  // if std::chrono::system_clock::time_point can represent that value.

  struct {
    std::chrono::system_clock::time_point t;
    google::protobuf::Timestamp expected;
  } round_trip_cases[] = {
      {epoch - sec(1), MakeProto(-1, 0)},
      {epoch - sec(1) + us(1), MakeProto(-1, 1000)},
      {epoch - us(1), MakeProto(-1, 999999000)},
      {epoch, MakeProto(0, 0)},
      {epoch + us(1), MakeProto(0, 1000)},
      {epoch + sec(1), MakeProto(1, 0)},
      {epoch + sec(1) + us(1), MakeProto(1, 1000)},
      {epoch + sec(123) + us(456), MakeProto(123, 456000)},
      {contemporary, MakeProto(1595331183, 123456000)},
  };

  for (auto const& tc : round_trip_cases) {
    SCOPED_TRACE("Time: " + absl::FormatTime(absl::FromChrono(tc.t)));
    auto const p = ToProtoTimestamp(tc.t);
    EXPECT_THAT(p, IsProtoEqual(tc.expected));
    auto const t = ToChronoTimePoint(p);
    EXPECT_EQ(t, tc.t);
  }
}

}  // namespace
}  // namespace internal
}  // namespace GOOGLE_CLOUD_CPP_NS
}  // namespace cloud
}  // namespace google
