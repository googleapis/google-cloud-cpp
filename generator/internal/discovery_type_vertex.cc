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

#include "generator/internal/discovery_type_vertex.h"
#include "google/cloud/internal/absl_str_join_quiet.h"
#include "absl/strings/str_format.h"

namespace google {
namespace cloud {
namespace generator_internal {

DiscoveryTypeVertex::DiscoveryTypeVertex() : json_("") {}

DiscoveryTypeVertex::DiscoveryTypeVertex(std::string name, nlohmann::json json)
    : name_(std::move(name)), json_(std::move(json)) {}

void DiscoveryTypeVertex::AddNeedsTypeName(std::string type_name) {
  needs_.insert(std::move(type_name));
}

void DiscoveryTypeVertex::AddNeededByTypeName(std::string type_name) {
  needed_by_.insert(std::move(type_name));
}

std::string DiscoveryTypeVertex::DebugString() const {
  return absl::StrFormat("name: %s; needs: %s; needed_by: %s", name_,
                         absl::StrJoin(needs_, ","),
                         absl::StrJoin(needed_by_, ","));
}

}  // namespace generator_internal
}  // namespace cloud
}  // namespace google
