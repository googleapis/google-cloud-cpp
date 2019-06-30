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
#include "google/cloud/internal/time_format.h"
#include <cctype>
#include <cstdio>
#include <iomanip>
#include <iostream>
#include <sstream>

namespace {
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
}  // namespace

namespace google {
namespace cloud {
inline namespace GOOGLE_CLOUD_CPP_NS {
namespace internal {

std::string FormatRfc3339(std::chrono::system_clock::time_point tp) {
  return TimestampToString(tp);
}

std::string FormatV4SignedUrlTimestamp(
    std::chrono::system_clock::time_point tp) {
  std::tm tm = AsUtcTm(tp);
  char buffer[256];
  std::strftime(buffer, sizeof(buffer), "%Y%m%dT%H%M%SZ", &tm);
  return buffer;
}

std::string FormatV4SignedUrlScope(std::chrono::system_clock::time_point tp) {
  std::tm tm = AsUtcTm(tp);
  char buffer[256];
  std::strftime(buffer, sizeof(buffer), "%Y%m%d", &tm);
  return buffer;
}

}  // namespace internal
}  // namespace GOOGLE_CLOUD_CPP_NS
}  // namespace cloud
}  // namespace google
