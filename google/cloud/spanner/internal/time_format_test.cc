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

#include "google/cloud/spanner/internal/time_format.h"
#include <gmock/gmock.h>
#include <string>

namespace google {
namespace cloud {
namespace spanner {
inline namespace SPANNER_CLIENT_NS {
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

}  // namespace
}  // namespace internal
}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner
}  // namespace cloud
}  // namespace google
