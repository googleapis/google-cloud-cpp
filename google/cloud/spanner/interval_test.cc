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

class IntervalTest : public Interval {
 public:
  explicit IntervalTest(std::chrono::nanoseconds offset) : Interval(offset) {}
  IntervalTest(
      std::int32_t years, std::int32_t months, std::int32_t days,
      std::chrono::nanoseconds offset = std::chrono::nanoseconds::zero())
      : Interval(years, months, days, offset) {}

  IntervalTest(Interval i)  // NOLINT(google-explicit-constructor)
      : Interval(i.months_, i.days_, i.offset_) {}

  using Interval::operator+;
  using Interval::operator-;
  using Interval::operator*;
};

namespace {

using ::google::cloud::testing_util::StatusIs;

using std::chrono::duration;
using std::chrono::hours;
using std::chrono::microseconds;
using std::chrono::milliseconds;
using std::chrono::minutes;
using std::chrono::nanoseconds;
using std::chrono::seconds;

auto HMS(int h, int m, int s) { return hours(h) + minutes(m) + seconds(s); }

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
  EXPECT_EQ(-Interval(-1, 2, -3, HMS(-4, 5, -6)),
            Interval(1, -2, 3, HMS(4, -5, 6)));

  // Addition/subtraction.
  EXPECT_EQ(IntervalTest(0, 1, 0) + Interval(0, 2, 0), Interval(0, 3, 0));
  EXPECT_EQ(IntervalTest(1, 0, 0, hours(24)) + Interval(1, 0, 3),
            Interval(2, 0, 3, hours(24)));
  EXPECT_EQ(IntervalTest(1, 0, 1) - Interval(hours(24)),
            Interval(1, 0, 1, -hours(24)));
  EXPECT_EQ(IntervalTest(0, 11, 0) + Interval(0, 1, 1), Interval(1, 0, 1));
  EXPECT_EQ(IntervalTest(hours(2) + minutes(50)) + Interval(minutes(10)),
            Interval(hours(3)));
  EXPECT_EQ(IntervalTest(hours(2) + minutes(50)) - Interval(minutes(50)),
            Interval(hours(2)));

  // Multiplication
  EXPECT_EQ(IntervalTest(0, 1, 0) * 2, Interval(0, 2, 0));
  EXPECT_EQ(IntervalTest(1, 6, 0) * 2, Interval(3, 0, 0));
  EXPECT_EQ(IntervalTest(hours(5) + minutes(5)) * 2,
            Interval(hours(10) + minutes(10)));
  EXPECT_EQ(IntervalTest(0, 0, 15, hours(24)) * 3,
            Interval(0, 1, 15, hours(72)));
  EXPECT_EQ(IntervalTest(0, 1, 15) * 3, Interval(0, 3, 45));
  EXPECT_EQ(IntervalTest(0, 1, 15) * 12, Interval(1, 0, 180));
  EXPECT_EQ(IntervalTest(0, 1, 0) * 0.5, Interval(0, 0, 15));
  EXPECT_EQ(IntervalTest(minutes(1)) * 0.5, Interval(seconds(30)));
  EXPECT_EQ(IntervalTest(minutes(1)) * 600, Interval(hours(10)));
}

