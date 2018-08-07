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

#include "google/cloud/storage/internal/format_rfc3339.h"
#include "google/cloud/internal/throw_delegate.h"
#include <cctype>
#include <cstdio>
#include <iomanip>
#include <iostream>
#include <sstream>

namespace {
std::string FormatFractional(std::chrono::nanoseconds ns) {
  if (ns.count() == 0) {
    return "";
  }

  char buffer[16];
  std::string result = ".";
  // If the fractional seconds can be just expressed as milliseconds, do that,
  // we do not want to print 1.123000000
  auto d = std::lldiv(ns.count(), 1000000LL);
  if (d.rem == 0) {
    std::snprintf(buffer, sizeof(buffer), "%03lld", d.quot);
    result += buffer;
    return result;
  }
  d = std::lldiv(ns.count(), 1000LL);
  if (d.rem == 0) {
    std::snprintf(buffer, sizeof(buffer), "%06lld", d.quot);
    result += buffer;
    return result;
  }

  std::snprintf(buffer, sizeof(buffer), "%09lld",
                static_cast<long long>(ns.count()));
  result += buffer;
  return result;
}
}  // namespace

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace internal {

std::string FormatRfc3339(std::chrono::system_clock::time_point tp) {
  std::time_t time = std::chrono::system_clock::to_time_t(tp);
  std::tm tm;
  // The standard C++ function to convert time_t to a struct tm is not thread
  // safe (it holds global storage), use some OS specific stuff here:
#if _WIN32
  gmtime_s(&tm, &time);
#else
  gmtime_r(&time, &tm);
#endif  // _WIN32
  char buffer[256];
  std::strftime(buffer, sizeof(buffer), "%Y-%m-%dT%T", &tm);

  std::string result(buffer);
  // Add the fractional seconds...
  auto duration = tp.time_since_epoch();
  using std::chrono::duration_cast;
  auto fractional = duration_cast<std::chrono::nanoseconds>(
      duration - duration_cast<std::chrono::seconds>(duration));
  result += FormatFractional(fractional);
  result += "Z";
  return result;
}
}  // namespace internal
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google
