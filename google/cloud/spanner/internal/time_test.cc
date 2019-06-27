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

namespace google {
namespace cloud {
namespace spanner {
inline namespace SPANNER_CLIENT_NS {
namespace internal {
namespace {

using Duration = google::protobuf::Duration;
using Timestamp = google::protobuf::Timestamp;
using microseconds = std::chrono::microseconds;
using nanoseconds = std::chrono::nanoseconds;
using system_clock = std::chrono::system_clock;
using time_point = system_clock::time_point;

TEST(Duration, ToProto) {
  Duration d;

  d = ToProto(nanoseconds(-1234567890));
  EXPECT_EQ(-1, d.seconds());
  EXPECT_EQ(-234567890, d.nanos());

  d = ToProto(nanoseconds(-1000000001));
  EXPECT_EQ(-1, d.seconds());
  EXPECT_EQ(-1, d.nanos());

  d = ToProto(nanoseconds(-1000000000));
  EXPECT_EQ(-1, d.seconds());
  EXPECT_EQ(0, d.nanos());

  d = ToProto(nanoseconds(-999999999));
  EXPECT_EQ(0, d.seconds());
  EXPECT_EQ(-999999999, d.nanos());

  d = ToProto(nanoseconds(-1));
  EXPECT_EQ(0, d.seconds());
  EXPECT_EQ(-1, d.nanos());

  d = ToProto(nanoseconds(0));
  EXPECT_EQ(0, d.seconds());
  EXPECT_EQ(0, d.nanos());

  d = ToProto(nanoseconds(1));
  EXPECT_EQ(0, d.seconds());
  EXPECT_EQ(1, d.nanos());

  d = ToProto(nanoseconds(999999999));
  EXPECT_EQ(0, d.seconds());
  EXPECT_EQ(999999999, d.nanos());

  d = ToProto(nanoseconds(1000000000));
  EXPECT_EQ(1, d.seconds());
  EXPECT_EQ(0, d.nanos());

  d = ToProto(nanoseconds(1000000001));
  EXPECT_EQ(1, d.seconds());
  EXPECT_EQ(1, d.nanos());

  d = ToProto(nanoseconds(1234567890));
  EXPECT_EQ(1, d.seconds());
  EXPECT_EQ(234567890, d.nanos());
}

TEST(Duration, FromProto) {
  Duration d;

  d.set_seconds(-1);
  d.set_nanos(-234567890);
  EXPECT_EQ(nanoseconds(-1234567890), FromProto(d));

  d.set_seconds(-1);
  d.set_nanos(-1);
  EXPECT_EQ(nanoseconds(-1000000001), FromProto(d));

  d.set_seconds(-1);
  d.set_nanos(0);
  EXPECT_EQ(nanoseconds(-1000000000), FromProto(d));

  d.set_seconds(0);
  d.set_nanos(-999999999);
  EXPECT_EQ(nanoseconds(-999999999), FromProto(d));

  d.set_seconds(0);
  d.set_nanos(-1);
  EXPECT_EQ(nanoseconds(-1), FromProto(d));

  d.set_seconds(0);
  d.set_nanos(0);
  EXPECT_EQ(nanoseconds(0), FromProto(d));

  d.set_seconds(0);
  d.set_nanos(1);
  EXPECT_EQ(nanoseconds(1), FromProto(d));

  d.set_seconds(0);
  d.set_nanos(999999999);
  EXPECT_EQ(nanoseconds(999999999), FromProto(d));

  d.set_seconds(1);
  d.set_nanos(0);
  EXPECT_EQ(nanoseconds(1000000000), FromProto(d));

  d.set_seconds(1);
  d.set_nanos(1);
  EXPECT_EQ(nanoseconds(1000000001), FromProto(d));

  d.set_seconds(1);
  d.set_nanos(234567890);
  EXPECT_EQ(nanoseconds(1234567890), FromProto(d));
}

TEST(Time, ToProto) {
  Timestamp ts;

  ts = ToProto(system_clock::from_time_t(-1) - microseconds(999999));
  EXPECT_EQ(-2, ts.seconds());
  EXPECT_EQ(1000, ts.nanos());

  ts = ToProto(system_clock::from_time_t(-1) - microseconds(1));
  EXPECT_EQ(-2, ts.seconds());
  EXPECT_EQ(999999000, ts.nanos());

  ts = ToProto(system_clock::from_time_t(-1));
  EXPECT_EQ(-1, ts.seconds());
  EXPECT_EQ(0, ts.nanos());

  ts = ToProto(system_clock::from_time_t(0) - microseconds(999999));
  EXPECT_EQ(-1, ts.seconds());
  EXPECT_EQ(1000, ts.nanos());

  ts = ToProto(system_clock::from_time_t(0) - microseconds(1));
  EXPECT_EQ(-1, ts.seconds());
  EXPECT_EQ(999999000, ts.nanos());

  ts = ToProto(system_clock::from_time_t(0));
  EXPECT_EQ(0, ts.seconds());
  EXPECT_EQ(0, ts.nanos());

  ts = ToProto(system_clock::from_time_t(0) + microseconds(1));
  EXPECT_EQ(0, ts.seconds());
  EXPECT_EQ(1000, ts.nanos());

  ts = ToProto(system_clock::from_time_t(0) + microseconds(999999));
  EXPECT_EQ(0, ts.seconds());
  EXPECT_EQ(999999000, ts.nanos());

  ts = ToProto(system_clock::from_time_t(1));
  EXPECT_EQ(1, ts.seconds());
  EXPECT_EQ(0, ts.nanos());

  ts = ToProto(system_clock::from_time_t(1) + microseconds(1));
  EXPECT_EQ(1, ts.seconds());
  EXPECT_EQ(1000, ts.nanos());

  ts = ToProto(system_clock::from_time_t(1) + microseconds(999999));
  EXPECT_EQ(1, ts.seconds());
  EXPECT_EQ(999999000, ts.nanos());
}

TEST(Time, FromProto) {
  Timestamp ts;

  ts.set_seconds(-2);
  ts.set_nanos(1000);
  EXPECT_EQ(system_clock::from_time_t(-1) - microseconds(999999),
            FromProto(ts));

  ts.set_seconds(-2);
  ts.set_nanos(999999000);
  EXPECT_EQ(system_clock::from_time_t(-1) - microseconds(1), FromProto(ts));

  ts.set_seconds(-1);
  ts.set_nanos(0);
  EXPECT_EQ(system_clock::from_time_t(-1), FromProto(ts));

  ts.set_seconds(-1);
  ts.set_nanos(1000);
  EXPECT_EQ(system_clock::from_time_t(0) - microseconds(999999), FromProto(ts));

  ts.set_seconds(-1);
  ts.set_nanos(999999000);
  EXPECT_EQ(system_clock::from_time_t(0) - microseconds(1), FromProto(ts));

  ts.set_seconds(0);
  ts.set_nanos(0);
  EXPECT_EQ(system_clock::from_time_t(0), FromProto(ts));

  ts.set_seconds(0);
  ts.set_nanos(1000);
  EXPECT_EQ(system_clock::from_time_t(0) + microseconds(1), FromProto(ts));

  ts.set_seconds(0);
  ts.set_nanos(999999000);
  EXPECT_EQ(system_clock::from_time_t(0) + microseconds(999999), FromProto(ts));

  ts.set_seconds(1);
  ts.set_nanos(0);
  EXPECT_EQ(system_clock::from_time_t(1), FromProto(ts));

  ts.set_seconds(1);
  ts.set_nanos(1000);
  EXPECT_EQ(system_clock::from_time_t(1) + microseconds(1), FromProto(ts));

  ts.set_seconds(1);
  ts.set_nanos(999999000);
  EXPECT_EQ(system_clock::from_time_t(1) + microseconds(999999), FromProto(ts));
}

TEST(Time, TimestampToString) {
  time_point tp = system_clock::from_time_t(1561135942);
  EXPECT_EQ("2019-06-21T16:52:22Z", TimestampToString(tp));
  tp += microseconds(6);
  EXPECT_EQ("2019-06-21T16:52:22.000006Z", TimestampToString(tp));
  tp += microseconds(50);
  EXPECT_EQ("2019-06-21T16:52:22.000056Z", TimestampToString(tp));
  tp += microseconds(400);
  EXPECT_EQ("2019-06-21T16:52:22.000456Z", TimestampToString(tp));
  tp += microseconds(3000);
  EXPECT_EQ("2019-06-21T16:52:22.003456Z", TimestampToString(tp));
  tp += microseconds(20000);
  EXPECT_EQ("2019-06-21T16:52:22.023456Z", TimestampToString(tp));
  tp += microseconds(100000);
  EXPECT_EQ("2019-06-21T16:52:22.123456Z", TimestampToString(tp));
  tp -= microseconds(6);
  EXPECT_EQ("2019-06-21T16:52:22.12345Z", TimestampToString(tp));
  tp -= microseconds(50);
  EXPECT_EQ("2019-06-21T16:52:22.1234Z", TimestampToString(tp));
  tp -= microseconds(400);
  EXPECT_EQ("2019-06-21T16:52:22.123Z", TimestampToString(tp));
  tp -= microseconds(3000);
  EXPECT_EQ("2019-06-21T16:52:22.12Z", TimestampToString(tp));
  tp -= microseconds(20000);
  EXPECT_EQ("2019-06-21T16:52:22.1Z", TimestampToString(tp));
  tp -= microseconds(100000);
  EXPECT_EQ("2019-06-21T16:52:22Z", TimestampToString(tp));
}

TEST(Time, TimestampToStringLimit) {
  time_point tp = system_clock::from_time_t(-9223372036L);
  EXPECT_EQ("1677-09-21T00:12:44Z", TimestampToString(tp));

  tp = system_clock::from_time_t(9223372036L) + microseconds(775807);
  EXPECT_EQ("2262-04-11T23:47:16.775807Z", TimestampToString(tp));
}

TEST(Time, TimestampFromString) {
  time_point tp = system_clock::from_time_t(1561135942);
  EXPECT_EQ(tp, TimestampFromString("2019-06-21T16:52:22Z").value());
  tp += microseconds(6);
  EXPECT_EQ(tp, TimestampFromString("2019-06-21T16:52:22.000006Z").value());
  tp += microseconds(50);
  EXPECT_EQ(tp, TimestampFromString("2019-06-21T16:52:22.000056Z").value());
  tp += microseconds(400);
  EXPECT_EQ(tp, TimestampFromString("2019-06-21T16:52:22.000456Z").value());
  tp += microseconds(3000);
  EXPECT_EQ(tp, TimestampFromString("2019-06-21T16:52:22.003456Z").value());
  tp += microseconds(20000);
  EXPECT_EQ(tp, TimestampFromString("2019-06-21T16:52:22.023456Z").value());
  tp += microseconds(100000);
  EXPECT_EQ(tp, TimestampFromString("2019-06-21T16:52:22.123456Z").value());
  tp -= microseconds(6);
  EXPECT_EQ(tp, TimestampFromString("2019-06-21T16:52:22.12345Z").value());
  tp -= microseconds(50);
  EXPECT_EQ(tp, TimestampFromString("2019-06-21T16:52:22.1234Z").value());
  tp -= microseconds(400);
  EXPECT_EQ(tp, TimestampFromString("2019-06-21T16:52:22.123Z").value());
  tp -= microseconds(3000);
  EXPECT_EQ(tp, TimestampFromString("2019-06-21T16:52:22.12Z").value());
  tp -= microseconds(20000);
  EXPECT_EQ(tp, TimestampFromString("2019-06-21T16:52:22.1Z").value());
  tp -= microseconds(100000);
  EXPECT_EQ(tp, TimestampFromString("2019-06-21T16:52:22Z").value());
}

TEST(Time, TimestampFromStringFailure) {
  EXPECT_FALSE(TimestampFromString(""));
  EXPECT_FALSE(TimestampFromString("garbage in"));
  EXPECT_FALSE(TimestampFromString("2019-06-21T16:52:22"));
  EXPECT_FALSE(TimestampFromString("2019-06-21T16:52:22.9"));
  EXPECT_FALSE(TimestampFromString("2019-06-21T16:52:22.Z"));
  EXPECT_FALSE(TimestampFromString("2019-06-21T16:52:22ZX"));
}

}  // namespace
}  // namespace internal
}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner
}  // namespace cloud
}  // namespace google
