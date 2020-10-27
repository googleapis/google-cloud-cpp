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

#include "google/cloud/tracing_options.h"
#include "absl/types/optional.h"
#include <algorithm>
#include <cstdint>
#include <string>

namespace google {
namespace cloud {
inline namespace GOOGLE_CLOUD_CPP_NS {
namespace {

absl::optional<bool> ParseBoolean(std::string const& str) {
  for (auto const* t : {"Y", "y", "T", "t", "1", "on"}) {
    if (str == t) return true;
  }
  for (auto const* f : {"N", "n", "F", "f", "0", "off"}) {
    if (str == f) return false;
  }
  return {};
}

absl::optional<std::int64_t> ParseInteger(std::string const& str) {
  std::size_t econv = -1;
  auto val = std::stoll(str, &econv);
  if (econv != str.size()) return {};
  return val;
}

}  // namespace

TracingOptions& TracingOptions::SetOptions(std::string const& str) {
  auto const beg = str.begin();
  auto const end = str.end();
  for (auto pos = beg;;) {
    auto const comma = std::find(pos, end, ',');
    auto const equal = std::find(pos, comma, '=');
    std::string const opt{pos, equal};
    std::string const val{equal + (equal == comma ? 0 : 1), comma};
    if (opt == "single_line_mode") {
      if (auto v = ParseBoolean(val)) single_line_mode_ = *v;
    } else if (opt == "use_short_repeated_primitives") {
      if (auto v = ParseBoolean(val)) use_short_repeated_primitives_ = *v;
    } else if (opt == "truncate_string_field_longer_than") {
      if (auto v = ParseInteger(val)) truncate_string_field_longer_than_ = *v;
    }
    if (comma == end) break;
    pos = comma + 1;
  }
  return *this;
}

}  // namespace GOOGLE_CLOUD_CPP_NS
}  // namespace cloud
}  // namespace google
