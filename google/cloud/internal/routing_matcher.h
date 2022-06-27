// Copyright 2022 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_ROUTING_MATCHER_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_ROUTING_MATCHER_H

#include "google/cloud/version.h"
#include "absl/types/optional.h"
#include <functional>
#include <regex>
#include <string>
#include <vector>

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {

/**
 * A helper class used by our `MetadataDecorator`s to match and extract routing
 * keys from a proto.
 */
template <typename Request>
struct RoutingMatcher {
  // Includes an equals sign. e.g. "key="
  std::string routing_key;

  struct Pattern {
    std::function<std::string const&(Request const&)> field_getter;
    absl::optional<std::regex> re;
  };
  std::vector<Pattern> patterns;

  // If a match is found for this routing_key, append "routing_key=value" to
  // the `params` vector.
  void AppendParam(Request const& request, std::vector<std::string>& params) {
    std::smatch match;
    for (auto const& pattern : patterns) {
      auto const& field = pattern.field_getter(request);
      if (field.empty()) continue;
      // When the optional regex is not engaged, it is implied that we should
      // match the whole field.
      if (!pattern.re) {
        params.push_back(routing_key + field);
        return;
      }
      if (std::regex_match(field, match, *pattern.re)) {
        params.push_back(routing_key + match[1].str());
        return;
      }
    }
  }
};

}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_ROUTING_MATCHER_H
