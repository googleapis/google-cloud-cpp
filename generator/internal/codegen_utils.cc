// Copyright 2020 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "generator/internal/codegen_utils.h"
// TODO(#4501) - fix by doing #include <absl/...>
#if _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4244)
#endif  // _MSC_VER
#include "absl/strings/str_cat.h"
#include "absl/strings/str_join.h"
#include "absl/strings/str_replace.h"
#include "absl/strings/str_split.h"
#if _MSC_VER
#pragma warning(pop)
#endif  // _MSC_VER
// TODO(#4501) - end
#include <cctype>
#include <string>

namespace google {
namespace cloud {
namespace generator_internal {

std::string GeneratedFileSuffix() { return ".gcpcxx.pb"; }

std::string LocalInclude(absl::string_view header) {
  return absl::StrCat("\"", header, "\"");
}

std::string SystemInclude(absl::string_view header) {
  return absl::StrCat("<", header, ">");
}

std::string CamelCaseToSnakeCase(absl::string_view input) {
  std::string output;
  for (auto i = 0U; i < input.size(); ++i) {
    if (input[i] != '_' && i + 2 < input.size()) {
      if (std::isupper(static_cast<unsigned char>(input[i + 1])) &&
          std::islower(static_cast<unsigned char>(input[i + 2]))) {
        absl::StrAppend(
            &output,
            std::string(1, std::tolower(static_cast<unsigned char>(input[i]))),
            "_");
        continue;
      }
    }
    if (input[i] != '_' && i + 1 < input.size()) {
      if ((std::islower(static_cast<unsigned char>(input[i])) ||
           std::isdigit(static_cast<unsigned char>(input[i]))) &&
          std::isupper(static_cast<unsigned char>(input[i + 1]))) {
        absl::StrAppend(
            &output,
            std::string(1, std::tolower(static_cast<unsigned char>(input[i]))),
            "_");
        continue;
      }
    }
    absl::StrAppend(
        &output,
        std::string(1, std::tolower(static_cast<unsigned char>(input[i]))));
  }
  return output;
}

std::string ServiceNameToFilePath(absl::string_view service_name) {
  std::vector<absl::string_view> components = absl::StrSplit(service_name, ".");
  absl::ConsumeSuffix(&components.back(), "Service");
  auto formatter = [](std::string* s, absl::string_view sv) {
    *s += CamelCaseToSnakeCase(sv);
  };
  return absl::StrJoin(components, "/", formatter);
}

std::string ProtoNameToCppName(absl::string_view proto_name) {
  return "::" + absl::StrReplaceAll(proto_name, {{".", "::"}});
}

}  // namespace generator_internal
}  // namespace cloud
}  // namespace google
