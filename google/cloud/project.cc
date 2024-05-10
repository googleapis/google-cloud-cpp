// Copyright 2021 Google LLC
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

#include "google/cloud/project.h"
#include "google/cloud/internal/make_status.h"
#include <ostream>
#include <regex>

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

Project::Project(std::string project_id) : project_id_(std::move(project_id)) {}

std::string Project::FullName() const { return "projects/" + project_id_; }

bool operator==(Project const& a, Project const& b) {
  return a.project_id_ == b.project_id_;
}

bool operator!=(Project const& a, Project const& b) { return !(a == b); }

std::ostream& operator<<(std::ostream& os, Project const& p) {
  return os << p.FullName();
}

StatusOr<Project> MakeProject(std::string const& full_name) {
  std::regex re("projects/([^/]+)");
  std::smatch matches;
  if (!std::regex_match(full_name, matches, re)) {
    return internal::InvalidArgumentError("Improperly formatted Project: " +
                                          full_name);
  }
  return Project(std::move(matches[1]));
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google
