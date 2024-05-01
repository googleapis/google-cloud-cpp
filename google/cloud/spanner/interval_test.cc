// Copyright 2024 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "google/cloud/spanner/interval.h"
#include "google/cloud/spanner/timestamp.h"
#include "google/cloud/testing_util/status_matchers.h"
#include "absl/time/time.h"
#include <gtest/gtest.h>
#include <limits>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace google {
namespace cloud {
namespace spanner {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::google::cloud::testing_util::StatusIs;
using std::chrono::duration;
using std::chrono::duration_cast;
using std::chrono::hours;
using std::chrono::microseconds;
using std::chrono::milliseconds;
using std::chrono::minutes;
using std::chrono::nanoseconds;
using std::chrono::seconds;

Timestamp MakeTimestamp(std::string const& s) {
  return spanner_internal::TimestampFromRFC3339(s).value();
}

TEST(Interval, RegularSemantics) {
  Interval const intvl(0, 1, 2, hours(3));

  Interval const copy1(intvl);
  EXPECT_EQ(copy1, intvl);

  Interval const copy2 = intvl;
  EXPECT_EQ(copy2, intvl);

  Interval assign;
  assign = intvl;
  EXPECT_EQ(assign, intvl);
}

// https://gist.github.com/henryivesjones/ebd653acbf61cb408380a49659e2be97
// is the source of some of the relational/arithmetic test cases.

TEST(Interval, RelationalOperators) {
  EXPECT_EQ(Interval(), Interval());

  EXPECT_LT(Interval(0, 1, 0), Interval(0, 2, 0));
  EXPECT_EQ(Interval(0, 1, 30), Interval(0, 2, 0));
  EXPECT_GT(Interval(0, 0, 365), Interval(1, 0, 0));
  EXPECT_GT(Interval(0, 0, 52 * 7), Interval(1, 0, 0));

  EXPECT_EQ(Interval(0, 2, -90, -hours(12)), Interval(0, -1, -1, hours(12)));

  // Check microsecond rounding.
  EXPECT_EQ(Interval(nanoseconds(1'250)), Interval(microseconds(1)));
  EXPECT_EQ(Interval(-nanoseconds(1'250)), Interval(-microseconds(1)));
  EXPECT_EQ(Interval(nanoseconds(1'500)), Interval(microseconds(2)));
  EXPECT_EQ(Interval(-nanoseconds(1'500)), Interval(-microseconds(2)));
  EXPECT_EQ(Interval(nanoseconds(2'500)), Interval(microseconds(2)));
  EXPECT_EQ(Interval(-nanoseconds(2'500)), Interval(-microseconds(2)));
  EXPECT_EQ(Interval(nanoseconds(2'750)), Interval(microseconds(3)));
  EXPECT_EQ(Interval(-nanoseconds(2'750)), Interval(-microseconds(3)));

  // Check that the logical value of an Interval used during comparison is
  // able to represent values beyond the std::chrono::nanoseconds limits.
  EXPECT_GT(Interval(296, 6, 12), Interval(nanoseconds::max()));
  EXPECT_LT(-Interval(296, 6, 12), Interval(nanoseconds::min()));
}

TEST(Interval, ArithmeticOperators) {
  // Negation.
  EXPECT_EQ(-Interval(), Interval());
  EXPECT_EQ(-Interval(0, 10, 11, hours(12)), Interval(0, -10, -11, -hours(12)));

  // Addition/subtraction.
  EXPECT_EQ(Interval(0, 1, 0) + Interval(0, 2, 0), Interval(0, 3, 0));
  EXPECT_EQ(Interval(1, 0, 0, hours(24)) + Interval(1, 0, 3),
            Interval(2, 0, 3, hours(24)));
  EXPECT_EQ(Interval(1, 0, 1) - Interval(hours(24)),
            Interval(1, 0, 1, -hours(24)));
  EXPECT_EQ(Interval(0, 11, 0) + Interval(0, 1, 1), Interval(1, 0, 1));
  EXPECT_EQ(Interval(hours(2) + minutes(50)) + Interval(minutes(10)),
            Interval(hours(3)));
  EXPECT_EQ(Interval(hours(2) + minutes(50)) - Interval(minutes(50)),
            Interval(hours(2)));

  // Multiplication/division.
  EXPECT_EQ(Interval(0, 1, 0) * 2, Interval(0, 2, 0));
  EXPECT_EQ(Interval(1, 6, 0) * 2, Interval(3, 0, 0));
  EXPECT_EQ(Interval(hours(5) + minutes(5)) * 2,
            Interval(hours(10) + minutes(10)));
  EXPECT_EQ(Interval(0, 0, 15, hours(24)) * 3, Interval(0, 1, 15, hours(72)));
  EXPECT_EQ(Interval(0, 1, 15) * 3, Interval(0, 3, 45));
  EXPECT_EQ(Interval(0, 1, 15) * 12, Interval(1, 0, 180));
  EXPECT_EQ(Interval(0, 1, 0) / 30, Interval(0, 0, 1));
  EXPECT_EQ(Interval(1, 0, 0) / 365,
            Interval(duration_cast<nanoseconds>(duration<double>(hours(24)) *
                                                (360.0 / 365))));
  EXPECT_EQ(Interval(1, 0, 0) / 12, Interval(0, 1, 0));
  EXPECT_EQ(Interval(0, 1, 0) / 4, Interval(0, 0, 7, hours(12)));
  EXPECT_EQ(Interval(0, 1, 0) * 0.5, Interval(0, 0, 15));
  EXPECT_EQ(Interval(minutes(1)) * 0.5, Interval(seconds(30)));
  EXPECT_EQ(Interval(0, 3, 30) / 30, Interval(0, 0, 4));
  EXPECT_EQ(Interval(milliseconds(1001)) / 1'000'000 * 1'000,
            Interval(microseconds(1001)));
  EXPECT_EQ(Interval(minutes(1)) * 600, Interval(hours(10)));
}

TEST(Interval, Range) {
  auto huge = Interval(178'000'000, 0, 0);
  EXPECT_EQ(std::string(huge), "178000000 years");
  EXPECT_EQ(std::string(-huge), "-178000000 years");

  EXPECT_LT(-huge, huge);
  EXPECT_GT(huge, huge - Interval(microseconds(1)));
  EXPECT_LT(-huge, -huge + Interval(microseconds(1)));
}

// Check that parsing the result of a string conversion yields the same value.
TEST(Interval, RoundTrip) {
  std::vector<Interval> test_cases = {
      Interval(),
      Interval(1, 0, 0),
      Interval(0, 2, 0),
      Interval(0, 0, 3),
      Interval(hours(4)),
      Interval(minutes(5)),
      Interval(seconds(6)),
      Interval(milliseconds(123)),
      Interval(microseconds(123456)),
      Interval(nanoseconds(123456789)),
  };

  for (auto const& tc : test_cases) {
    auto intvl = MakeInterval(std::string(tc));
    EXPECT_STATUS_OK(intvl) << tc;
    if (intvl) {
      EXPECT_EQ(*intvl, tc);
    }
    intvl = MakeInterval(std::string(-tc));
    EXPECT_STATUS_OK(intvl) << -tc;
    if (intvl) {
      EXPECT_EQ(*intvl, -tc);
    }
  }
}

// https://www.postgresql.org/docs/current/datatype-datetime.html and
// https://www.postgresqltutorial.com/postgresql-tutorial/postgresql-interval
// are the sources of some of the MakeInterval() test cases.

TEST(Interval, MakeInterval) {
  auto hms = [](int h, int m, int s) {
    return hours(h) + minutes(m) + seconds(s);
  };
  std::vector<std::pair<std::string, Interval>> test_cases = {
      {"2 microseconds", Interval(microseconds(2))},
      {"3 milliseconds", Interval(milliseconds(3))},
      {"4 seconds", Interval(seconds(4))},
      {"5 minutes", Interval(minutes(5))},
      {"6 hours", Interval(hours(6))},
      {"7 days", Interval(0, 0, 7)},
      {"8 weeks", Interval(0, 0, 8 * 7)},
      {"9 months", Interval(0, 9, 0)},
      {"10 years", Interval(10, 0, 0)},
      {"11 decades", Interval(11 * 10, 0, 0)},
      {"12 centuries", Interval(12 * 100, 0, 0)},
      {"13 millennia", Interval(13 * 1'000, 0, 0)},

      {"1 century", Interval(100, 0, 0)},
      {"1 millennium", Interval(1'000, 0, 0)},

      {"1.5 years", Interval(1, 6, 0)},
      {"1.75 months", Interval(0, 1, 22, hms(12, 0, 0))},
      {"@-1.5 years", Interval(-1, -6, 0)},

      {"@ 1 year 2 mons", Interval(1, 2, 0)},
      {"1 year 2 mons", Interval(1, 2, 0)},
      {"1-2", Interval(1, 2, 0)},

      {"@ 3 days 4 hours 5 mins 6 secs", Interval(0, 0, 3, hms(4, 5, 6))},
      {"3 days 04:05:06", Interval(0, 0, 3, hms(4, 5, 6))},
      {"3 4:05:06", Interval(0, 0, 3, hms(4, 5, 6))},

      {" 6 years 5 months 4 days 3 hours 2 minutes 1 second ",
       Interval(6, 5, 4, hms(3, 2, 1))},
      {" @ 6 years 5 mons 4 days 3 hours 2 mins 1 sec ",
       Interval(6, 5, 4, hms(3, 2, 1))},
      {" 6 years 5 mons 4 days 03:02:01 ", Interval(6, 5, 4, hms(3, 2, 1))},
      {" +6-5 +4 +3:02:01 ", Interval(6, 5, 4, hms(3, 2, 1))},

      {"1 year 2 months 3 days 4 hours 5 minutes 6 seconds",
       Interval(1, 2, 3, hms(4, 5, 6))},
      {"-1 year -2 mons +3 days -04:05:06",
       Interval(-1, -2, 3, hms(-4, -5, -6))},
      {"-1-2 +3 -4:05:06", Interval(-1, 2, 3, hms(-4, -5, -6))},

      {"@ 1 year 2 mons -3 days 4 hours 5 mins 6 secs ago",
       Interval(-2, 10, 3, hms(-4, -5, -6))},

      {"17h 20m 05s", Interval(hms(17, 20, 5))},

      {"-42 microseconds", Interval(-microseconds(42))},
      {"+87 milliseconds", Interval(milliseconds(87))},

      {"4 decades", Interval(40, 0, 0)},
      {"3 centuries", Interval(300, 0, 0)},
      {"2 millennia", Interval(2'000, 0, 0)},

      {"", Interval()},
      {"ago", Interval()},
  };

  for (auto const& tc : test_cases) {
    auto intvl = MakeInterval(tc.first);
    EXPECT_STATUS_OK(intvl) << tc.first;
    if (!intvl) continue;
    EXPECT_EQ(*intvl, tc.second);
  }

  EXPECT_THAT(MakeInterval("junk"), StatusIs(StatusCode::kInvalidArgument));

  // Check that we reject double plurals.
  EXPECT_THAT(MakeInterval("7 dayss"), StatusIs(StatusCode::kInvalidArgument));
}

// Output streaming of an Interval is defined to use the string conversion
// operator, so here we simply verify that output streaming is available.
TEST(Interval, OutputStreaming) {
  std::ostringstream os;
  os << Interval(1, 2, 3,
                 hours(4) + minutes(5) + seconds(6) + nanoseconds(123456789));
  EXPECT_EQ("1 year 2 months 3 days 04:05:06.123456789", os.str());
}

TEST(Interval, TimestampOperations) {
  auto hms = [](int h, int m, int s) {
    return absl::Hours(h) + absl::Minutes(m) + absl::Seconds(s);
  };

  auto utc = absl::UTCTimeZone();
  absl::TimeZone nyc;
  if (!absl::LoadTimeZone("America/New_York", &nyc)) GTEST_SKIP();

  // Some simple cases of zero-length intervals.
  EXPECT_EQ(Timestamp(), Add(Timestamp(), Interval(), utc));
  EXPECT_EQ(Timestamp(), Add(Timestamp(), Interval(), nyc));
  EXPECT_EQ(Interval(), Diff(Timestamp(), Timestamp(), utc));
  EXPECT_EQ(Interval(), Diff(Timestamp(), Timestamp(), nyc));

  // Over continuous civil-time segments, Timestamp/Interval operations
  // behave in obvious ways.
  EXPECT_EQ(MakeTimestamp("2022-04-06T08:10:12.123457788Z"),
            Add(MakeTimestamp("2021-02-03T04:05:06.123456789Z"),
                Interval(1, 2, 3, hms(4, 5, 6) + absl::Nanoseconds(999)), utc));
  EXPECT_EQ(Interval(0, 0, 427, hms(4, 5, 6) + absl::Nanoseconds(999)),
            Diff(MakeTimestamp("2022-04-06T08:10:12.123457788Z"),
                 MakeTimestamp("2021-02-03T04:05:06.123456789Z"), utc));

  // If we cross a Feb 29 there is an extra day.
  EXPECT_EQ(MakeTimestamp("2021-04-06T08:10:12.123457788Z"),
            Add(MakeTimestamp("2020-02-03T04:05:06.123456789Z"),
                Interval(1, 2, 3, hms(4, 5, 6) + absl::Nanoseconds(999)), utc));
  EXPECT_EQ(Interval(0, 0, 428, hms(4, 5, 6) + absl::Nanoseconds(999)),
            Diff(MakeTimestamp("2021-04-06T08:10:12.123457788Z"),
                 MakeTimestamp("2020-02-03T04:05:06.123456789Z"), utc));

  // Over civil-time discontinuities, two civil hours is either one absolute
  // hour (skipped) or three absolute hours (repeated).
  EXPECT_EQ(MakeTimestamp("2023-03-12T03:02:03.456789-04:00"),
            Add(MakeTimestamp("2023-03-12T01:02:03.456789-05:00"),
                Interval(0, 0, 0, absl::Hours(2)), nyc));
  EXPECT_EQ(Interval(absl::Hours(2)),
            Diff(MakeTimestamp("2023-03-12T03:02:03.456789-04:00"),
                 MakeTimestamp("2023-03-12T01:02:03.456789-05:00"), nyc));
  EXPECT_EQ(MakeTimestamp("2023-11-05T03:02:03.456789-05:00"),
            Add(MakeTimestamp("2023-11-05T01:02:03.456789-04:00"),
                Interval(0, 0, 0, absl::Hours(2)), nyc));
  EXPECT_EQ(Interval(absl::Hours(2)),
            Diff(MakeTimestamp("2023-11-05T03:02:03.456789-05:00"),
                 MakeTimestamp("2023-11-05T01:02:03.456789-04:00"), nyc));
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace spanner
}  // namespace cloud
}  // namespace google