TEST(Interval, Range) {
  auto huge = IntervalTest(178'000'000, 0, 0);
  EXPECT_EQ(std::string(huge), "P178000000Y");
  EXPECT_EQ(std::string(-huge), "P-178000000Y");

  EXPECT_LT(-huge, huge);
  EXPECT_GT(huge, huge - Interval(microseconds(1)));
  EXPECT_LT(-huge, IntervalTest(-huge) + Interval(microseconds(1)));
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
      Interval(0, 11, 27, HMS(3, 55, 6)),
      Interval(-1, 2, -3, HMS(4, -5, 6)),
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

// https://gist.github.com/rbeauchamp/6d8f8ffe22358665a9dd6571b79e1d5f
// is the source of many of the MakeInterval() test cases.
TEST(Interval, MakeIntervalISO8601) {
  std::vector<std::pair<std::string, Interval>> test_cases = {
      {"P3Y6M4DT12H30M5S", Interval(3, 6, 4, HMS(12, 30, 5))},
      {"-P3Y6M4DT12H30M5S", -Interval(3, 6, 4, HMS(12, 30, 5))},
      {"P1DT2H3M4S", Interval(0, 0, 1, HMS(2, 3, 4))},
      {"PT1H30M", Interval(0, 0, 0, HMS(1, 30, 0))},
      {"P1Y", Interval(1, 0, 0)},
      {"P1M", Interval(0, 1, 0)},
      {"P2M", Interval(0, 2, 0)},
      {"P1W", Interval(0, 0, 7)},
      {"P1D", Interval(0, 0, 1)},
      {"PT1H", Interval(0, 0, 0, HMS(1, 0, 0))},
      {"PT1M", Interval(0, 0, 0, HMS(0, 1, 0))},
      {"PT1S", Interval(0, 0, 0, HMS(0, 0, 1))},
      {"-P1Y", -Interval(1, 0, 0)},
      {"-P1M", -Interval(0, 1, 0)},
      {"-P2M", -Interval(0, 2, 0)},
      {"-P1W", -Interval(0, 0, 7)},
      {"-P1D", -Interval(0, 0, 1)},
      {"-PT1H", -Interval(0, 0, 0, HMS(1, 0, 0))},
      {"-PT1M", -Interval(0, 0, 0, HMS(0, 1, 0))},
      {"-PT1S", -Interval(0, 0, 0, HMS(0, 0, 1))},
      {"PT1H1M1S", Interval(0, 0, 0, HMS(1, 1, 1))},
      {"-PT1H1M1S", -Interval(0, 0, 0, HMS(1, 1, 1))},
      {"P1Y2M3DT4H5M6S", Interval(1, 2, 3, HMS(4, 5, 6))},
      {"P2Y10M19DT23H59M59S", Interval(2, 10, 19, HMS(23, 59, 59))},
      {"-P1Y2M3DT4H5M6S", -Interval(1, 2, 3, HMS(4, 5, 6))},
      {"-P2Y10M19DT23H59M59S", -Interval(2, 10, 19, HMS(23, 59, 59))},
      {"P9998Y", Interval(9998, 0, 0)},
      {"-P9998Y", -Interval(9998, 0, 0)},
      {"PT0S", Interval()},
      {"PT0M0S", Interval()},
      {"PT0H0M0S", Interval()},
      {"P0DT0H0M0S", Interval()},
      {"P0W", Interval()},
      {"P0M", Interval()},
      {"P0Y", Interval()},
      {"P0Y0M0DT0H0M0.1S", Interval(0, 0, 0, milliseconds(100))},
      {"P0Y0M1DT0H0M0S", Interval(0, 0, 1)},
      {"P0Y1M0DT0H0M0S", Interval(0, 1, 0)},
      {"P1Y0M0DT0H0M0S", Interval(1, 0, 0)},
      {"P1Y2M3D", Interval(1, 2, 3)},
      {"P2Y", Interval(2, 0, 0)},
      {"P1M1D", Interval(0, 1, 1)},
      // Additional locally-generated cases.
      {"P23DT23H", Interval(0, 0, 23, HMS(23, 0, 0))},
      {"PT36H", Interval(0, 0, 0, HMS(36, 0, 0))},
      {"P1DT12H", Interval(0, 0, 1, HMS(12, 0, 0))},
      {"-P-1Y-2M-3DT-4H-5M-6S", -Interval(-1, -2, -3, HMS(-4, -5, -6))},
      {"P0.5Y", Interval(0, 6, 0)},
      {"P0,5Y", Interval(0, 6, 0)},
      {"P0.25M", Interval(0, 0, 7, HMS(12, 0, 0))},
      {"P2.5W", Interval(0, 0, 17, HMS(12, 0, 0))},
      {"P0.1D", Interval(0, 0, 0, HMS(2, 24, 0))},
      {"PT0.5H", Interval(0, 0, 0, HMS(0, 30, 0))},
      {"PT0.25M", Interval(0, 0, 0, HMS(0, 0, 15))},
      {"PT0.1S", Interval(0, 0, 0, milliseconds(100))},
  };

  for (auto const& tc : test_cases) {
    auto intvl = MakeInterval(tc.first);
    EXPECT_STATUS_OK(intvl) << tc.first;
    if (!intvl) continue;
    EXPECT_EQ(*intvl, tc.second);
  }

  std::vector<std::string> error_cases = {
      "P",
      "PT",
      "P1",
      "P1Y2M3DT",
      "P1Y1M1DT1H1M1S1",
      "P1Y1M1DT1H1M1S1D",
      "P1Y1M1DT1H1M1S1H",
      "P1Y1M1DT1H1M1S1M",
      "P1Y1M1DT1H1M1S1S",
      "PT1H1M1S1Y",
      "P1Y1M1DT1H1M1S-",
      "P1Y1M1DT1H1M1S+",
      "P1Y1M1DT1H1M1SS",
      "P1Y1M1DT1H1M1MM",
      "P1Y1M1DT1H1M1HH",
      "P1Y1M1DT1H1M1DD",
      "P1Y1M1DT1H1M1YY",
      // Additional locally-generated cases.
      "P0.5Y1D",
      "P1D1Y",
      "PT1S1M",
  };

  for (auto const& ec : error_cases) {
    EXPECT_THAT(MakeInterval(ec), StatusIs(StatusCode::kInvalidArgument));
  }
}

// https://www.postgresql.org/docs/current/datatype-datetime.html and
// https://www.postgresqltutorial.com/postgresql-tutorial/postgresql-interval
// are the sources of some of the MakeInterval() test cases.
TEST(Interval, MakeIntervalPostgreSQL) {
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
      {"1.75 months", Interval(0, 1, 22, HMS(12, 0, 0))},
      {"@-1.5 years", Interval(-1, -6, 0)},

      {"@ 1 year 2 mons", Interval(1, 2, 0)},
      {"1 year 2 mons", Interval(1, 2, 0)},
      {"1-2", Interval(1, 2, 0)},

      {"@ 3 days 4 hours 5 mins 6 secs", Interval(0, 0, 3, HMS(4, 5, 6))},
      {"3 days 04:05:06", Interval(0, 0, 3, HMS(4, 5, 6))},
      {"3 4:05:06", Interval(0, 0, 3, HMS(4, 5, 6))},

      {" 6 years 5 months 4 days 3 hours 2 minutes 1 second ",
       Interval(6, 5, 4, HMS(3, 2, 1))},
      {" @ 6 years 5 mons 4 days 3 hours 2 mins 1 sec ",
       Interval(6, 5, 4, HMS(3, 2, 1))},
      {" 6 years 5 mons 4 days 03:02:01 ", Interval(6, 5, 4, HMS(3, 2, 1))},
      {" +6-5 +4 +3:02:01 ", Interval(6, 5, 4, HMS(3, 2, 1))},

      {"1 year 2 months 3 days 4 hours 5 minutes 6 seconds",
       Interval(1, 2, 3, HMS(4, 5, 6))},
      {"-1 year -2 mons +3 days -04:05:06",
       Interval(-1, -2, 3, HMS(-4, -5, -6))},
      {"-1-2 +3 -4:05:06", Interval(-1, 2, 3, HMS(-4, -5, -6))},

      {"@ 1 year 2 mons -3 days 4 hours 5 mins 6 secs ago",
       Interval(-2, 10, 3, HMS(-4, -5, -6))},

      {"17h 20m 05s", Interval(HMS(17, 20, 5))},

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
  os << Interval(1, 2, 3, HMS(4, 5, 6) + nanoseconds(123456789));
  EXPECT_EQ("P1Y2M3DT4H5M6.123456789S", os.str());
}

TEST(Interval, Justification) {
  EXPECT_EQ(std::string(Interval(0, 0, 35)), "P35D");
  EXPECT_EQ(std::string(JustifyDays(Interval(0, 0, 35))), "P1M5D");
  EXPECT_EQ(std::string(Interval(0, 0, -35)), "P-35D");
  EXPECT_EQ(std::string(JustifyDays(Interval(0, 0, -35))), "P-2M25D");

  EXPECT_EQ(std::string(Interval(hours(27))), "PT27H");
  EXPECT_EQ(std::string(JustifyHours(Interval(hours(27)))), "P1DT3H");
  EXPECT_EQ(std::string(Interval(-hours(27))), "PT-27H");
  EXPECT_EQ(std::string(JustifyHours(Interval(-hours(27)))), "P-2DT21H");

  EXPECT_EQ(std::string(JustifyInterval(Interval(0, 1, 0, -hours(1)))),
            "P29DT23H");
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace spanner
}  // namespace cloud
}  // namespace google
