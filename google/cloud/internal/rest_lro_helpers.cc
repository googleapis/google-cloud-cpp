// Copyright 2024 Google LLC
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

#include "google/cloud/internal/rest_lro_helpers.h"
#include "absl/strings/str_split.h"

namespace google {
namespace cloud {
namespace rest_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

// TODO(#14337): Check for errors and return StatusOr here once call sites
// are capable of propagating errors.
ComputeOperationInfo ParseComputeOperationInfo(std::string const& self_link) {
  ComputeOperationInfo info;
  std::vector<std::string> self_link_parts = absl::StrSplit(self_link, '/');
  for (auto i = self_link_parts.begin(); i != self_link_parts.end(); ++i) {
    if (*i == "projects" && std::next(i) != self_link_parts.end()) {
      info.project = *(++i);
    } else if (*i == "regions" && std::next(i) != self_link_parts.end()) {
      info.region = *(++i);
    } else if (*i == "zones" && std::next(i) != self_link_parts.end()) {
      info.zone = *(++i);
    } else if (*i == "operations" && std::next(i) != self_link_parts.end()) {
      info.operation = *(++i);
    }
  }
  return info;
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace rest_internal
}  // namespace cloud
}  // namespace google
