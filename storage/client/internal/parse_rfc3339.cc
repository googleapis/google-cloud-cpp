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

#include "storage/client/internal/parse_rfc3339.h"
#include "google/cloud/internal/throw_delegate.h"
#include <cstdio>
#include <iomanip>
#include <iostream>
#include <sstream>

namespace {
[[noreturn]] void ReportError(std::string const& timestamp, char const* msg) {
  std::ostringstream os;
  os << "Error parsing RFC 3339 timestamp: " << msg
     << " Valid format is YYYY-MM-DD[Tt]HH:MM:SS[.s+](Z|+HH:MM), got="
     << timestamp;
  google::cloud::internal::RaiseInvalidArgument(os.str());
}

std::chrono::system_clock::time_point ParseDateTime(
    char const*& buffer, std::string const& timestamp) {
  // Use std::mktime to compute the number of seconds because RFC 3339 requires
  // working with civil time, including the annoying leap seconds, and mktime
  // does.
  int year, mon, mday;
  char date_time_separator;
  int hh, mm, ss;

  int pos;
  auto count = std::sscanf(buffer, "%4d-%2d-%2d%c%2d:%2d:%2d%n", &year, &mon,
                           &mday, &date_time_separator, &hh, &mm, &ss, &pos);
  // All the fields up to this point have fixed width, so total width must be:
  constexpr int EXPECTED_WIDTH = 19;
  constexpr int EXPECTED_FIELDS = 7;
  if (count != EXPECTED_FIELDS or pos != EXPECTED_WIDTH) {
    ReportError(timestamp,
                "Invalid format for RFC 3339 timestamp detected while parsing"
                " the base date and time portion.");
  }
  if (date_time_separator != 'T' and date_time_separator != 't') {
    ReportError(timestamp, "Invalid date-time separator, expected 'T' or 't'.");
  }
  if (mon <= 0 or mon > 12) {
    ReportError(timestamp, "Out of range month.");
  }
  if (mday <= 0 or mday > 31) {
    ReportError(timestamp, "Out of range month day.");
  }
  if (hh < 0 or hh > 23) {
    ReportError(timestamp, "Out of range hour.");
  }
  if (mm < 0 or mm > 59) {
    ReportError(timestamp, "Out of range minute.");
  }
  if (ss < 0 or ss > 60) {
    ReportError(timestamp, "Out of range second.");
  }
  // Advance the pointer for all the characters read.
  buffer += pos;

  std::tm tm{0};
  tm.tm_year = year - 1900;
  tm.tm_mon = mon - 1;
  tm.tm_mday = mday;
  tm.tm_hour = hh;
  tm.tm_min = mm;
  tm.tm_sec = ss;
  return std::chrono::system_clock::from_time_t(std::mktime(&tm));
}

std::chrono::nanoseconds ParseFractionalSeconds(char const*& buffer,
                                                std::string const& timestamp) {
  if (buffer[0] != '.') {
    return std::chrono::nanoseconds(0);
  }
  ++buffer;

  long fractional_seconds;
  int pos;
  auto count = std::sscanf(buffer, "%9ld%n", &fractional_seconds, &pos);
  if (count != 1) {
    ReportError(timestamp, "Invalid fractional seconds component.");
  }
  // Normalize the fractional seconds to nanoseconds.
  for (int digits = pos; digits < 9; ++digits) {
    fractional_seconds *= 10;
  }
  // Now skip any other digits ...
  buffer += pos;
  while (std::isdigit(buffer[0])) {
    ++buffer;
  }
  return std::chrono::nanoseconds(fractional_seconds);
}

std::chrono::seconds ParseOffset(char const*& buffer,
                                 std::string const& timestamp) {
  if (buffer[0] == '+' or buffer[0] == '-') {
    bool positive = buffer[0] == '+';
    ++buffer;
    // Parse the HH:MM offset.
    int hh, mm, pos;
    auto count = std::sscanf(buffer, "%2d:%2d%n", &hh, &mm, &pos);
    constexpr int EXPECTED_OFFSET_WIDTH = 5;
    constexpr int EXPECTED_OFFSET_FIELDS = 2;
    if (count != EXPECTED_OFFSET_FIELDS or pos != EXPECTED_OFFSET_WIDTH) {
      ReportError(timestamp, "Invalid timezone offset, expected [+/-]HH:SS.");
    }
    if (hh < 0 or hh > 23) {
      ReportError(timestamp, "Out of range offset hour.");
    }
    if (mm < 0 or mm > 59) {
      ReportError(timestamp, "Out of range offset minute.");
    }
    buffer += pos;
    using namespace std::chrono;
    if (positive) {
      return duration_cast<seconds>(hours(hh) + minutes(mm));
    }
    return duration_cast<seconds>(-hours(hh) - minutes(mm));
  }
  if (buffer[0] != 'Z' and buffer[0] != 'z') {
    ReportError(timestamp, "Invalid timezone offset, expected 'Z' or 'z'.");
  }
  ++buffer;
  return std::chrono::seconds(0);
}
}  // anonymous namespace

namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace internal {
std::chrono::system_clock::time_point ParseRfc3339(
    std::string const& timestamp) {
  // TODO(#530) - dynamically change the timezone offset.
  // Because this computation is a bit expensive, assume the timezone offset
  // does not change during the lifetime of the program.  This function takes
  // the current time, converts to UTC and then convert back to a time_t.  The
  // difference is the offset.
  static std::chrono::seconds const LOCALTIME_OFFSET = []() {
    auto now = std::time(nullptr);
    std::tm lcl;
// The standard C++ function to convert time_t to a struct tm is not thread
// safe (it holds global storage), use some OS specific stuff here:
#if WIN32
    gmtime_s(&now, &lcl);
#else
    gmtime_r(&now, &lcl);
#endif  // WIN32
    return std::chrono::seconds(mktime(&lcl) - now);
  }();

  char const* buffer = timestamp.c_str();
  auto time_point = ParseDateTime(buffer, timestamp);
  std::chrono::nanoseconds nsec = ParseFractionalSeconds(buffer, timestamp);
  std::chrono::seconds offset = ParseOffset(buffer, timestamp);

  if (buffer[0] != '\0') {
    ReportError(timestamp, "Additional text after RFC 3339 date.");
  }

  time_point += nsec;
  time_point -= offset;
  time_point -= LOCALTIME_OFFSET;
  return time_point;
}

}  // namespace internal
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
