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

#include "google/cloud/internal/time_format.h"
#include <gmock/gmock.h>
#include <string>

namespace google {
namespace cloud {
inline namespace GOOGLE_CLOUD_CPP_NS {
namespace internal {
namespace {

constexpr auto kTimeFormat = "%Y-%m-%dT%H:%M:%S";

TEST(TimeFormat, Format) {
  std::tm tm;
  tm.tm_year = 2019 - 1900;
  tm.tm_mon = 6 - 1;
  tm.tm_mday = 21;
  tm.tm_hour = 16;
  tm.tm_min = 52;
  tm.tm_sec = 22;
  EXPECT_EQ("2019-06-21T16:52:22", FormatTime(kTimeFormat, tm));
}

TEST(TimeFormat, Parse) {
  std::tm tm{};

  EXPECT_EQ(19, ParseTime(kTimeFormat, "2019-06-21T16:52:22", &tm));
  EXPECT_EQ(tm.tm_year, 2019 - 1900);
  EXPECT_EQ(tm.tm_mon, 6 - 1);
  EXPECT_EQ(tm.tm_mday, 21);
  EXPECT_EQ(tm.tm_hour, 16);
  EXPECT_EQ(tm.tm_min, 52);
  EXPECT_EQ(tm.tm_sec, 22);

  EXPECT_EQ(19, ParseTime(kTimeFormat, "2020-07-22T17:53:23xxx", &tm));
  EXPECT_EQ(tm.tm_year, 2020 - 1900);
  EXPECT_EQ(tm.tm_mon, 7 - 1);
  EXPECT_EQ(tm.tm_mday, 22);
  EXPECT_EQ(tm.tm_hour, 17);
  EXPECT_EQ(tm.tm_min, 53);
  EXPECT_EQ(tm.tm_sec, 23);

  EXPECT_EQ(std::string::npos, ParseTime(kTimeFormat, "garbage in", &tm));
}

TEST(TimeFormat, TimestampToString) {
  using std::chrono::microseconds;
  auto tp = std::chrono::system_clock::from_time_t(1561135942);
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

TEST(TimeFormat, TimestampToStringLimit) {
  using std::chrono::microseconds;
  using std::chrono::system_clock;
  auto tp = system_clock::from_time_t(-9223372036L);
  EXPECT_EQ("1677-09-21T00:12:44Z", TimestampToString(tp));

  tp = system_clock::from_time_t(9223372036L) + microseconds(775807);
  EXPECT_EQ("2262-04-11T23:47:16.775807Z", TimestampToString(tp));
}

TEST(TimeFormat, TimestampFromStringZ) {
  using std::chrono::microseconds;
  using std::chrono::system_clock;
  auto tp = system_clock::from_time_t(1561135942);
  EXPECT_EQ(tp, TimestampFromStringZ("2019-06-21T16:52:22Z").value());
  tp += microseconds(6);
  EXPECT_EQ(tp, TimestampFromStringZ("2019-06-21T16:52:22.000006Z").value());
  tp += microseconds(50);
  EXPECT_EQ(tp, TimestampFromStringZ("2019-06-21T16:52:22.000056Z").value());
  tp += microseconds(400);
  EXPECT_EQ(tp, TimestampFromStringZ("2019-06-21T16:52:22.000456Z").value());
  tp += microseconds(3000);
  EXPECT_EQ(tp, TimestampFromStringZ("2019-06-21T16:52:22.003456Z").value());
  tp += microseconds(20000);
  EXPECT_EQ(tp, TimestampFromStringZ("2019-06-21T16:52:22.023456Z").value());
  tp += microseconds(100000);
  EXPECT_EQ(tp, TimestampFromStringZ("2019-06-21T16:52:22.123456Z").value());
  tp -= microseconds(6);
  EXPECT_EQ(tp, TimestampFromStringZ("2019-06-21T16:52:22.12345Z").value());
  tp -= microseconds(50);
  EXPECT_EQ(tp, TimestampFromStringZ("2019-06-21T16:52:22.1234Z").value());
  tp -= microseconds(400);
  EXPECT_EQ(tp, TimestampFromStringZ("2019-06-21T16:52:22.123Z").value());
  tp -= microseconds(3000);
  EXPECT_EQ(tp, TimestampFromStringZ("2019-06-21T16:52:22.12Z").value());
  tp -= microseconds(20000);
  EXPECT_EQ(tp, TimestampFromStringZ("2019-06-21T16:52:22.1Z").value());
  tp -= microseconds(100000);
  EXPECT_EQ(tp, TimestampFromStringZ("2019-06-21T16:52:22Z").value());
}

TEST(TimeFormat, TimestampFromStringZFailure) {
  EXPECT_FALSE(TimestampFromStringZ(""));
  EXPECT_FALSE(TimestampFromStringZ("garbage in"));
  EXPECT_FALSE(TimestampFromStringZ("2019-06-21T16:52:22"));
  EXPECT_FALSE(TimestampFromStringZ("2019-06-21T16:52:22.9"));
  EXPECT_FALSE(TimestampFromStringZ("2019-06-21T16:52:22.9+01:00"));
  EXPECT_FALSE(TimestampFromStringZ("2019-06-21T16:52:22.Z"));
  EXPECT_FALSE(TimestampFromStringZ("2019-06-21T16:52:22ZX"));
}

TEST(TimeFormat, TimestampFromString) {
  using std::chrono::hours;
  using std::chrono::microseconds;
  using std::chrono::minutes;
  using std::chrono::system_clock;
  // Use `date --date=2019-06-21T16:52:22Z +%s` to get the timestamp in seconds.
  auto tp = system_clock::from_time_t(1561135942);
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
  tp -= minutes(7);
  EXPECT_EQ(tp, TimestampFromString("2019-06-21T16:52:22+00:07").value());
  tp -= hours(2);
  EXPECT_EQ(tp, TimestampFromString("2019-06-21T16:52:22+02:07").value());
  tp += hours(5) + minutes(7);
  EXPECT_EQ(tp, TimestampFromString("2019-06-21T16:52:22-03:00").value());
}

TEST(TimeFormat, TimestampFromStringFailure) {
  EXPECT_FALSE(TimestampFromString(""));
  EXPECT_FALSE(TimestampFromString("garbage in"));
  EXPECT_FALSE(TimestampFromString("2019-06-21T16:52:22"));
  EXPECT_FALSE(TimestampFromString("2019-06-21T16:52:22.9"));
  EXPECT_FALSE(TimestampFromString("2019-06-21T16:52:22.Z"));
  EXPECT_FALSE(TimestampFromString("2019-06-21T16:52:22ZX"));
  EXPECT_FALSE(TimestampFromString("2019-06-21T16:52:22.9+"));
  EXPECT_FALSE(TimestampFromString("2019-06-21T16:52:22.9-"));
  EXPECT_FALSE(TimestampFromString("2019-06-21T16:52:22.9/01:00"));
  EXPECT_FALSE(TimestampFromString("2019-06-21T16:52:22.9+25:00"));
  EXPECT_FALSE(TimestampFromString("2019-06-21T16:52:22.9+01:70"));
  EXPECT_FALSE(TimestampFromString("2019-06-21T16:52:22.9q"));
}

}  // namespace
}  // namespace internal
}  // namespace GOOGLE_CLOUD_CPP_NS
}  // namespace cloud
}  // namespace google
