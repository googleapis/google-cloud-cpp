// Copyright 2023 Google LLC
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

#include "generator/internal/routing.h"
#include "google/cloud/internal/absl_str_join_quiet.h"
#include "google/cloud/internal/absl_str_replace_quiet.h"
#include "google/cloud/log.h"
#include "absl/strings/str_split.h"
#include "google/api/routing.pb.h"
#include "google/protobuf/compiler/cpp/names.h"
#include <regex>
using ::google::protobuf::compiler::cpp::FieldName;

namespace google {
namespace cloud {
namespace generator_internal {

ExplicitRoutingInfo ParseExplicitRoutingHeader(
    google::protobuf::MethodDescriptor const& method) {
  ExplicitRoutingInfo info;
  if (!method.options().HasExtension(google::api::routing)) return info;
  google::api::RoutingRule rule =
      method.options().GetExtension(google::api::routing);

  auto const& rps = rule.routing_parameters();
  // We use reverse iterators so that "last wins" becomes "first wins".
  for (auto it = rps.rbegin(); it != rps.rend(); ++it) {
    std::vector<std::string> chunks;
    auto const* input_type = method.input_type();
    for (auto const& sv : absl::StrSplit(it->field(), '.')) {
      auto const chunk = std::string(sv);
      auto const* chunk_descriptor = input_type->FindFieldByName(chunk);
      chunks.push_back(FieldName(chunk_descriptor));
      input_type = chunk_descriptor->message_type();
    }
    auto field_name = absl::StrJoin(chunks, "().");
    auto const& path_template = it->path_template();
    // When a path_template is not supplied, we use the field name as the
    // routing parameter key. The pattern matches the whole value of the field.
    if (path_template.empty()) {
      info[it->field()].push_back({std::move(field_name), "(.*)"});
      continue;
    }
    // When a path_template is supplied, we convert the pattern from the proto
    // into a std::regex. We extract the routing parameter key and set up a
    // single capture group. For example:
    //
    // Input :
    //   - path_template = "projects/*/{foo=instances/*}:**"
    // Output:
    //   - param         = "foo"
    //   - pattern       = "projects/[^/]+/(instances/[^:]+):.*"
    static std::regex const kPatternRegex(R"((.*)\{(.*)=(.*)\}(.*))");
    std::smatch match;
    if (!std::regex_match(path_template, match, kPatternRegex)) {
      GCP_LOG(FATAL) << __FILE__ << ":" << __LINE__ << ": "
                     << "RoutingParameters path template is malformed: "
                     << path_template;
    }
    auto pattern =
        absl::StrCat(match[1].str(), "(", match[3].str(), ")", match[4].str());
    pattern = absl::StrReplaceAll(
        pattern,
        {{"**", ".*"}, {"*):", "[^:]+):"}, {"*:", "[^:]+:"}, {"*", "[^/]+"}});
    info[match[2].str()].push_back({std::move(field_name), std::move(pattern)});
  }
  return info;
}

}  // namespace generator_internal
}  // namespace cloud
}  // namespace google
