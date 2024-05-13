// Copyright 2019 Google LLC
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

#include "google/cloud/spanner/instance.h"
#include "google/cloud/internal/make_status.h"
#include <ostream>
#include <regex>

namespace google {
namespace cloud {
namespace spanner {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

Instance::Instance(Project project, std::string instance_id)
    : project_(std::move(project)), instance_id_(std::move(instance_id)) {}

Instance::Instance(std::string project_id, std::string instance_id)
    : Instance(Project(std::move(project_id)), std::move(instance_id)) {}

std::string Instance::FullName() const {
  return project_.FullName() + "/instances/" + instance_id_;
}

bool operator==(Instance const& a, Instance const& b) {
  return a.project_ == b.project_ && a.instance_id_ == b.instance_id_;
}

bool operator!=(Instance const& a, Instance const& b) { return !(a == b); }

std::ostream& operator<<(std::ostream& os, Instance const& in) {
  return os << in.FullName();
}

StatusOr<Instance> MakeInstance(std::string const& full_name) {
  std::regex re("projects/([^/]+)/instances/([^/]+)");
  std::smatch matches;
  if (!std::regex_match(full_name, matches, re)) {
    return internal::InvalidArgumentError(
        "Improperly formatted Instance: " + full_name, GCP_ERROR_INFO());
  }
  return Instance(Project(std::move(matches[1])), std::move(matches[2]));
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace spanner
}  // namespace cloud
}  // namespace google
