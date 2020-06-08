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

#include "google/cloud/internal/format_time_point.h"
#include <array>
#include <cstdio>
#include <ctime>

namespace {
std::string FormatFractional(std::chrono::nanoseconds ns) {
  if (ns.count() == 0) {
    return "";
  }

  using std::chrono::milliseconds;
  using std::chrono::nanoseconds;
  using std::chrono::seconds;
  auto constexpr kMaxNanosecondsDigits = 9;
  auto constexpr kBufferSize = 16;
  static_assert(kBufferSize > (kMaxNanosecondsDigits  // digits
                               + 1                    // period
                               + 1),                  // NUL terminator
                "Buffer is not large enough for printing nanoseconds");
  auto constexpr kNanosecondsPerMillisecond =
      nanoseconds(milliseconds(1)).count();
  auto constexpr kMillisecondsPerSecond = milliseconds(seconds(1)).count();

  std::array<char, kBufferSize> buffer{};
  // If the fractional seconds can be just expressed as milliseconds, do that,
  // we do not want to print 1.123000000
  auto d = std::lldiv(ns.count(), kNanosecondsPerMillisecond);
  if (d.rem == 0) {
    std::snprintf(buffer.data(), buffer.size(), ".%03lld", d.quot);
    return buffer.data();
  }
  d = std::lldiv(ns.count(), kMillisecondsPerSecond);
  if (d.rem == 0) {
    std::snprintf(buffer.data(), buffer.size(), ".%06lld", d.quot);
    return buffer.data();
  }

  std::snprintf(buffer.data(), buffer.size(), ".%09lld",
                static_cast<long long>(ns.count()));
  return buffer.data();
}

}  // namespace

namespace google {
namespace cloud {
inline namespace GOOGLE_CLOUD_CPP_NS {
namespace internal {

std::tm AsUtcTm(std::chrono::system_clock::time_point tp) {
  std::time_t time = std::chrono::system_clock::to_time_t(tp);
  std::tm tm{};
  // The standard C++ function to convert time_t to a struct tm is not thread
  // safe (it holds global storage), use some OS specific stuff here:
#if _WIN32
  gmtime_s(&tm, &time);
#else
  gmtime_r(&time, &tm);
#endif  // _WIN32
  return tm;
}

auto constexpr kTimestampFormatSize = 256;
static_assert(kTimestampFormatSize > ((4 + 1)    // YYYY-
                                      + (2 + 1)  // MM-
                                      + 2        // DD
                                      + 1        // T
                                      + (2 + 1)  // HH:
                                      + (2 + 1)  // MM:
                                      + 2        // SS
                                      + 1),      // Z
              "Buffer size not large enough for YYYY-MM-DDTHH:MM:SSZ format");

std::string FormatRfc3339(std::chrono::system_clock::time_point tp) {
  std::tm tm = AsUtcTm(tp);
  std::array<char, kTimestampFormatSize> buffer{};
  std::strftime(buffer.data(), buffer.size(), "%Y-%m-%dT%H:%M:%S", &tm);

  std::string result(buffer.data());
  // Add the fractional seconds...
  auto duration = tp.time_since_epoch();
  using std::chrono::duration_cast;
  auto fractional = duration_cast<std::chrono::nanoseconds>(
      duration - duration_cast<std::chrono::seconds>(duration));
  result += FormatFractional(fractional);
  result += "Z";
  return result;
}

std::string FormatV4SignedUrlTimestamp(
    std::chrono::system_clock::time_point tp) {
  std::tm tm = AsUtcTm(tp);
  std::array<char, kTimestampFormatSize> buffer{};
  std::strftime(buffer.data(), buffer.size(), "%Y%m%dT%H%M%SZ", &tm);
  return buffer.data();
}

std::string FormatV4SignedUrlScope(std::chrono::system_clock::time_point tp) {
  std::tm tm = AsUtcTm(tp);

  auto constexpr kDateFormatSize = 256;
  static_assert(kDateFormatSize > (4      // YYYY
                                   + 2    // MM
                                   + 2),  // DD
                "Buffer size not large enough for YYYYMMDD format");
  std::array<char, kTimestampFormatSize> buffer{};
  std::strftime(buffer.data(), buffer.size(), "%Y%m%d", &tm);
  return buffer.data();
}

}  // namespace internal
}  // namespace GOOGLE_CLOUD_CPP_NS
}  // namespace cloud
}  // namespace google
