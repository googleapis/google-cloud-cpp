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
#include <limits>

namespace google {
namespace cloud {
namespace spanner {
inline namespace SPANNER_CLIENT_NS {
namespace {

using ::testing::HasSubstr;

TEST(Timestamp, FromRFC3339) {
  EXPECT_EQ(internal::TimestampFromCounts(1561135942, 0).value(),
            internal::TimestampFromRFC3339("2019-06-21T16:52:22Z").value());
  EXPECT_EQ(
      internal::TimestampFromCounts(1561135942, 9).value(),
      internal::TimestampFromRFC3339("2019-06-21T16:52:22.000000009Z").value());
  EXPECT_EQ(
      internal::TimestampFromCounts(1561135942, 89).value(),
      internal::TimestampFromRFC3339("2019-06-21T16:52:22.000000089Z").value());
  EXPECT_EQ(
      internal::TimestampFromCounts(1561135942, 789).value(),
      internal::TimestampFromRFC3339("2019-06-21T16:52:22.000000789Z").value());
  EXPECT_EQ(
      internal::TimestampFromCounts(1561135942, 6789).value(),
      internal::TimestampFromRFC3339("2019-06-21T16:52:22.000006789Z").value());
  EXPECT_EQ(
      internal::TimestampFromCounts(1561135942, 56789).value(),
      internal::TimestampFromRFC3339("2019-06-21T16:52:22.000056789Z").value());
  EXPECT_EQ(
      internal::TimestampFromCounts(1561135942, 456789).value(),
      internal::TimestampFromRFC3339("2019-06-21T16:52:22.000456789Z").value());
  EXPECT_EQ(
      internal::TimestampFromCounts(1561135942, 3456789).value(),
      internal::TimestampFromRFC3339("2019-06-21T16:52:22.003456789Z").value());
  EXPECT_EQ(
      internal::TimestampFromCounts(1561135942, 23456789).value(),
      internal::TimestampFromRFC3339("2019-06-21T16:52:22.023456789Z").value());
  EXPECT_EQ(
      internal::TimestampFromCounts(1561135942, 123456789).value(),
      internal::TimestampFromRFC3339("2019-06-21T16:52:22.123456789Z").value());
  EXPECT_EQ(
      internal::TimestampFromCounts(1561135942, 123456780).value(),
      internal::TimestampFromRFC3339("2019-06-21T16:52:22.12345678Z").value());
  EXPECT_EQ(
      internal::TimestampFromCounts(1561135942, 123456700).value(),
      internal::TimestampFromRFC3339("2019-06-21T16:52:22.1234567Z").value());
  EXPECT_EQ(
      internal::TimestampFromCounts(1561135942, 123456000).value(),
      internal::TimestampFromRFC3339("2019-06-21T16:52:22.123456Z").value());
  EXPECT_EQ(
      internal::TimestampFromCounts(1561135942, 123450000).value(),
      internal::TimestampFromRFC3339("2019-06-21T16:52:22.12345Z").value());
  EXPECT_EQ(
      internal::TimestampFromCounts(1561135942, 123400000).value(),
      internal::TimestampFromRFC3339("2019-06-21T16:52:22.1234Z").value());
  EXPECT_EQ(internal::TimestampFromCounts(1561135942, 123000000).value(),
            internal::TimestampFromRFC3339("2019-06-21T16:52:22.123Z").value());
  EXPECT_EQ(internal::TimestampFromCounts(1561135942, 120000000).value(),
            internal::TimestampFromRFC3339("2019-06-21T16:52:22.12Z").value());
  EXPECT_EQ(internal::TimestampFromCounts(1561135942, 100000000).value(),
            internal::TimestampFromRFC3339("2019-06-21T16:52:22.1Z").value());
}

TEST(Timestamp, FromRFC3339Offset) {
  EXPECT_EQ(
      internal::TimestampFromCounts(1546398245, 0).value(),
      internal::TimestampFromRFC3339("2019-01-02T03:04:05+00:00").value());
  EXPECT_EQ(
      internal::TimestampFromCounts(1546398245 + 3600 + 120, 0).value(),
      internal::TimestampFromRFC3339("2019-01-02T03:04:05+01:02").value());
  EXPECT_EQ(
      internal::TimestampFromCounts(1546398245 - 3600 - 120, 0).value(),
      internal::TimestampFromRFC3339("2019-01-02T03:04:05-01:02").value());
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
  EXPECT_FALSE(internal::TimestampFromRFC3339("2019-06-21T16:52:22+00:00:00"));
}

TEST(Timestamp, FromRFC3339Limit) {
  // Verify Spanner range requirements.
  EXPECT_EQ(
      internal::TimestampFromCounts(-62135596800, 0).value(),
      internal::TimestampFromRFC3339("0001-01-01T00:00:00.000000000Z").value());
  EXPECT_EQ(
      internal::TimestampFromCounts(253402300799, 999999999).value(),
      internal::TimestampFromRFC3339("9999-12-31T23:59:59.999999999Z").value());

  // std::tm range limits (including -1900 tm_year bias).
  EXPECT_EQ(
      internal::TimestampFromCounts(-67768040609740800, 0).value(),
      internal::TimestampFromRFC3339("-2147481748-01-01T00:00:00.000000000Z")
          .value());
  EXPECT_EQ(
      internal::TimestampFromCounts(67768036191676799, 999999999).value(),
      internal::TimestampFromRFC3339("2147485547-12-31T23:59:59.999999999Z")
          .value());

  // One nanosecond beyond std::tm range limits fails to parse.
  EXPECT_FALSE(
      internal::TimestampFromRFC3339("-2147481749-12-31T23:59:59.999999999Z"));
  EXPECT_FALSE(
      internal::TimestampFromRFC3339("2147485548-01-01T00:00:00.000000000Z"));
}

TEST(Timestamp, ToRFC3339) {
  EXPECT_EQ("2019-06-21T16:52:22Z",
            internal::TimestampToRFC3339(
                internal::TimestampFromCounts(1561135942, 0).value()));
  EXPECT_EQ("2019-06-21T16:52:22.000000009Z",
            internal::TimestampToRFC3339(
                internal::TimestampFromCounts(1561135942, 9).value()));
  EXPECT_EQ("2019-06-21T16:52:22.000000089Z",
            internal::TimestampToRFC3339(
                internal::TimestampFromCounts(1561135942, 89).value()));
  EXPECT_EQ("2019-06-21T16:52:22.000000789Z",
            internal::TimestampToRFC3339(
                internal::TimestampFromCounts(1561135942, 789).value()));
  EXPECT_EQ("2019-06-21T16:52:22.000006789Z",
            internal::TimestampToRFC3339(
                internal::TimestampFromCounts(1561135942, 6789).value()));
  EXPECT_EQ("2019-06-21T16:52:22.000056789Z",
            internal::TimestampToRFC3339(
                internal::TimestampFromCounts(1561135942, 56789).value()));
  EXPECT_EQ("2019-06-21T16:52:22.000456789Z",
            internal::TimestampToRFC3339(
                internal::TimestampFromCounts(1561135942, 456789).value()));
  EXPECT_EQ("2019-06-21T16:52:22.003456789Z",
            internal::TimestampToRFC3339(
                internal::TimestampFromCounts(1561135942, 3456789).value()));
  EXPECT_EQ("2019-06-21T16:52:22.023456789Z",
            internal::TimestampToRFC3339(
                internal::TimestampFromCounts(1561135942, 23456789).value()));
  EXPECT_EQ("2019-06-21T16:52:22.123456789Z",
            internal::TimestampToRFC3339(
                internal::TimestampFromCounts(1561135942, 123456789).value()));
  EXPECT_EQ("2019-06-21T16:52:22.12345678Z",
            internal::TimestampToRFC3339(
                internal::TimestampFromCounts(1561135942, 123456780).value()));
  EXPECT_EQ("2019-06-21T16:52:22.1234567Z",
            internal::TimestampToRFC3339(
                internal::TimestampFromCounts(1561135942, 123456700).value()));
  EXPECT_EQ("2019-06-21T16:52:22.123456Z",
            internal::TimestampToRFC3339(
                internal::TimestampFromCounts(1561135942, 123456000).value()));
  EXPECT_EQ("2019-06-21T16:52:22.12345Z",
            internal::TimestampToRFC3339(
                internal::TimestampFromCounts(1561135942, 123450000).value()));
  EXPECT_EQ("2019-06-21T16:52:22.1234Z",
            internal::TimestampToRFC3339(
                internal::TimestampFromCounts(1561135942, 123400000).value()));
  EXPECT_EQ("2019-06-21T16:52:22.123Z",
            internal::TimestampToRFC3339(
                internal::TimestampFromCounts(1561135942, 123000000).value()));
  EXPECT_EQ("2019-06-21T16:52:22.12Z",
            internal::TimestampToRFC3339(
                internal::TimestampFromCounts(1561135942, 120000000).value()));
  EXPECT_EQ("2019-06-21T16:52:22.1Z",
            internal::TimestampToRFC3339(
                internal::TimestampFromCounts(1561135942, 100000000).value()));
}

TEST(Timestamp, ToRFC3339Limit) {
  // Spanner range requirements.
  EXPECT_EQ("0001-01-01T00:00:00Z",
            internal::TimestampToRFC3339(
                internal::TimestampFromCounts(-62135596800, 0).value()));
  EXPECT_EQ(
      "9999-12-31T23:59:59.999999999Z",
      internal::TimestampToRFC3339(
          internal::TimestampFromCounts(253402300799, 999999999).value()));

  // std::tm range limits (including -1900 tm_year bias).
  EXPECT_EQ("-2147481748-01-01T00:00:00Z",
            internal::TimestampToRFC3339(
                internal::TimestampFromCounts(-67768040609740800, 0).value()));
  EXPECT_EQ(
      "2147485547-12-31T23:59:59.999999999Z",
      internal::TimestampToRFC3339(
          internal::TimestampFromCounts(67768036191676799, 999999999).value()));

  // One nanosecond beyond std::tm range limits gives unspecified behavior.
  // Here we expect that we *do not* observe the "right" outputs.
  EXPECT_NE("-2147481749-12-31T23:59:59.999999999Z",
            internal::TimestampToRFC3339(
                internal::TimestampFromCounts(-67768040609740801, 999999999)
                    .value()));
  EXPECT_NE("2147485548-01-01T00:00:00",
            internal::TimestampToRFC3339(
                internal::TimestampFromCounts(67768036191676800, 0).value()));
}

TEST(Timestamp, FromProto) {
  google::protobuf::Timestamp proto;

  proto.set_seconds(0);
  proto.set_nanos(0);
  EXPECT_EQ(internal::TimestampFromCounts(0, 0).value(),
            internal::TimestampFromProto(proto));

  proto.set_seconds(1576030524);
  proto.set_nanos(611422667);
  EXPECT_EQ(internal::TimestampFromCounts(1576030524, 611422667).value(),
            internal::TimestampFromProto(proto));

  proto.set_seconds(-62135596800);
  proto.set_nanos(0);
  EXPECT_EQ(internal::TimestampFromCounts(-62135596800, 0).value(),
            internal::TimestampFromProto(proto));

  proto.set_seconds(253402300799);
  proto.set_nanos(999999999);
  EXPECT_EQ(internal::TimestampFromCounts(253402300799, 999999999).value(),
            internal::TimestampFromProto(proto));
}

TEST(Timestamp, ToProto) {
  auto proto =
      internal::TimestampToProto(internal::TimestampFromCounts(0, 0).value());
  EXPECT_EQ(0, proto.seconds());
  EXPECT_EQ(0, proto.nanos());

  proto = internal::TimestampToProto(
      internal::TimestampFromCounts(1576030524, 611422667).value());
  EXPECT_EQ(1576030524, proto.seconds());
  EXPECT_EQ(611422667, proto.nanos());

  proto = internal::TimestampToProto(
      internal::TimestampFromCounts(-62135596800, 0).value());
  EXPECT_EQ(-62135596800, proto.seconds());
  EXPECT_EQ(0, proto.nanos());

  proto = internal::TimestampToProto(
      internal::TimestampFromCounts(253402300799, 999999999).value());
  EXPECT_EQ(253402300799, proto.seconds());
  EXPECT_EQ(999999999, proto.nanos());
}

TEST(Timestamp, FromChrono) {
  // A chrono duration that can hold values beyond the range of Timestamp.
  using big_minutes = std::chrono::duration<std::int64_t, std::ratio<60>>;

  // Assumes the system_clock epoch is hour-aligned with the Unix epoch.
  auto const unix_epoch = std::chrono::time_point_cast<std::chrono::hours>(
      std::chrono::system_clock::from_time_t(0));

  auto const tp1 = unix_epoch + std::chrono::seconds(2123456789) +
                   std::chrono::nanoseconds(123456789);
  EXPECT_EQ(internal::TimestampFromCounts(2123456789, 123456789).value(),
            MakeTimestamp(tp1).value());

  auto const tp2 = unix_epoch + std::chrono::seconds(2123456789) +
                   std::chrono::microseconds(123456789);
  EXPECT_EQ(internal::TimestampFromCounts(2123456789, 123456789000).value(),
            MakeTimestamp(tp2).value());

  auto const tp3 = unix_epoch + std::chrono::seconds(2123456789) +
                   std::chrono::milliseconds(123456789);
  EXPECT_EQ(internal::TimestampFromCounts(2123456789, 123456789000000).value(),
            MakeTimestamp(tp3).value());

  auto const tp4 = unix_epoch + std::chrono::minutes(2123456789);
  EXPECT_EQ(internal::TimestampFromCounts(2123456789LL * 60, 0).value(),
            MakeTimestamp(tp4).value());

  auto const tp5 = unix_epoch + std::chrono::hours(2123456789);
  EXPECT_EQ(internal::TimestampFromCounts(2123456789LL * 60 * 60, 0).value(),
            MakeTimestamp(tp5).value());

  auto const tp_big_pos = unix_epoch + big_minutes(153722867280912931LL);
  auto const ts_big_pos = MakeTimestamp(tp_big_pos);
  EXPECT_FALSE(ts_big_pos.ok());
  EXPECT_THAT(ts_big_pos.status().message(), HasSubstr("positive overflow"));

  auto const tp6 = unix_epoch - std::chrono::seconds(2123456789) +
                   std::chrono::nanoseconds(123456789);
  EXPECT_EQ(internal::TimestampFromCounts(-2123456789, 123456789).value(),
            MakeTimestamp(tp6).value());

  auto const tp7 = unix_epoch - std::chrono::seconds(2123456789) +
                   std::chrono::microseconds(123456789);
  EXPECT_EQ(internal::TimestampFromCounts(-2123456789, 123456789000).value(),
            MakeTimestamp(tp7).value());

  auto const tp8 = unix_epoch - std::chrono::seconds(2123456789) +
                   std::chrono::milliseconds(123456789);
  EXPECT_EQ(internal::TimestampFromCounts(-2123456789, 123456789000000).value(),
            MakeTimestamp(tp8).value());

  auto const tp9 = unix_epoch - std::chrono::minutes(2123456789);
  EXPECT_EQ(internal::TimestampFromCounts(-2123456789LL * 60, 0).value(),
            MakeTimestamp(tp9).value());

  auto const tp10 = unix_epoch - std::chrono::hours(2123456789);
  EXPECT_EQ(internal::TimestampFromCounts(-2123456789LL * 60 * 60, 0).value(),
            MakeTimestamp(tp10).value());

  auto const tp_big_neg = unix_epoch - big_minutes(153722867280912931LL);
  auto const ts_big_neg = MakeTimestamp(tp_big_neg);
  EXPECT_FALSE(ts_big_neg.ok());
  EXPECT_THAT(ts_big_neg.status().message(), HasSubstr("negative overflow"));
}

TEST(Timestamp, ToChrono) {
  // Assumes the system_clock epoch is hour-aligned with the Unix epoch.
  auto const epoch = std::chrono::time_point_cast<std::chrono::seconds>(
      std::chrono::system_clock::from_time_t(0));

  auto const ts_pos =
      internal::TimestampFromCounts(2123456789, 123456789).value();

  auto const tp1 = epoch + std::chrono::seconds(2123456789) +
                   std::chrono::nanoseconds(123456789);
  EXPECT_EQ(tp1, ts_pos.get<sys_time<std::chrono::nanoseconds>>().value());

  auto const tp2 = epoch + std::chrono::seconds(2123456789) +
                   std::chrono::microseconds(123456);
  EXPECT_EQ(tp2, ts_pos.get<sys_time<std::chrono::microseconds>>().value());

  auto const tp3 =
      epoch + std::chrono::seconds(2123456789) + std::chrono::milliseconds(123);
  EXPECT_EQ(tp3, ts_pos.get<sys_time<std::chrono::milliseconds>>().value());

  auto const tp4 = epoch + std::chrono::seconds(2123456789);
  EXPECT_EQ(tp4, ts_pos.get<sys_time<std::chrono::seconds>>().value());

  auto const tp5 = epoch + std::chrono::hours(2123456789 / 60 / 60);
  EXPECT_EQ(tp5, ts_pos.get<sys_time<std::chrono::hours>>().value());

  auto const ts_big_pos = internal::TimestampFromCounts(20000000000, 0).value();
  auto const tp_big_pos = ts_big_pos.get<sys_time<std::chrono::nanoseconds>>();
  EXPECT_FALSE(tp_big_pos.ok());
  EXPECT_THAT(tp_big_pos.status().message(), HasSubstr("positive overflow"));

  auto const ts_neg =
      internal::TimestampFromCounts(-2123456789, 123456789).value();

  auto const tp6 = epoch - std::chrono::seconds(2123456789) +
                   std::chrono::nanoseconds(123456789);
  EXPECT_EQ(tp6, ts_neg.get<sys_time<std::chrono::nanoseconds>>().value());

  auto const tp7 = epoch - std::chrono::seconds(2123456789) +
                   std::chrono::microseconds(123456);
  EXPECT_EQ(tp7, ts_neg.get<sys_time<std::chrono::microseconds>>().value());

  auto const tp8 =
      epoch - std::chrono::seconds(2123456789) + std::chrono::milliseconds(123);
  EXPECT_EQ(tp8, ts_neg.get<sys_time<std::chrono::milliseconds>>().value());

  auto const tp9 = epoch - std::chrono::seconds(2123456789);
  EXPECT_EQ(tp9, ts_neg.get<sys_time<std::chrono::seconds>>().value());

  auto const tp10 = epoch - std::chrono::hours(2123456789 / 60 / 60);
  EXPECT_EQ(tp10, ts_neg.get<sys_time<std::chrono::hours>>().value());

  auto const ts_big_neg =
      internal::TimestampFromCounts(-20000000000, 0).value();
  auto const tp_big_neg = ts_big_neg.get<sys_time<std::chrono::nanoseconds>>();
  EXPECT_FALSE(tp_big_neg.ok());
  EXPECT_THAT(tp_big_neg.status().message(), HasSubstr("negative overflow"));
}

TEST(Timestamp, FromCounts) {
  constexpr std::int64_t kMax = std::numeric_limits<std::int64_t>::max();
  auto ts = internal::TimestampFromCounts(kMax, 999999999);
  EXPECT_TRUE(ts.ok());
  ts = internal::TimestampFromCounts(kMax, 1000000000);
  EXPECT_FALSE(ts.ok());
  EXPECT_THAT(ts.status().message(), HasSubstr("positive overflow"));

  constexpr std::int64_t kMin = std::numeric_limits<std::int64_t>::min();
  ts = internal::TimestampFromCounts(kMin, 0);
  EXPECT_TRUE(ts.ok());
  ts = internal::TimestampFromCounts(kMin, -1);
  EXPECT_FALSE(ts.ok());
  EXPECT_THAT(ts.status().message(), HasSubstr("negative overflow"));
}

TEST(Timestamp, RelationalOperators) {
  auto const ts1 = internal::TimestampFromCounts(1576030524, 611422667);
  auto ts2 = ts1;
  EXPECT_EQ(ts2, ts1);

  ts2 = internal::TimestampFromCounts(1576030524, 999999999);
  EXPECT_NE(ts2, ts1);

  ts2 = internal::TimestampFromCounts(1111111111, 611422667);
  EXPECT_NE(ts2, ts1);
}

}  // namespace
}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner
}  // namespace cloud
}  // namespace google
