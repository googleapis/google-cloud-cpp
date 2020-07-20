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
#include "absl/time/time.h"
#include <gmock/gmock.h>
#include <chrono>

namespace google {
namespace cloud {
inline namespace GOOGLE_CLOUD_CPP_NS {
namespace internal {
namespace {

using ::google::cloud::testing_util::IsProtoEqual;

google::protobuf::Timestamp MakeProto(std::int64_t sec, std::int32_t nsec) {
  google::protobuf::Timestamp ts;
  ts.set_seconds(sec);
  ts.set_nanos(nsec);
  return ts;
}

TEST(TimeUtils, ToProtoTimestamp) {
  // Define a few constants and lambdas to make the test cases below read
  // nicely with minimal wrapping.
  auto const epoch = std::chrono::system_clock::from_time_t(0);
  auto const& sec = [](std::int64_t n) { return std::chrono::seconds(n); };
  auto const& ns = [](std::int64_t n) { return std::chrono::nanoseconds(n); };

  struct {
    std::chrono::system_clock::time_point tp;
    google::protobuf::Timestamp expected;
  } test_case[] = {
      {epoch - sec(1), MakeProto(-1, 0)},
      {epoch - sec(1) + ns(1), MakeProto(-1, 1)},
      {epoch - ns(1), MakeProto(-1, 999999999)},
      {epoch, MakeProto(0, 0)},
      {epoch + ns(1), MakeProto(0, 1)},
      {epoch + sec(1), MakeProto(1, 0)},
      {epoch + sec(1) + ns(1), MakeProto(1, 1)},
  };

  for (auto const& tc : test_case) {
    SCOPED_TRACE("Time point: " + absl::FormatTime(absl::FromChrono(tc.tp)));
    auto const p = ToProtoTimestamp(tc.tp);
    EXPECT_THAT(p, IsProtoEqual(tc.expected));
  }
}

TEST(TimeUtils, ToChronoTimePoint) {
  google::protobuf::Timestamp proto_timestamp;
  proto_timestamp.set_seconds(867);
  proto_timestamp.set_nanos(530900);
  std::chrono::system_clock::time_point timepoint =
      ToChronoTimePoint(proto_timestamp);

  std::chrono::system_clock::time_point expected =
      std::chrono::system_clock::from_time_t(0) +
      std::chrono::duration_cast<std::chrono::system_clock::duration>(
          std::chrono::seconds(867) + std::chrono::nanoseconds(530900));
  EXPECT_EQ(timepoint, expected);
}

TEST(TimeUtils, ToAbslTime) {
  google::protobuf::Timestamp proto_timestamp;
  proto_timestamp.set_seconds(867);
  proto_timestamp.set_nanos(530900);
  auto const actual = ToAbslTime(proto_timestamp);
  auto const expected = absl::FromUnixSeconds(867) + absl::Nanoseconds(530900);
  EXPECT_EQ(expected, actual);
}

TEST(TimeUtils, ConvertAbslTimeToProtoTimestamp) {
  google::protobuf::Timestamp expected;
  expected.set_seconds(867);
  expected.set_nanos(530900);
  auto const t = absl::FromUnixSeconds(867) + absl::Nanoseconds(530900);
  auto const actual = ToProtoTimestamp(t);
  EXPECT_THAT(expected, IsProtoEqual(actual));
}

}  // namespace
}  // namespace internal
}  // namespace GOOGLE_CLOUD_CPP_NS
}  // namespace cloud
}  // namespace google
