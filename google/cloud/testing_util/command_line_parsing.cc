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

#include "google/cloud/testing_util/command_line_parsing.h"
#include "google/cloud/internal/throw_delegate.h"
#include "absl/strings/match.h"
#include "absl/time/time.h"
#include <iomanip>
#include <sstream>

namespace google {
namespace cloud {
inline namespace GOOGLE_CLOUD_CPP_NS {
namespace testing_util {

// This parser does not validate the input fully, but it is good enough for our
// purposes.
std::int64_t ParseSize(std::string const& val) {
  struct Match {
    char const* suffix;
    std::int64_t multiplier;
  } matches[] = {
      {"TiB", kTiB}, {"GiB", kGiB}, {"MiB", kMiB}, {"KiB", kKiB},
      {"TB", kTB},   {"GB", kGB},   {"MB", kMB},   {"KB", kKB},
  };
  auto const s = std::stol(val);
  for (auto const& m : matches) {
    if (absl::EndsWith(val, m.suffix)) return s * m.multiplier;
  }
  return s;
}

std::size_t ParseBufferSize(std::string const& val) {
  auto const s = ParseSize(val);
  if (s < 0 || static_cast<std::uint64_t>(s) >
                   (std::numeric_limits<std::size_t>::max)()) {
    internal::ThrowRangeError("invalid range in ParseBufferSize");
  }
  return static_cast<std::size_t>(s);
}

std::chrono::seconds ParseDuration(std::string const& val) {
  absl::Duration d;
  if (!absl::ParseDuration(val, &d)) {
    internal::ThrowInvalidArgument("invalid duration: " + val);
  }
  return absl::ToChronoSeconds(d);
}

absl::optional<bool> ParseBoolean(std::string const& val) {
  auto lower = val;
  std::transform(lower.begin(), lower.end(), lower.begin(),
                 [](char x) { return static_cast<char>(std::tolower(x)); });
  if (lower == "true") return true;
  if (lower == "false") return false;
  return {};
}

std::string FormatSize(std::uintmax_t size) {
  struct {
    std::uintmax_t limit;
    std::uintmax_t resolution;
    char const* name;
  } ranges[] = {
      {kKiB, 1, "B"},
      {kMiB, kKiB, "KiB"},
      {kGiB, kMiB, "MiB"},
      {kTiB, kGiB, "GiB"},
  };
  std::uintmax_t resolution = kTiB;
  char const* name = "TiB";
  for (auto const& r : ranges) {
    if (size < r.limit) {
      resolution = r.resolution;
      name = r.name;
      break;
    }
  }
  std::ostringstream os;
  os.setf(std::ios::fixed);
  os.precision(1);
  os << (static_cast<double>(size) / static_cast<double>(resolution)) << name;
  return os.str();
}

std::string Basename(std::string const& path) {
  // With C++17 we would use `std::filesystem::path`, until then use
  // `find_last_of()`
#if _WIN32
  return path.substr(path.find_last_of("\\/") + 1);
#else
  return path.substr(path.find_last_of('/') + 1);
#endif  // _WIN32
}

std::string BuildUsage(std::vector<OptionDescriptor> const& desc,
                       std::string const& command_path) {
  std::ostringstream os;
  os << "Usage: " << Basename(command_path) << " [options] <region>\n";
  for (auto const& d : desc) {
    os << "    " << d.option << ": " << d.help << "\n";
  }
  return std::move(os).str();
}

std::vector<std::string> OptionsParse(std::vector<OptionDescriptor> const& desc,
                                      std::vector<std::string> argv) {
  if (argv.empty()) {
    return argv;
  }
  std::string const usage = BuildUsage(desc, argv[0]);

  auto next_arg = argv.begin() + 1;
  while (next_arg != argv.end()) {
    std::string const& argument = *next_arg;

    // Try to match `argument` against the options in `desc`
    bool matched = false;
    for (auto const& d : desc) {
      if (argument.rfind(d.option, 0) != 0) {
        // Not a match, keep searching
        continue;
      }
      std::string val = argument.substr(d.option.size());
      if (!val.empty() && val[0] != '=') {
        // Matched a prefix of an option, keep searching.
        continue;
      }
      if (!val.empty()) {
        // The first character must be '=', remove it too.
        val.erase(val.begin());
      }
      d.parser(val);
      // This is a match, consume the argument and stop the search.
      matched = true;
      break;
    }
    // If next_arg is matched against any option erase it, otherwise skip it.
    next_arg = matched ? argv.erase(next_arg) : next_arg + 1;
  }
  return argv;
}

}  // namespace testing_util
}  // namespace GOOGLE_CLOUD_CPP_NS
}  // namespace cloud
}  // namespace google
