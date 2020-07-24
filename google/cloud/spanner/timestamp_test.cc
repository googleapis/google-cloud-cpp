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
#include <google/protobuf/timestamp.pb.h>
#include <gmock/gmock.h>
#include <cstdint>
#include <limits>
#include <sstream>

namespace google {
namespace cloud {
namespace spanner {
inline namespace SPANNER_CLIENT_NS {
namespace {

using ::testing::HasSubstr;

// Note: You can validate the std::time_t/string conversions using date(1).
//   $ date --utc +%Y-%m-%dT%H:%M:%SZ --date=@1561135942
//   2019-06-21T16:52:22Z
//   $ date +%s --date=2019-06-21T16:52:22Z
//   1561135942

// Assumes the system_clock epoch is hour-aligned with the Unix epoch.
auto const kUnixEpoch = std::chrono::time_point_cast<std::chrono::hours>(
    std::chrono::system_clock::from_time_t(0));

google::protobuf::Timestamp MakeProtoTimestamp(std::int64_t seconds,
                                               std::int32_t nanos) {
  google::protobuf::Timestamp proto;
  proto.set_seconds(seconds);
  proto.set_nanos(nanos);
  return proto;
}

TEST(Timestamp, RegularSemantics) {
  Timestamp const ts =
      internal::TimestampFromProto(MakeProtoTimestamp(0, 0)).value();

  Timestamp const copy1(ts);
  EXPECT_EQ(copy1, ts);

  Timestamp const copy2 = ts;
  EXPECT_EQ(copy2, ts);

  Timestamp assign;
  assign = ts;
  EXPECT_EQ(assign, ts);
}

TEST(Timestamp, RelationalOperators) {
  EXPECT_EQ(
      internal::TimestampFromProto(MakeProtoTimestamp(1576030524, 611422667))
          .value(),
      internal::TimestampFromProto(MakeProtoTimestamp(1576030524, 611422667))
          .value());
  EXPECT_LE(
      internal::TimestampFromProto(MakeProtoTimestamp(1576030524, 611422667))
          .value(),
      internal::TimestampFromProto(MakeProtoTimestamp(1576030524, 611422667))
          .value());
  EXPECT_GE(
      internal::TimestampFromProto(MakeProtoTimestamp(1576030524, 611422667))
          .value(),
      internal::TimestampFromProto(MakeProtoTimestamp(1576030524, 611422667))
          .value());

  EXPECT_NE(
      internal::TimestampFromProto(MakeProtoTimestamp(1576030524, 611422667))
          .value(),
      internal::TimestampFromProto(MakeProtoTimestamp(1576030524, 611422668))
          .value());
  EXPECT_LT(
      internal::TimestampFromProto(MakeProtoTimestamp(1576030524, 611422667))
          .value(),
      internal::TimestampFromProto(MakeProtoTimestamp(1576030524, 611422668))
          .value());
  EXPECT_NE(
      internal::TimestampFromProto(MakeProtoTimestamp(1576030524, 611422667))
          .value(),
      internal::TimestampFromProto(MakeProtoTimestamp(1576030525, 611422667))
          .value());
  EXPECT_LT(
      internal::TimestampFromProto(MakeProtoTimestamp(1576030524, 611422667))
          .value(),
      internal::TimestampFromProto(MakeProtoTimestamp(1576030525, 611422667))
          .value());

  EXPECT_NE(
      internal::TimestampFromProto(MakeProtoTimestamp(1576030524, 611422668))
          .value(),
      internal::TimestampFromProto(MakeProtoTimestamp(1576030524, 611422667))
          .value());
  EXPECT_GT(
      internal::TimestampFromProto(MakeProtoTimestamp(1576030524, 611422668))
          .value(),
      internal::TimestampFromProto(MakeProtoTimestamp(1576030524, 611422667))
          .value());
  EXPECT_NE(
      internal::TimestampFromProto(MakeProtoTimestamp(1576030525, 611422667))
          .value(),
      internal::TimestampFromProto(MakeProtoTimestamp(1576030524, 611422667))
          .value());
  EXPECT_GT(
      internal::TimestampFromProto(MakeProtoTimestamp(1576030525, 611422667))
          .value(),
      internal::TimestampFromProto(MakeProtoTimestamp(1576030524, 611422667))
          .value());
}

TEST(Timestamp, OutputStreaming) {
  std::ostringstream os;
  os << internal::TimestampFromProto(MakeProtoTimestamp(1561135942, 123456789))
            .value();
  EXPECT_EQ("2019-06-21T16:52:22.123456789Z", os.str());
}

TEST(Timestamp, FromRFC3339) {
  EXPECT_EQ(
      internal::TimestampFromProto(MakeProtoTimestamp(1561135942, 0)).value(),
      internal::TimestampFromRFC3339("2019-06-21T16:52:22Z").value());
  EXPECT_EQ(
      internal::TimestampFromProto(MakeProtoTimestamp(1561135942, 9)).value(),
      internal::TimestampFromRFC3339("2019-06-21T16:52:22.000000009Z").value());
  EXPECT_EQ(
      internal::TimestampFromProto(MakeProtoTimestamp(1561135942, 89)).value(),
      internal::TimestampFromRFC3339("2019-06-21T16:52:22.000000089Z").value());
  EXPECT_EQ(
      internal::TimestampFromProto(MakeProtoTimestamp(1561135942, 789)).value(),
      internal::TimestampFromRFC3339("2019-06-21T16:52:22.000000789Z").value());
  EXPECT_EQ(
      internal::TimestampFromProto(MakeProtoTimestamp(1561135942, 6789))
          .value(),
      internal::TimestampFromRFC3339("2019-06-21T16:52:22.000006789Z").value());
  EXPECT_EQ(
      internal::TimestampFromProto(MakeProtoTimestamp(1561135942, 56789))
          .value(),
      internal::TimestampFromRFC3339("2019-06-21T16:52:22.000056789Z").value());
  EXPECT_EQ(
      internal::TimestampFromProto(MakeProtoTimestamp(1561135942, 456789))
          .value(),
      internal::TimestampFromRFC3339("2019-06-21T16:52:22.000456789Z").value());
  EXPECT_EQ(
      internal::TimestampFromProto(MakeProtoTimestamp(1561135942, 3456789))
          .value(),
      internal::TimestampFromRFC3339("2019-06-21T16:52:22.003456789Z").value());
  EXPECT_EQ(
      internal::TimestampFromProto(MakeProtoTimestamp(1561135942, 23456789))
          .value(),
      internal::TimestampFromRFC3339("2019-06-21T16:52:22.023456789Z").value());
  EXPECT_EQ(
      internal::TimestampFromProto(MakeProtoTimestamp(1561135942, 123456789))
          .value(),
      internal::TimestampFromRFC3339("2019-06-21T16:52:22.123456789Z").value());
  EXPECT_EQ(
      internal::TimestampFromProto(MakeProtoTimestamp(1561135942, 123456780))
          .value(),
      internal::TimestampFromRFC3339("2019-06-21T16:52:22.12345678Z").value());
  EXPECT_EQ(
      internal::TimestampFromProto(MakeProtoTimestamp(1561135942, 123456700))
          .value(),
      internal::TimestampFromRFC3339("2019-06-21T16:52:22.1234567Z").value());
  EXPECT_EQ(
      internal::TimestampFromProto(MakeProtoTimestamp(1561135942, 123456000))
          .value(),
      internal::TimestampFromRFC3339("2019-06-21T16:52:22.123456Z").value());
  EXPECT_EQ(
      internal::TimestampFromProto(MakeProtoTimestamp(1561135942, 123450000))
          .value(),
      internal::TimestampFromRFC3339("2019-06-21T16:52:22.12345Z").value());
  EXPECT_EQ(
      internal::TimestampFromProto(MakeProtoTimestamp(1561135942, 123400000))
          .value(),
      internal::TimestampFromRFC3339("2019-06-21T16:52:22.1234Z").value());
  EXPECT_EQ(
      internal::TimestampFromProto(MakeProtoTimestamp(1561135942, 123000000))
          .value(),
      internal::TimestampFromRFC3339("2019-06-21T16:52:22.123Z").value());
  EXPECT_EQ(
      internal::TimestampFromProto(MakeProtoTimestamp(1561135942, 120000000))
          .value(),
      internal::TimestampFromRFC3339("2019-06-21T16:52:22.12Z").value());
  EXPECT_EQ(
      internal::TimestampFromProto(MakeProtoTimestamp(1561135942, 100000000))
          .value(),
      internal::TimestampFromRFC3339("2019-06-21T16:52:22.1Z").value());
}

TEST(Timestamp, FromRFC3339Offset) {
  EXPECT_EQ(
      internal::TimestampFromProto(MakeProtoTimestamp(1546398245, 0)).value(),
      internal::TimestampFromRFC3339("2019-01-02T03:04:05+00:00").value());
  EXPECT_EQ(
      internal::TimestampFromProto(
          MakeProtoTimestamp(1546398245 + 3600 + 120, 0))
          .value(),
      internal::TimestampFromRFC3339("2019-01-02T03:04:05-01:02").value());
  EXPECT_EQ(
      internal::TimestampFromProto(
          MakeProtoTimestamp(1546398245 - 3600 - 120, 0))
          .value(),
      internal::TimestampFromRFC3339("2019-01-02T03:04:05+01:02").value());
}

TEST(Timestamp, FromRFC3339Failure) {
  EXPECT_FALSE(internal::TimestampFromRFC3339(""));
  EXPECT_FALSE(internal::TimestampFromRFC3339("garbage in"));
  EXPECT_FALSE(internal::TimestampFromRFC3339("2019-06-21T16:52:22"));
  EXPECT_FALSE(internal::TimestampFromRFC3339("2019-06-21T16:52:22.9"));
  EXPECT_FALSE(internal::TimestampFromRFC3339("2019-06-21T16:52:22.Z"));
  EXPECT_FALSE(internal::TimestampFromRFC3339("2019-06-21T16:52:22ZX"));
  EXPECT_FALSE(internal::TimestampFromRFC3339("2019-06-21T16:52:-22Z"));

  EXPECT_FALSE(internal::TimestampFromRFC3339("2019-06-21T16:52:22+0:"));
  EXPECT_FALSE(internal::TimestampFromRFC3339("2019-06-21T16:52:22+:0"));
  EXPECT_FALSE(internal::TimestampFromRFC3339("2019-06-21T16:52:22+0:-0"));
  EXPECT_FALSE(internal::TimestampFromRFC3339("2019-06-21T16:52:22x00:00"));
  EXPECT_FALSE(internal::TimestampFromRFC3339("2019-06-21T16:52:22+ab:cd"));
  EXPECT_FALSE(internal::TimestampFromRFC3339("2019-06-21T16:52:22-24:60"));
}

TEST(Timestamp, FromRFC3339Limit) {
  // Verify Spanner range requirements.
  EXPECT_EQ(
      internal::TimestampFromProto(MakeProtoTimestamp(-62135596800, 0)).value(),
      internal::TimestampFromRFC3339("0001-01-01T00:00:00.000000000Z").value());
  EXPECT_EQ(
      internal::TimestampFromProto(MakeProtoTimestamp(253402300799, 999999999))
          .value(),
      internal::TimestampFromRFC3339("9999-12-31T23:59:59.999999999Z").value());

  // One nanosecond before the lower bound is invalid.
  EXPECT_FALSE(internal::TimestampFromRFC3339("0-12-31T23:59:59.999999999Z"));

  // One nanosecond past the upper bound is invalid.
  EXPECT_FALSE(internal::TimestampFromRFC3339("10000-01-01T00:00:00Z"));
}

TEST(Timestamp, ToRFC3339) {
  EXPECT_EQ("2019-06-21T16:52:22Z",
            internal::TimestampToRFC3339(
                internal::TimestampFromProto(MakeProtoTimestamp(1561135942, 0))
                    .value()));
  EXPECT_EQ("2019-06-21T16:52:22.000000009Z",
            internal::TimestampToRFC3339(
                internal::TimestampFromProto(MakeProtoTimestamp(1561135942, 9))
                    .value()));
  EXPECT_EQ("2019-06-21T16:52:22.000000089Z",
            internal::TimestampToRFC3339(
                internal::TimestampFromProto(MakeProtoTimestamp(1561135942, 89))
                    .value()));
  EXPECT_EQ(
      "2019-06-21T16:52:22.000000789Z",
      internal::TimestampToRFC3339(
          internal::TimestampFromProto(MakeProtoTimestamp(1561135942, 789))
              .value()));
  EXPECT_EQ(
      "2019-06-21T16:52:22.000006789Z",
      internal::TimestampToRFC3339(
          internal::TimestampFromProto(MakeProtoTimestamp(1561135942, 6789))
              .value()));
  EXPECT_EQ(
      "2019-06-21T16:52:22.000056789Z",
      internal::TimestampToRFC3339(
          internal::TimestampFromProto(MakeProtoTimestamp(1561135942, 56789))
              .value()));
  EXPECT_EQ(
      "2019-06-21T16:52:22.000456789Z",
      internal::TimestampToRFC3339(
          internal::TimestampFromProto(MakeProtoTimestamp(1561135942, 456789))
              .value()));
  EXPECT_EQ(
      "2019-06-21T16:52:22.003456789Z",
      internal::TimestampToRFC3339(
          internal::TimestampFromProto(MakeProtoTimestamp(1561135942, 3456789))
              .value()));
  EXPECT_EQ(
      "2019-06-21T16:52:22.023456789Z",
      internal::TimestampToRFC3339(
          internal::TimestampFromProto(MakeProtoTimestamp(1561135942, 23456789))
              .value()));
  EXPECT_EQ("2019-06-21T16:52:22.123456789Z",
            internal::TimestampToRFC3339(
                internal::TimestampFromProto(
                    MakeProtoTimestamp(1561135942, 123456789))
                    .value()));
  EXPECT_EQ("2019-06-21T16:52:22.12345678Z",
            internal::TimestampToRFC3339(
                internal::TimestampFromProto(
                    MakeProtoTimestamp(1561135942, 123456780))
                    .value()));
  EXPECT_EQ("2019-06-21T16:52:22.1234567Z",
            internal::TimestampToRFC3339(
                internal::TimestampFromProto(
                    MakeProtoTimestamp(1561135942, 123456700))
                    .value()));
  EXPECT_EQ("2019-06-21T16:52:22.123456Z",
            internal::TimestampToRFC3339(
                internal::TimestampFromProto(
                    MakeProtoTimestamp(1561135942, 123456000))
                    .value()));
  EXPECT_EQ("2019-06-21T16:52:22.12345Z",
            internal::TimestampToRFC3339(
                internal::TimestampFromProto(
                    MakeProtoTimestamp(1561135942, 123450000))
                    .value()));
  EXPECT_EQ("2019-06-21T16:52:22.1234Z",
            internal::TimestampToRFC3339(
                internal::TimestampFromProto(
                    MakeProtoTimestamp(1561135942, 123400000))
                    .value()));
  EXPECT_EQ("2019-06-21T16:52:22.123Z",
            internal::TimestampToRFC3339(
                internal::TimestampFromProto(
                    MakeProtoTimestamp(1561135942, 123000000))
                    .value()));
  EXPECT_EQ("2019-06-21T16:52:22.12Z",
            internal::TimestampToRFC3339(
                internal::TimestampFromProto(
                    MakeProtoTimestamp(1561135942, 120000000))
                    .value()));
  EXPECT_EQ("2019-06-21T16:52:22.1Z",
            internal::TimestampToRFC3339(
                internal::TimestampFromProto(
                    MakeProtoTimestamp(1561135942, 100000000))
                    .value()));
}

TEST(Timestamp, ToRFC3339Limit) {
  // Spanner range requirements.
  EXPECT_EQ(
      "0001-01-01T00:00:00Z",
      internal::TimestampToRFC3339(
          internal::TimestampFromProto(MakeProtoTimestamp(-62135596800, 0))
              .value()));
  EXPECT_EQ("9999-12-31T23:59:59.999999999Z",
            internal::TimestampToRFC3339(
                internal::TimestampFromProto(
                    MakeProtoTimestamp(253402300799, 999999999))
                    .value()));
}

TEST(Timestamp, FromProto) {
  auto proto = MakeProtoTimestamp(0, 0);
  EXPECT_EQ("1970-01-01T00:00:00Z",
            internal::TimestampToRFC3339(
                internal::TimestampFromProto(proto).value()));

  proto = MakeProtoTimestamp(1576030524, 611422667);
  EXPECT_EQ("2019-12-11T02:15:24.611422667Z",
            internal::TimestampToRFC3339(
                internal::TimestampFromProto(proto).value()));

  proto = MakeProtoTimestamp(-62135596800, 0);
  EXPECT_EQ("0001-01-01T00:00:00Z",
            internal::TimestampToRFC3339(
                internal::TimestampFromProto(proto).value()));

  proto = MakeProtoTimestamp(253402300799, 999999999);
  EXPECT_EQ("9999-12-31T23:59:59.999999999Z",
            internal::TimestampToRFC3339(
                internal::TimestampFromProto(proto).value()));
}

TEST(Timestamp, FromProtoLimit) {
  // The min/max values that are allowed to be encoded in a Timestamp proto:
  // ["0001-01-01T00:00:00Z", "9999-12-31T23:59:59.999999999Z"]
  // Note: These values can be computed with `date +%s --date="YYYY-MM-...Z"`
  EXPECT_EQ(internal::TimestampFromRFC3339("0001-01-01T00:00:00Z").value(),
            internal::TimestampFromProto(MakeProtoTimestamp(-62135596800, 0))
                .value());
  EXPECT_EQ(
      internal::TimestampFromRFC3339("9999-12-31T23:59:59.999999999Z").value(),
      internal::TimestampFromProto(MakeProtoTimestamp(253402300799, 999999999))
          .value());
}

TEST(Timestamp, ToProto) {
  auto proto = internal::TimestampToProto(
      internal::TimestampFromRFC3339("1970-01-01T00:00:00.000000000Z").value());
  EXPECT_EQ(0, proto.seconds());
  EXPECT_EQ(0, proto.nanos());

  proto = internal::TimestampToProto(
      internal::TimestampFromRFC3339("2019-12-11T02:15:24.611422667Z").value());
  EXPECT_EQ(1576030524, proto.seconds());
  EXPECT_EQ(611422667, proto.nanos());

  proto = internal::TimestampToProto(
      internal::TimestampFromRFC3339("0001-01-01T00:00:00.000000000Z").value());
  EXPECT_EQ(-62135596800, proto.seconds());
  EXPECT_EQ(0, proto.nanos());

  proto = internal::TimestampToProto(
      internal::TimestampFromRFC3339("9999-12-31T23:59:59.999999999Z").value());
  EXPECT_EQ(253402300799, proto.seconds());
  EXPECT_EQ(999999999, proto.nanos());
}

TEST(Timestamp, FromChrono) {  // i.e., MakeTimestamp(sys_time<Duration>)
  auto const tp1 = kUnixEpoch + std::chrono::seconds(2123456789) +
                   std::chrono::nanoseconds(123456789);
  EXPECT_EQ(
      internal::TimestampFromProto(MakeProtoTimestamp(2123456789, 123456789))
          .value(),
      MakeTimestamp(tp1).value());

  auto const tp2 = kUnixEpoch + std::chrono::seconds(2123456789) +
                   std::chrono::microseconds(123456);
  EXPECT_EQ(
      internal::TimestampFromProto(MakeProtoTimestamp(2123456789, 123456000))
          .value(),
      MakeTimestamp(tp2).value());

  auto const tp3 = kUnixEpoch + std::chrono::seconds(2123456789) +
                   std::chrono::milliseconds(123);
  EXPECT_EQ(
      internal::TimestampFromProto(MakeProtoTimestamp(2123456789, 123000000))
          .value(),
      MakeTimestamp(tp3).value());

  auto const tp4 = kUnixEpoch + std::chrono::minutes(2123456789);
  EXPECT_EQ(
      internal::TimestampFromProto(MakeProtoTimestamp(2123456789LL * 60, 0))
          .value(),
      MakeTimestamp(tp4).value());

  auto const tp5 = kUnixEpoch - std::chrono::seconds(2123456789) +
                   std::chrono::nanoseconds(123456789);
  EXPECT_EQ(
      internal::TimestampFromProto(MakeProtoTimestamp(-2123456789, 123456789))
          .value(),
      MakeTimestamp(tp5).value());

  auto const tp6 = kUnixEpoch - std::chrono::seconds(2123456789) +
                   std::chrono::microseconds(123456);
  EXPECT_EQ(
      internal::TimestampFromProto(MakeProtoTimestamp(-2123456789, 123456000))
          .value(),
      MakeTimestamp(tp6).value());

  auto const tp7 = kUnixEpoch - std::chrono::seconds(2123456789) +
                   std::chrono::milliseconds(123);
  EXPECT_EQ(
      internal::TimestampFromProto(MakeProtoTimestamp(-2123456789, 123000000))
          .value(),
      MakeTimestamp(tp7).value());
}

TEST(Timestamp, FromChronoOverflow) {
  // A chrono duration that can hold values beyond the range of Timestamp.
  using big_minutes = std::chrono::duration<std::int64_t, std::ratio<60>>;

  auto const tp1 = kUnixEpoch + big_minutes(153722867280912931);
  auto const ts1 = MakeTimestamp(tp1);
  EXPECT_FALSE(ts1.ok());
  EXPECT_THAT(ts1.status().message(), HasSubstr("positive overflow"));

  auto const tp2 = kUnixEpoch - big_minutes(153722867280912931);
  auto const ts2 = MakeTimestamp(tp2);
  EXPECT_FALSE(ts2.ok());
  EXPECT_THAT(ts2.status().message(), HasSubstr("negative overflow"));
}

TEST(Timestamp, ToChrono) {  // i.e., Timestamp::get<sys_time<Duration>>()
  auto const ts_pos =
      internal::TimestampFromProto(MakeProtoTimestamp(2123456789, 123456789))
          .value();

  auto const tp1 = kUnixEpoch + std::chrono::seconds(2123456789) +
                   std::chrono::nanoseconds(123456789);
  EXPECT_EQ(tp1, ts_pos.get<sys_time<std::chrono::nanoseconds>>().value());

  auto const tp2 = kUnixEpoch + std::chrono::seconds(2123456789) +
                   std::chrono::microseconds(123456);
  EXPECT_EQ(tp2, ts_pos.get<sys_time<std::chrono::microseconds>>().value());

  auto const tp3 = kUnixEpoch + std::chrono::seconds(2123456789) +
                   std::chrono::milliseconds(123);
  EXPECT_EQ(tp3, ts_pos.get<sys_time<std::chrono::milliseconds>>().value());

  auto const tp4 = kUnixEpoch + std::chrono::seconds(2123456789);
  EXPECT_EQ(tp4, ts_pos.get<sys_time<std::chrono::seconds>>().value());

  auto const tp5 = kUnixEpoch + std::chrono::hours(2123456789 / 60 / 60);
  EXPECT_EQ(tp5, ts_pos.get<sys_time<std::chrono::hours>>().value());

  auto const ts_neg =
      internal::TimestampFromProto(MakeProtoTimestamp(-2123456789, 123456789))
          .value();

  auto const tp6 = kUnixEpoch - std::chrono::seconds(2123456789) +
                   std::chrono::nanoseconds(123456789);
  EXPECT_EQ(tp6, ts_neg.get<sys_time<std::chrono::nanoseconds>>().value());

  auto const tp7 = kUnixEpoch - std::chrono::seconds(2123456789) +
                   std::chrono::microseconds(123456);
  EXPECT_EQ(tp7, ts_neg.get<sys_time<std::chrono::microseconds>>().value());

  auto const tp8 = kUnixEpoch - std::chrono::seconds(2123456789) +
                   std::chrono::milliseconds(123);
  EXPECT_EQ(tp8, ts_neg.get<sys_time<std::chrono::milliseconds>>().value());

  auto const tp9 = kUnixEpoch - std::chrono::seconds(2123456789);
  EXPECT_EQ(tp9, ts_neg.get<sys_time<std::chrono::seconds>>().value());

  // Timestamps drop precision by flooring toward negative infinity. Therefore,
  // we need to craft our expectation by making sure our division also floors.
  auto const tp10 =
      kUnixEpoch - std::chrono::hours((2123456789 + 3600 - 1) / 3600);
  EXPECT_EQ(tp10, ts_neg.get<sys_time<std::chrono::hours>>().value());

  // The limit of a 64-bit count of nanoseconds (assuming the system_clock
  // epoch is the Unix epoch).
  auto const ts11 =
      internal::TimestampFromProto(MakeProtoTimestamp(9223372036, 854775807))
          .value();
  auto const tp11 = kUnixEpoch + std::chrono::seconds(9223372036) +
                    std::chrono::nanoseconds(854775807);
  EXPECT_EQ(tp11, ts11.get<sys_time<std::chrono::nanoseconds>>().value());
}

TEST(Timestamp, ToChronoOverflow) {
  auto const ts1 =
      internal::TimestampFromProto(MakeProtoTimestamp(20000000000, 0)).value();
  auto const tp1 = ts1.get<sys_time<std::chrono::nanoseconds>>();
  EXPECT_FALSE(tp1.ok());
  EXPECT_THAT(tp1.status().message(), HasSubstr("positive overflow"));

  auto const ts2 =
      internal::TimestampFromProto(MakeProtoTimestamp(-20000000000, 0)).value();
  auto const tp2 = ts2.get<sys_time<std::chrono::nanoseconds>>();
  EXPECT_FALSE(tp2.ok());
  EXPECT_THAT(tp2.status().message(), HasSubstr("negative overflow"));

  // One beyond the limit of a 64-bit count of nanoseconds (assuming the
  // system_clock epoch is the Unix epoch). This overflow is detected in a
  // different code path to the "positive overflow" above.
  auto const ts3 =
      internal::TimestampFromProto(MakeProtoTimestamp(9223372036, 854775808))
          .value();
  auto const tp3 = ts3.get<sys_time<std::chrono::nanoseconds>>();
  EXPECT_FALSE(tp3.ok());
  EXPECT_THAT(tp3.status().message(), HasSubstr("positive overflow"));

  // Uses a small duration (8-bits of seconds) to clearly demonstrate that we
  // detect potential overflow into the Duration's rep.
  using Sec8Bit = std::chrono::duration<std::int8_t, std::ratio<1>>;
  auto const ts4 =
      internal::TimestampFromProto(MakeProtoTimestamp(127, 0)).value();
  auto const tp4 = ts4.get<sys_time<Sec8Bit>>();
  EXPECT_TRUE(tp4);  // An in-range value succeeds.

  // One beyond the capacity for (signed) 8-bits of seconds would overflow.
  auto const ts5 =
      internal::TimestampFromProto(MakeProtoTimestamp(128, 0)).value();
  auto const tp5 = ts5.get<sys_time<Sec8Bit>>();
  EXPECT_FALSE(tp5);
  EXPECT_THAT(tp5.status().message(), HasSubstr("positive overflow"));

  // One less than the capacity for (signed) 8-bits of seconds would overflow.
  auto const ts6 =
      internal::TimestampFromProto(MakeProtoTimestamp(-129, 0)).value();
  auto const tp6 = ts6.get<sys_time<Sec8Bit>>();
  EXPECT_FALSE(tp6);
  EXPECT_THAT(tp6.status().message(), HasSubstr("negative overflow"));
}

TEST(Timestamp, AbslTimeRoundTrip) {  // i.e., MakeTimestamp(absl::Time)
  // Some constants and lambdas to make the test cases below more succinct.
  auto const s = [](std::int64_t n) { return absl::Seconds(n); };
  auto const ns = [](std::int64_t n) { return absl::Nanoseconds(n); };
  auto const epoch = absl::UnixEpoch();
  auto const contemporary =
      absl::FromUnixSeconds(2123456789) + absl::Nanoseconds(123456789);

  struct {
    absl::Time t;
    google::protobuf::Timestamp proto;
  } round_trip_cases[] = {
      {epoch - s(123) - ns(456), MakeProtoTimestamp(-124, 1000000000 - 456)},
      {epoch - s(123), MakeProtoTimestamp(-123, 0)},
      {epoch - s(1), MakeProtoTimestamp(-1, 0)},
      {epoch - ns(1), MakeProtoTimestamp(-1, 999999999)},
      {epoch, MakeProtoTimestamp(0, 0)},
      {epoch + ns(1), MakeProtoTimestamp(0, 1)},
      {epoch + s(1), MakeProtoTimestamp(1, 0)},
      {epoch + s(123), MakeProtoTimestamp(123, 0)},
      {epoch + ns(456), MakeProtoTimestamp(0, 456)},
      {epoch + s(123) + ns(456), MakeProtoTimestamp(123, 456)},
      {contemporary, MakeProtoTimestamp(2123456789, 123456789)},
  };

  for (auto const& tc : round_trip_cases) {
    SCOPED_TRACE("Time: " + absl::FormatTime(tc.t));
    auto const ts = MakeTimestamp(tc.t).value();
    EXPECT_EQ(ts, internal::TimestampFromProto(tc.proto).value());
    auto const t = ts.get<absl::Time>().value();
    EXPECT_EQ(t, tc.t);
  }
}

TEST(Timestamp, FromAbslTimeOverflow) {  // i.e., MakeTimestamp(absl::Time)
  auto const min_timestamp =
      internal::TimestampFromRFC3339("0001-01-01T00:00:00Z").value();
  auto const max_timestamp =
      internal::TimestampFromRFC3339("9999-12-31T23:59:59.999999999Z").value();

  auto const min_time = min_timestamp.get<absl::Time>().value();
  auto const max_time = max_timestamp.get<absl::Time>().value();

  // First show that the min and max absl::Time values are within range.
  EXPECT_TRUE(MakeTimestamp(min_time));
  EXPECT_TRUE(MakeTimestamp(max_time));

  // One nanosecond before the min time is out of range.
  EXPECT_FALSE(MakeTimestamp(min_time - absl::Nanoseconds(1)));

  // One nanosecond after the max time is out of range.
  EXPECT_FALSE(MakeTimestamp(max_time + absl::Nanoseconds(1)));

  // Of course, infinities are out of range.
  EXPECT_FALSE(MakeTimestamp(absl::InfinitePast()));
  EXPECT_FALSE(MakeTimestamp(absl::InfiniteFuture()));
}

}  // namespace
}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner
}  // namespace cloud
}  // namespace google
