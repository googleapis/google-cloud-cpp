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

ComputeOperationInfo ParseComputeOperationInfo(std::string const& self_link) {
  ComputeOperationInfo info;
  std::vector<std::string> self_link_parts = absl::StrSplit(self_link, '/');
  for (std::size_t i = 0; i != self_link_parts.size(); ++i) {
    if (self_link_parts[i] == "projects") {
      info.project = self_link_parts[++i];
    } else if (self_link_parts[i] == "regions") {
      info.region = self_link_parts[++i];
    } else if (self_link_parts[i] == "zones") {
      info.zone = self_link_parts[++i];
    } else if (self_link_parts[i] == "operations") {
      info.operation = self_link_parts[++i];
    }
  }
  return info;
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace rest_internal
}  // namespace cloud
}  // namespace google
