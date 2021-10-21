// Copyright 2018 Google LLC
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

#include "google/cloud/internal/parse_rfc3339.h"
#include <gtest/gtest.h>
#include <ctime>

namespace google {
namespace cloud {
inline namespace GOOGLE_CLOUD_CPP_NS {
namespace internal {
namespace {

using ::std::chrono::duration_cast;
using ::std::chrono::milliseconds;
using ::std::chrono::nanoseconds;
using ::std::chrono::seconds;

TEST(ParseRfc3339Test, ParseEpoch) {
  auto timestamp = ParseRfc3339("1970-01-01T00:00:00Z");
  // The C++ 11, 14 and 17 standards do not guarantee that the system clock's
  // epoch is actually the same as the Unix Epoch. Luckily, the platforms we
  // support actually have that property, and C++ 20 fixes things. If this test
  // breaks because somebody is unlucky enough to have a C++ compiler + library
  // with a different Epoch then at least we know what the problem is.
  EXPECT_EQ(0, timestamp.time_since_epoch().count())
      << "Your C++ compiler, library, and/or your operating system does\n"
      << "not use the Unix epoch (1970-01-01T00:00:00Z) as its epoch.\n"
      << "The Google Cloud C++ Libraries have not been ported to\n"
      << "environments where this is the case.  Please contact us at\n"
      << "   https://github.com/googleapis/google-cloud-cpp/issues\n"
      << "and provide as many details as possible about your build\n"
      << "environment.";
}

TEST(ParseRfc3339Test, ParseSimpleZulu) {
  struct Timestamps {
    std::string input;
    std::time_t expected;
  } tests[] = {
      // Use `date -u +%s --date='....'` to get the expected values.
      {"2018-05-18T14:42:03Z", 1526654523L},
      {"2020-01-01T00:00:00Z", 1577836800L},
      {"2020-01-31T00:00:00Z", 1580428800L},
      {"2020-02-29T00:00:00Z", 1582934400L},
      {"2020-03-31T00:00:00Z", 1585612800L},
      {"2020-04-30T00:00:00Z", 1588204800L},
      {"2020-05-31T00:00:00Z", 1590883200L},
      {"2020-06-30T00:00:00Z", 1593475200L},
      {"2020-07-31T00:00:00Z", 1596153600L},
      {"2020-08-31T00:00:00Z", 1598832000L},
      {"2020-09-30T00:00:00Z", 1601424000L},
      {"2020-10-31T00:00:00Z", 1604102400L},
      {"2020-11-20T00:00:00Z", 1605830400L},
      {"2020-12-31T00:00:00Z", 1609372800L},
      {"2020-01-01T00:00:59Z", 1577836859L},
      {"2020-01-01T00:59:59Z", 1577840399L},
      {"2020-01-01T23:59:59Z", 1577923199L},
  };
  for (auto const& test : tests) {
    auto timestamp = ParseRfc3339(test.input);
    auto actual = std::chrono::system_clock::to_time_t(timestamp);
    EXPECT_EQ(actual, test.expected)
        << " when testing with input=" << test.input;
  }
}

TEST(ParseRfc3339Test, ParseAlternativeSeparators) {
  auto timestamp = ParseRfc3339("2018-05-18t14:42:03z");
  // Use `date -u +%s --date='2018-05-18T14:42:03'` to get the magic value:
  EXPECT_EQ(1526654523L,
            duration_cast<seconds>(timestamp.time_since_epoch()).count());
}

TEST(ParseRfc3339Test, ParseFractional) {
  auto timestamp = ParseRfc3339("2018-05-18T14:42:03.123456789Z");
  // Use `date -u +%s --date='2018-05-18T14:42:03'` to get the magic value:
  auto actual_seconds = duration_cast<seconds>(timestamp.time_since_epoch());
  EXPECT_EQ(1526654523L, actual_seconds.count());

  bool const system_clock_has_nanos = std::ratio_greater_equal<
      std::nano, std::chrono::system_clock::duration::period>::value;
  if (system_clock_has_nanos) {
    auto actual_nanoseconds = duration_cast<nanoseconds>(
        timestamp.time_since_epoch() - actual_seconds);
    EXPECT_EQ(123456789L, actual_nanoseconds.count());
  } else {
    // On platforms where the system clock has less than nanosecond precision
    // just check for milliseconds, we could check at the highest possible
    // precision but that is overkill.
    auto actual_milliseconds = duration_cast<milliseconds>(
        timestamp.time_since_epoch() - actual_seconds);
    EXPECT_EQ(123L, actual_milliseconds.count());
  }
}

TEST(ParseRfc3339Test, ParseFractionalMoreThanNanos) {
  auto timestamp = ParseRfc3339("2018-05-18T14:42:03.1234567890123Z");
  // Use `date -u +%s --date='2018-05-18T14:42:03'` to get the magic value:
  auto actual_seconds = duration_cast<seconds>(timestamp.time_since_epoch());
  EXPECT_EQ(1526654523L, actual_seconds.count());
  bool const system_clock_has_nanos = std::ratio_greater_equal<
      std::nano, std::chrono::system_clock::duration::period>::value;
  if (system_clock_has_nanos) {
    auto actual_nanoseconds = duration_cast<nanoseconds>(
        timestamp.time_since_epoch() - actual_seconds);
    EXPECT_EQ(123456789L, actual_nanoseconds.count());
  } else {
    // On platforms where the system clock has less than nanosecond precision
    // just check for milliseconds, we could check at the highest possible
    // precision but that is overkill.
    auto actual_milliseconds = duration_cast<milliseconds>(
        timestamp.time_since_epoch() - actual_seconds);
    EXPECT_EQ(123L, actual_milliseconds.count());
  }
}

TEST(ParseRfc3339Test, ParseFractionalLessThanNanos) {
  auto timestamp = ParseRfc3339("2018-05-18T14:42:03.123456Z");
  // Use `date -u +%s --date='2018-05-18T14:42:03'` to get the magic value:
  auto actual_seconds = duration_cast<seconds>(timestamp.time_since_epoch());
  EXPECT_EQ(1526654523L, actual_seconds.count());
  auto actual_nanoseconds =
      duration_cast<nanoseconds>(timestamp.time_since_epoch() - actual_seconds);
  EXPECT_EQ(123456000L, actual_nanoseconds.count());
}

TEST(ParseRfc3339Test, ParseWithOffset) {
  auto timestamp = ParseRfc3339("2018-05-18T14:42:03+08:00");
  // Use `date -u +%s --date='2018-05-18T14:42:03+08:00'` to get the magic
  // value.
  auto actual_seconds = duration_cast<seconds>(timestamp.time_since_epoch());
  EXPECT_EQ(1526625723L, actual_seconds.count());
}

TEST(ParseRfc3339Test, ParseFull) {
  auto timestamp = ParseRfc3339("2018-05-18T14:42:03.5-01:05");
  // Use `date -u +%s --date='2018-05-18T14:42:03.5-01:05'` to get the magic
  // value.
  auto actual_seconds = duration_cast<seconds>(timestamp.time_since_epoch());
  EXPECT_EQ(1526658423L, actual_seconds.count());
  auto actual_milliseconds = duration_cast<milliseconds>(
      timestamp.time_since_epoch() - actual_seconds);
  EXPECT_EQ(500, actual_milliseconds.count());
}

TEST(ParseRfc3339Test, DetectInvalidSeparator) {
#if GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
  EXPECT_THROW(ParseRfc3339("2018-05-18x14:42:03Z"), std::invalid_argument);
#else
  EXPECT_DEATH_IF_SUPPORTED(ParseRfc3339("2018-05-18x14:42:03Z"),
                            "exceptions are disabled");
#endif  // GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
#if GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
  EXPECT_THROW(ParseRfc3339("2018-05-18T14:42:03x"), std::invalid_argument);
#else
  EXPECT_DEATH_IF_SUPPORTED(ParseRfc3339("2018-05-18T14:42:03x"),
                            "exceptions are disabled");
#endif  // GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
}

TEST(ParseRfc3339Test, DetectLongMonth) {
#if GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
  EXPECT_THROW(ParseRfc3339("2018-123-18T14:42:03Z"), std::invalid_argument);
#else
  EXPECT_DEATH_IF_SUPPORTED(ParseRfc3339("2018-123-18T14:42:03Z"),
                            "exceptions are disabled");
#endif  // GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
}

TEST(ParseRfc3339Test, DetectOutOfRangeMonth) {
#if GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
  EXPECT_THROW(ParseRfc3339("2018-33-18T14:42:03Z"), std::invalid_argument);
#else
  EXPECT_DEATH_IF_SUPPORTED(ParseRfc3339("2018-33-18T14:42:03Z"),
                            "exceptions are disabled");
#endif  // GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
}

TEST(ParseRfc3339Test, DetectLongMDay) {
#if GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
  EXPECT_THROW(ParseRfc3339("2018-05-181T14:42:03Z"), std::invalid_argument);
#else
  EXPECT_DEATH_IF_SUPPORTED(ParseRfc3339("2018-05-181T14:42:03Z"),
                            "exceptions are disabled");
#endif  // GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
}

TEST(ParseRfc3339Test, DetectOutOfRangeMDay) {
#if GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
  EXPECT_THROW(ParseRfc3339("2018-05-55T14:42:03Z"), std::invalid_argument);
#else
  EXPECT_DEATH_IF_SUPPORTED(ParseRfc3339("2018-05-55T14:42:03Z"),
                            "exceptions are disabled");
#endif  // GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
}

TEST(ParseRfc3339Test, DetectOutOfRangeMDay30) {
#if GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
  EXPECT_THROW(ParseRfc3339("2018-06-31T14:42:03Z"), std::invalid_argument);
#else
  EXPECT_DEATH_IF_SUPPORTED(ParseRfc3339("2018-06-31T14:42:03Z"),
                            "exceptions are disabled");
#endif  // GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
}

TEST(ParseRfc3339Test, DetectOutOfRangeMDayFebLeap) {
#if GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
  EXPECT_THROW(ParseRfc3339("2016-02-30T14:42:03Z"), std::invalid_argument);
#else
  EXPECT_DEATH_IF_SUPPORTED(ParseRfc3339("2016-02-30T14:42:03Z"),
                            "exceptions are disabled");
#endif  // GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
}

TEST(ParseRfc3339Test, DetectOutOfRangeMDayFebNonLeap) {
#if GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
  EXPECT_THROW(ParseRfc3339("2017-02-29T14:42:03Z"), std::invalid_argument);
#else
  EXPECT_DEATH_IF_SUPPORTED(ParseRfc3339("2017-02-29T14:42:03Z"),
                            "exceptions are disabled");
#endif  // GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
}

TEST(ParseRfc3339Test, DetectLongHour) {
#if GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
  EXPECT_THROW(ParseRfc3339("2018-05-18T144:42:03Z"), std::invalid_argument);
#else
  EXPECT_DEATH_IF_SUPPORTED(ParseRfc3339("2018-05-18T144:42:03Z"),
                            "exceptions are disabled");
#endif  // GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
}

TEST(ParseRfc3339Test, DetectOutOfRangeHour) {
#if GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
  EXPECT_THROW(ParseRfc3339("2018-05-18T24:42:03Z"), std::invalid_argument);
#else
  EXPECT_DEATH_IF_SUPPORTED(ParseRfc3339("2018-05-18T24:42:03Z"),
                            "exceptions are disabled");
#endif  // GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
}

TEST(ParseRfc3339Test, DetectLongMinute) {
#if GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
  EXPECT_THROW(ParseRfc3339("2018-05-18T14:442:03Z"), std::invalid_argument);
#else
  EXPECT_DEATH_IF_SUPPORTED(ParseRfc3339("2018-05-18T14:442:03Z"),
                            "exceptions are disabled");
#endif  // GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
}

TEST(ParseRfc3339Test, DetectOutOfRangeMinute) {
#if GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
  EXPECT_THROW(ParseRfc3339("2018-05-18T22:60:03Z"), std::invalid_argument);
#else
  EXPECT_DEATH_IF_SUPPORTED(ParseRfc3339("2018-05-18T22:60:03Z"),
                            "exceptions are disabled");
#endif  // GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
}

TEST(ParseRfc3339Test, DetectLongSecond) {
#if GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
  EXPECT_THROW(ParseRfc3339("2018-05-18T14:42:003Z"), std::invalid_argument);
#else
  EXPECT_DEATH_IF_SUPPORTED(ParseRfc3339("2018-05-18T14:42:003Z"),
                            "exceptions are disabled");
#endif  // GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
}

TEST(ParseRfc3339Test, DetectOutOfRangeSecond) {
#if GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
  EXPECT_THROW(ParseRfc3339("2018-05-18T22:42:61Z"), std::invalid_argument);
#else
  EXPECT_DEATH_IF_SUPPORTED(ParseRfc3339("2018-05-18T22:42:61Z"),
                            "exceptions are disabled");
#endif  // GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
}

TEST(ParseRfc3339Test, DetectLongOffsetHour) {
#if GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
  EXPECT_THROW(ParseRfc3339("2018-05-18T14:42:03+008:00"),
               std::invalid_argument);
#else
  EXPECT_DEATH_IF_SUPPORTED(ParseRfc3339("2018-05-18T14:42:03+008:00"),
                            "exceptions are disabled");
#endif  // GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
}

TEST(ParseRfc3339Test, DetectShortOffsetHour) {
#if GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
  EXPECT_THROW(ParseRfc3339("2018-05-18T14:42:03+8:00"), std::invalid_argument);
#else
  EXPECT_DEATH_IF_SUPPORTED(ParseRfc3339("2018-05-18T14:42:03+8:00"),
                            "exceptions are disabled");
#endif  // GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
}

TEST(ParseRfc3339Test, DetectOutOfRangeOffsetHour) {
#if GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
  EXPECT_THROW(ParseRfc3339("2018-05-18T14:42:03+24:00"),
               std::invalid_argument);
#else
  EXPECT_DEATH_IF_SUPPORTED(ParseRfc3339("2018-05-18T14:42:03+24:00"),
                            "exceptions are disabled");
#endif  // GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
}

TEST(ParseRfc3339Test, DetectLongOffsetMinute) {
#if GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
  EXPECT_THROW(ParseRfc3339("2018-05-18T14:42:03+08:001"),
               std::invalid_argument);
#else
  EXPECT_DEATH_IF_SUPPORTED(ParseRfc3339("2018-05-18T14:42:03+08:001"),
                            "exceptions are disabled");
#endif  // GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
}

TEST(ParseRfc3339Test, DetectShortOffsetMinute) {
#if GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
  EXPECT_THROW(ParseRfc3339("2018-05-18T14:42:03+08:1"), std::invalid_argument);
#else
  EXPECT_DEATH_IF_SUPPORTED(ParseRfc3339("2018-05-18T14:42:03+08:1"),
                            "exceptions are disabled");
#endif  // GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
}

TEST(ParseRfc3339Test, DetectOutOfRangeOffsetMinute) {
#if GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
  EXPECT_THROW(ParseRfc3339("2018-05-18T14:42:03+08:60"),
               std::invalid_argument);
#else
  EXPECT_DEATH_IF_SUPPORTED(ParseRfc3339("2018-05-18T14:42:03+08:60"),
                            "exceptions are disabled");
#endif  // GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
}

}  // namespace
}  // namespace internal
}  // namespace GOOGLE_CLOUD_CPP_NS
}  // namespace cloud
}  // namespace google
