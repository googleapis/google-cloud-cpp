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

#include "google/cloud/bigtable/instance_resource.h"
#include <ostream>
#include <regex>

namespace google {
namespace cloud {
namespace bigtable {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

InstanceResource::InstanceResource(Project project, std::string instance_id)
    : project_(std::move(project)), instance_id_(std::move(instance_id)) {}

std::string InstanceResource::FullName() const {
  return project_.FullName() + "/instances/" + instance_id_;
}

bool operator==(InstanceResource const& a, InstanceResource const& b) {
  return a.project_ == b.project_ && a.instance_id_ == b.instance_id_;
}

bool operator!=(InstanceResource const& a, InstanceResource const& b) {
  return !(a == b);
}

std::ostream& operator<<(std::ostream& os, InstanceResource const& in) {
  return os << in.FullName();
}

StatusOr<InstanceResource> MakeInstanceResource(std::string const& full_name) {
  std::regex re("projects/([^/]+)/instances/([^/]+)");
  std::smatch matches;
  if (!std::regex_match(full_name, matches, re)) {
    return Status(StatusCode::kInvalidArgument,
                  "Improperly formatted InstanceResource: " + full_name);
  }
  return InstanceResource(Project(std::move(matches[1])),
                          std::move(matches[2]));
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable
}  // namespace cloud
}  // namespace google
