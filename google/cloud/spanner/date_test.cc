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

#include "google/cloud/spanner/date.h"
#include <gmock/gmock.h>
#include <sstream>
#include <vector>

namespace google {
namespace cloud {
namespace spanner {
inline namespace SPANNER_CLIENT_NS {
namespace {

TEST(Date, Basics) {
  Date d(2019, 6, 21);
  EXPECT_EQ(2019, d.year());
  EXPECT_EQ(6, d.month());
  EXPECT_EQ(21, d.day());

  Date copy = d;
  EXPECT_EQ(copy, d);

  Date d2(2019, 6, 22);
  EXPECT_NE(d2, d);
}

TEST(Date, RelationalOperators) {
  Date d1(2019, 6, 21);
  Date d2(2019, 6, 22);

  EXPECT_EQ(d1, d1);
  EXPECT_LE(d1, d1);
  EXPECT_GE(d1, d1);

  EXPECT_NE(d1, d2);
  EXPECT_LT(d1, d2);
  EXPECT_LE(d1, d2);
  EXPECT_GE(d2, d1);
  EXPECT_GT(d2, d1);
}

TEST(Date, Normalization) {
  // Non-leap-year day overflow.
  EXPECT_EQ(Date(2019, 3, 1), Date(2019, 2, 29));

  // Non-leap-year day underflow.
  EXPECT_EQ(Date(2019, 2, 28), Date(2019, 3, 0));

  // Leap-year day overflow.
  EXPECT_EQ(Date(2020, 3, 1), Date(2020, 2, 30));

  // Leap-year day underflow.
  EXPECT_EQ(Date(2020, 2, 29), Date(2020, 3, 0));

  // Month overflow.
  EXPECT_EQ(Date(2018, 1, 28), Date(2016, 25, 28));

  // Month underflow.
  EXPECT_EQ(Date(2013, 11, 28), Date(2016, -25, 28));

  // Four-century overflow.
  EXPECT_EQ(Date(2816, 1, 1), Date(2016, 1, 292195));

  // Four-century underflow.
  EXPECT_EQ(Date(1215, 12, 30), Date(2016, 1, -292195));

  // Mixed.
  EXPECT_EQ(Date(2012, 9, 30), Date(2016, -42, 122));
}

TEST(Date, OutputStream) {
  struct TestCase {
    Date date;
    std::string expected;
  };

  std::vector<TestCase> test_cases = {
      {Date(1, 1, 1), "0001-01-01"},
      {Date(1970, 1, 1), "1970-01-01"},
      {Date(2020, 3, 14), "2020-03-14"},
      {Date(9999, 12, 31), "9999-12-31"},
  };

  for (auto const& tc : test_cases) {
    std::ostringstream ss;
    ss << tc.date;
    EXPECT_EQ(ss.str(), tc.expected);
  }
}

}  // namespace
}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner
}  // namespace cloud
}  // namespace google
