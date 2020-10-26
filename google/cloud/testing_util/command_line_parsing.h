// Copyright 2020 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_TESTING_UTIL_COMMAND_LINE_PARSING_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_TESTING_UTIL_COMMAND_LINE_PARSING_H

#include "google/cloud/version.h"
#include "absl/types/optional.h"
#include <chrono>
#include <cinttypes>
#include <string>
#include <vector>

namespace google {
namespace cloud {
inline namespace GOOGLE_CLOUD_CPP_NS {
namespace testing_util {

std::int64_t constexpr kKiB = 1024;
std::int64_t constexpr kMiB = 1024 * kKiB;
std::int64_t constexpr kGiB = 1024 * kMiB;
std::int64_t constexpr kTiB = 1024 * kGiB;

std::int64_t constexpr kKB = 1000;
std::int64_t constexpr kMB = 1000 * kKB;
std::int64_t constexpr kGB = 1000 * kMB;
std::int64_t constexpr kTB = 1000 * kGB;

/// Parse a string as a byte size, with support for unit suffixes.
std::int64_t ParseSize(std::string const& val);

/**
 * Parse a string as a byte size, with support for unit suffixes.
 *
 * The size must be small enough for an in-memory buffer.
 */
std::size_t ParseBufferSize(std::string const& val);

/// Parse a string as a duration with support for hours (h), minutes (m), or
/// second (s) suffixes.
std::chrono::seconds ParseDuration(std::string const& val);

/// Parse a string as a boolean, returning a not-present value if the string is
/// empty.
absl::optional<bool> ParseBoolean(std::string const& val);

/// Defines a command-line option.
struct OptionDescriptor {
  using OptionParser = std::function<void(std::string const&)>;

  std::string option;
  std::string help;
  OptionParser parser;
};

/// Build the `Usage` string from a list of command-line option descriptions.
std::string BuildUsage(std::vector<OptionDescriptor> const& desc,
                       std::string const& command_path);

/**
 * Parse @p argv using the descriptions in @p desc, return unparsed arguments.
 */
std::vector<std::string> OptionsParse(std::vector<OptionDescriptor> const& desc,
                                      std::vector<std::string> argv);

/// Format a buffer size in human readable form.
std::string FormatSize(std::uintmax_t size);

}  // namespace testing_util
}  // namespace GOOGLE_CLOUD_CPP_NS
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_TESTING_UTIL_COMMAND_LINE_PARSING_H
