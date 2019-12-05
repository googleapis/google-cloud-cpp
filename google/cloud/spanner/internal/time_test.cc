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
#include <gmock/gmock.h>
#include <chrono>
#include <ctime>

namespace google {
namespace cloud {
namespace spanner {
inline namespace SPANNER_CLIENT_NS {
namespace internal {
namespace {

Timestamp FromTimeT(std::time_t t) {
  return std::chrono::time_point_cast<Timestamp::duration>(
      std::chrono::system_clock::from_time_t(t));
}

TEST(Duration, ToProto) {
  google::protobuf::Duration d;

  d = ToProto(std::chrono::nanoseconds(-1234567890));
  EXPECT_EQ(-1, d.seconds());
  EXPECT_EQ(-234567890, d.nanos());

  d = ToProto(std::chrono::nanoseconds(-1000000001));
  EXPECT_EQ(-1, d.seconds());
  EXPECT_EQ(-1, d.nanos());

  d = ToProto(std::chrono::nanoseconds(-1000000000));
  EXPECT_EQ(-1, d.seconds());
  EXPECT_EQ(0, d.nanos());

  d = ToProto(std::chrono::nanoseconds(-999999999));
  EXPECT_EQ(0, d.seconds());
  EXPECT_EQ(-999999999, d.nanos());

  d = ToProto(std::chrono::nanoseconds(-1));
  EXPECT_EQ(0, d.seconds());
  EXPECT_EQ(-1, d.nanos());

  d = ToProto(std::chrono::nanoseconds(0));
  EXPECT_EQ(0, d.seconds());
  EXPECT_EQ(0, d.nanos());

  d = ToProto(std::chrono::nanoseconds(1));
  EXPECT_EQ(0, d.seconds());
  EXPECT_EQ(1, d.nanos());

  d = ToProto(std::chrono::nanoseconds(999999999));
  EXPECT_EQ(0, d.seconds());
  EXPECT_EQ(999999999, d.nanos());

  d = ToProto(std::chrono::nanoseconds(1000000000));
  EXPECT_EQ(1, d.seconds());
  EXPECT_EQ(0, d.nanos());

  d = ToProto(std::chrono::nanoseconds(1000000001));
  EXPECT_EQ(1, d.seconds());
  EXPECT_EQ(1, d.nanos());

  d = ToProto(std::chrono::nanoseconds(1234567890));
  EXPECT_EQ(1, d.seconds());
  EXPECT_EQ(234567890, d.nanos());
}

TEST(Duration, FromProto) {
  google::protobuf::Duration d;

  d.set_seconds(-1);
  d.set_nanos(-234567890);
  EXPECT_EQ(std::chrono::nanoseconds(-1234567890), FromProto(d));

  d.set_seconds(-1);
  d.set_nanos(-1);
  EXPECT_EQ(std::chrono::nanoseconds(-1000000001), FromProto(d));

  d.set_seconds(-1);
  d.set_nanos(0);
  EXPECT_EQ(std::chrono::nanoseconds(-1000000000), FromProto(d));

  d.set_seconds(0);
  d.set_nanos(-999999999);
  EXPECT_EQ(std::chrono::nanoseconds(-999999999), FromProto(d));

  d.set_seconds(0);
  d.set_nanos(-1);
  EXPECT_EQ(std::chrono::nanoseconds(-1), FromProto(d));

  d.set_seconds(0);
  d.set_nanos(0);
  EXPECT_EQ(std::chrono::nanoseconds(0), FromProto(d));

  d.set_seconds(0);
  d.set_nanos(1);
  EXPECT_EQ(std::chrono::nanoseconds(1), FromProto(d));

  d.set_seconds(0);
  d.set_nanos(999999999);
  EXPECT_EQ(std::chrono::nanoseconds(999999999), FromProto(d));

  d.set_seconds(1);
  d.set_nanos(0);
  EXPECT_EQ(std::chrono::nanoseconds(1000000000), FromProto(d));

  d.set_seconds(1);
  d.set_nanos(1);
  EXPECT_EQ(std::chrono::nanoseconds(1000000001), FromProto(d));

  d.set_seconds(1);
  d.set_nanos(234567890);
  EXPECT_EQ(std::chrono::nanoseconds(1234567890), FromProto(d));
}

TEST(Time, ToProto) {
  google::protobuf::Timestamp ts;

  ts = ToProto(FromTimeT(-1) - std::chrono::nanoseconds(999999999));
  EXPECT_EQ(-2, ts.seconds());
  EXPECT_EQ(1, ts.nanos());

  ts = ToProto(FromTimeT(-1) - std::chrono::nanoseconds(1));
  EXPECT_EQ(-2, ts.seconds());
  EXPECT_EQ(999999999, ts.nanos());

  ts = ToProto(FromTimeT(-1));
  EXPECT_EQ(-1, ts.seconds());
  EXPECT_EQ(0, ts.nanos());

  ts = ToProto(FromTimeT(0) - std::chrono::nanoseconds(999999999));
  EXPECT_EQ(-1, ts.seconds());
  EXPECT_EQ(1, ts.nanos());

  ts = ToProto(FromTimeT(0) - std::chrono::nanoseconds(1));
  EXPECT_EQ(-1, ts.seconds());
  EXPECT_EQ(999999999, ts.nanos());

  ts = ToProto(FromTimeT(0));
  EXPECT_EQ(0, ts.seconds());
  EXPECT_EQ(0, ts.nanos());

  ts = ToProto(FromTimeT(0) + std::chrono::nanoseconds(1));
  EXPECT_EQ(0, ts.seconds());
  EXPECT_EQ(1, ts.nanos());

  ts = ToProto(FromTimeT(0) + std::chrono::nanoseconds(999999999));
  EXPECT_EQ(0, ts.seconds());
  EXPECT_EQ(999999999, ts.nanos());

  ts = ToProto(FromTimeT(1));
  EXPECT_EQ(1, ts.seconds());
  EXPECT_EQ(0, ts.nanos());

  ts = ToProto(FromTimeT(1) + std::chrono::nanoseconds(1));
  EXPECT_EQ(1, ts.seconds());
  EXPECT_EQ(1, ts.nanos());

  ts = ToProto(FromTimeT(1) + std::chrono::nanoseconds(999999999));
  EXPECT_EQ(1, ts.seconds());
  EXPECT_EQ(999999999, ts.nanos());
}

TEST(Time, FromProto) {
  google::protobuf::Timestamp ts;

  ts.set_seconds(-2);
  ts.set_nanos(1);
  EXPECT_EQ(FromTimeT(-1) - std::chrono::nanoseconds(999999999), FromProto(ts));

  ts.set_seconds(-2);
  ts.set_nanos(999999999);
  EXPECT_EQ(FromTimeT(-1) - std::chrono::nanoseconds(1), FromProto(ts));

  ts.set_seconds(-1);
  ts.set_nanos(0);
  EXPECT_EQ(FromTimeT(-1), FromProto(ts));

  ts.set_seconds(-1);
  ts.set_nanos(1);
  EXPECT_EQ(FromTimeT(0) - std::chrono::nanoseconds(999999999), FromProto(ts));

  ts.set_seconds(-1);
  ts.set_nanos(999999999);
  EXPECT_EQ(FromTimeT(0) - std::chrono::nanoseconds(1), FromProto(ts));

  ts.set_seconds(0);
  ts.set_nanos(0);
  EXPECT_EQ(FromTimeT(0), FromProto(ts));

  ts.set_seconds(0);
  ts.set_nanos(1);
  EXPECT_EQ(FromTimeT(0) + std::chrono::nanoseconds(1), FromProto(ts));

  ts.set_seconds(0);
  ts.set_nanos(999999999);
  EXPECT_EQ(FromTimeT(0) + std::chrono::nanoseconds(999999999), FromProto(ts));

  ts.set_seconds(1);
  ts.set_nanos(0);
  EXPECT_EQ(FromTimeT(1), FromProto(ts));

  ts.set_seconds(1);
  ts.set_nanos(1);
  EXPECT_EQ(FromTimeT(1) + std::chrono::nanoseconds(1), FromProto(ts));

  ts.set_seconds(1);
  ts.set_nanos(999999999);
  EXPECT_EQ(FromTimeT(1) + std::chrono::nanoseconds(999999999), FromProto(ts));
}

TEST(Time, TimestampToString) {
  Timestamp ts = FromTimeT(1561135942);
  EXPECT_EQ("2019-06-21T16:52:22Z", TimestampToString(ts));
  ts += std::chrono::nanoseconds(9);
  EXPECT_EQ("2019-06-21T16:52:22.000000009Z", TimestampToString(ts));
  ts += std::chrono::nanoseconds(80);
  EXPECT_EQ("2019-06-21T16:52:22.000000089Z", TimestampToString(ts));
  ts += std::chrono::nanoseconds(700);
  EXPECT_EQ("2019-06-21T16:52:22.000000789Z", TimestampToString(ts));
  ts += std::chrono::nanoseconds(6000);
  EXPECT_EQ("2019-06-21T16:52:22.000006789Z", TimestampToString(ts));
  ts += std::chrono::nanoseconds(50000);
  EXPECT_EQ("2019-06-21T16:52:22.000056789Z", TimestampToString(ts));
  ts += std::chrono::nanoseconds(400000);
  EXPECT_EQ("2019-06-21T16:52:22.000456789Z", TimestampToString(ts));
  ts += std::chrono::nanoseconds(3000000);
  EXPECT_EQ("2019-06-21T16:52:22.003456789Z", TimestampToString(ts));
  ts += std::chrono::nanoseconds(20000000);
  EXPECT_EQ("2019-06-21T16:52:22.023456789Z", TimestampToString(ts));
  ts += std::chrono::nanoseconds(100000000);
  EXPECT_EQ("2019-06-21T16:52:22.123456789Z", TimestampToString(ts));
  ts -= std::chrono::nanoseconds(9);
  EXPECT_EQ("2019-06-21T16:52:22.12345678Z", TimestampToString(ts));
  ts -= std::chrono::nanoseconds(80);
  EXPECT_EQ("2019-06-21T16:52:22.1234567Z", TimestampToString(ts));
  ts -= std::chrono::nanoseconds(700);
  EXPECT_EQ("2019-06-21T16:52:22.123456Z", TimestampToString(ts));
  ts -= std::chrono::nanoseconds(6000);
  EXPECT_EQ("2019-06-21T16:52:22.12345Z", TimestampToString(ts));
  ts -= std::chrono::nanoseconds(50000);
  EXPECT_EQ("2019-06-21T16:52:22.1234Z", TimestampToString(ts));
  ts -= std::chrono::nanoseconds(400000);
  EXPECT_EQ("2019-06-21T16:52:22.123Z", TimestampToString(ts));
  ts -= std::chrono::nanoseconds(3000000);
  EXPECT_EQ("2019-06-21T16:52:22.12Z", TimestampToString(ts));
  ts -= std::chrono::nanoseconds(20000000);
  EXPECT_EQ("2019-06-21T16:52:22.1Z", TimestampToString(ts));
  ts -= std::chrono::nanoseconds(100000000);
  EXPECT_EQ("2019-06-21T16:52:22Z", TimestampToString(ts));
}

TEST(Time, TimestampToStringOffset) {
  Timestamp ts = FromTimeT(1546398245);
  auto utc_offset = std::chrono::minutes::zero();
  EXPECT_EQ("2019-01-02T03:04:05Z", TimestampToString(ts, utc_offset));
  utc_offset = -std::chrono::minutes(1 * 60 + 2);
  EXPECT_EQ("2019-01-02T02:02:05-01:02", TimestampToString(ts, utc_offset));
  utc_offset = std::chrono::minutes(1 * 60 + 2);
  EXPECT_EQ("2019-01-02T04:06:05+01:02", TimestampToString(ts, utc_offset));
}

TEST(Time, TimestampToStringLimit) {
  Timestamp ts = FromTimeT(-9223372036L);
  EXPECT_EQ("1677-09-21T00:12:44Z", TimestampToString(ts));

  ts = FromTimeT(9223372036L) + std::chrono::nanoseconds(854775807);
  EXPECT_EQ("2262-04-11T23:47:16.854775807Z", TimestampToString(ts));
}

TEST(Time, TimestampFromString) {
  Timestamp ts = FromTimeT(1561135942);
  EXPECT_EQ(ts, TimestampFromString("2019-06-21T16:52:22Z").value());
  ts += std::chrono::nanoseconds(9);
  EXPECT_EQ(ts, TimestampFromString("2019-06-21T16:52:22.000000009Z").value());
  ts += std::chrono::nanoseconds(80);
  EXPECT_EQ(ts, TimestampFromString("2019-06-21T16:52:22.000000089Z").value());
  ts += std::chrono::nanoseconds(700);
  EXPECT_EQ(ts, TimestampFromString("2019-06-21T16:52:22.000000789Z").value());
  ts += std::chrono::nanoseconds(6000);
  EXPECT_EQ(ts, TimestampFromString("2019-06-21T16:52:22.000006789Z").value());
  ts += std::chrono::nanoseconds(50000);
  EXPECT_EQ(ts, TimestampFromString("2019-06-21T16:52:22.000056789Z").value());
  ts += std::chrono::nanoseconds(400000);
  EXPECT_EQ(ts, TimestampFromString("2019-06-21T16:52:22.000456789Z").value());
  ts += std::chrono::nanoseconds(3000000);
  EXPECT_EQ(ts, TimestampFromString("2019-06-21T16:52:22.003456789Z").value());
  ts += std::chrono::nanoseconds(20000000);
  EXPECT_EQ(ts, TimestampFromString("2019-06-21T16:52:22.023456789Z").value());
  ts += std::chrono::nanoseconds(100000000);
  EXPECT_EQ(ts, TimestampFromString("2019-06-21T16:52:22.123456789Z").value());
  ts -= std::chrono::nanoseconds(9);
  EXPECT_EQ(ts, TimestampFromString("2019-06-21T16:52:22.12345678Z").value());
  ts -= std::chrono::nanoseconds(80);
  EXPECT_EQ(ts, TimestampFromString("2019-06-21T16:52:22.1234567Z").value());
  ts -= std::chrono::nanoseconds(700);
  EXPECT_EQ(ts, TimestampFromString("2019-06-21T16:52:22.123456Z").value());
  ts -= std::chrono::nanoseconds(6000);
  EXPECT_EQ(ts, TimestampFromString("2019-06-21T16:52:22.12345Z").value());
  ts -= std::chrono::nanoseconds(50000);
  EXPECT_EQ(ts, TimestampFromString("2019-06-21T16:52:22.1234Z").value());
  ts -= std::chrono::nanoseconds(400000);
  EXPECT_EQ(ts, TimestampFromString("2019-06-21T16:52:22.123Z").value());
  ts -= std::chrono::nanoseconds(3000000);
  EXPECT_EQ(ts, TimestampFromString("2019-06-21T16:52:22.12Z").value());
  ts -= std::chrono::nanoseconds(20000000);
  EXPECT_EQ(ts, TimestampFromString("2019-06-21T16:52:22.1Z").value());
  ts -= std::chrono::nanoseconds(100000000);
  EXPECT_EQ(ts, TimestampFromString("2019-06-21T16:52:22Z").value());
}

TEST(Time, TimestampFromStringOffset) {
  Timestamp ts = FromTimeT(1546398245);
  EXPECT_EQ(ts + std::chrono::minutes::zero(),
            TimestampFromString("2019-01-02T03:04:05+00:00").value());
  EXPECT_EQ(ts + std::chrono::hours(1) + std::chrono::minutes(2),
            TimestampFromString("2019-01-02T03:04:05+01:02").value());
  EXPECT_EQ(ts - std::chrono::hours(1) - std::chrono::minutes(2),
            TimestampFromString("2019-01-02T03:04:05-01:02").value());
}

TEST(Time, TimestampFromStringLimit) {
  Timestamp ts = FromTimeT(-9223372036L);
  EXPECT_EQ(ts, TimestampFromString("1677-09-21T00:12:44Z").value());

  ts = FromTimeT(9223372036L) + std::chrono::nanoseconds(854775807);
  EXPECT_EQ(ts, TimestampFromString("2262-04-11T23:47:16.854775807Z").value());
}

TEST(Time, TimestampFromStringFailure) {
  EXPECT_FALSE(TimestampFromString(""));
  EXPECT_FALSE(TimestampFromString("garbage in"));
  EXPECT_FALSE(TimestampFromString("2019-06-21T16:52:22"));
  EXPECT_FALSE(TimestampFromString("2019-06-21T16:52:22.9"));
  EXPECT_FALSE(TimestampFromString("2019-06-21T16:52:22.Z"));
  EXPECT_FALSE(TimestampFromString("2019-06-21T16:52:22ZX"));
  EXPECT_FALSE(TimestampFromString("2019-06-21T16:52:-22Z"));

  EXPECT_FALSE(TimestampFromString("2019-06-21T16:52:22+0:"));
  EXPECT_FALSE(TimestampFromString("2019-06-21T16:52:22+:0"));
  EXPECT_FALSE(TimestampFromString("2019-06-21T16:52:22+0:-0"));
  EXPECT_FALSE(TimestampFromString("2019-06-21T16:52:22x00:00"));
  EXPECT_FALSE(TimestampFromString("2019-06-21T16:52:22+ab:cd"));
  EXPECT_FALSE(TimestampFromString("2019-06-21T16:52:22-24:60"));
  EXPECT_FALSE(TimestampFromString("2019-06-21T16:52:22+00:00:00"));
}

}  // namespace
}  // namespace internal
}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner
}  // namespace cloud
}  // namespace google
