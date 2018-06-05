// Copyright 2018 Google Inc.
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

#include "google/cloud/bigtable/benchmarks/benchmark.h"
#include <gmock/gmock.h>

using namespace google::cloud::bigtable::benchmarks;
using namespace std::chrono;

TEST(BenchmarksFormatDuration, NanoSeconds) {
  std::ostringstream os;
  os << FormatDuration(nanoseconds(123));
  EXPECT_EQ("123ns", os.str());
}

TEST(BenchmarksFormatDuration, MicroSeconds) {
  std::ostringstream os;
  os << FormatDuration(nanoseconds(2345));
  EXPECT_EQ("2.345us", os.str());
}

TEST(BenchmarksFormatDuration, MilliSeconds) {
  std::ostringstream os;
  os << FormatDuration(nanoseconds(234567800));
  EXPECT_EQ("234.567ms", os.str());
}

TEST(BenchmarksFormatDuration, WithZeroInMicros) {
  std::ostringstream os;
  os << FormatDuration(nanoseconds(234056001));
  EXPECT_EQ("234.056ms", os.str());
}

TEST(BenchmarksFormatDuration, MilliSeconds10) {
  std::ostringstream os;
  os << FormatDuration(milliseconds(10));
  EXPECT_EQ("10.000ms", os.str());
}

TEST(BenchmarksFormatDuration, MicroSeconds100) {
  std::ostringstream os;
  os << FormatDuration(microseconds(100));
  EXPECT_EQ("100.000us", os.str());
}

TEST(BenchmarksFormatDuration, Full) {
  auto duration = hours(1) + minutes(2) + seconds(3) + milliseconds(456);
  std::ostringstream os;
  os << FormatDuration(duration);
  EXPECT_EQ("1h2m3.456s", os.str());
}

TEST(BenchmarksFormatDuration, NoHours) {
  auto duration = minutes(2) + seconds(3) + milliseconds(456);
  std::ostringstream os;
  os << FormatDuration(duration);
  EXPECT_EQ("2m3.456s", os.str());
}

TEST(BenchmarksFormatDuration, NoMinutes) {
  auto duration = hours(1) + seconds(3) + milliseconds(456);
  std::ostringstream os;
  os << FormatDuration(duration);
  EXPECT_EQ("1h3.456s", os.str());
}

TEST(BenchmarksFormatDuration, NoSeconds) {
  auto duration = hours(1) + minutes(2) + milliseconds(456);
  std::ostringstream os;
  os << FormatDuration(duration);
  EXPECT_EQ("1h2m0.456s", os.str());
}

TEST(BenchmarksFormatDuration, NoMillis) {
  auto duration = hours(1) + minutes(2) + seconds(3) + nanoseconds(1);
  std::ostringstream os;
  os << FormatDuration(duration);
  EXPECT_EQ("1h2m3s", os.str());
}
