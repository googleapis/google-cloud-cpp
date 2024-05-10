// Copyright 2023 Google LLC
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

#include "google/cloud/location.h"
#include "google/cloud/internal/make_status.h"
#include <ostream>
#include <regex>

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

Location::Location(Project project, std::string location_id)
    : project_(std::move(project)), location_id_(std::move(location_id)) {}

Location::Location(std::string project_id, std::string location_id)
    : Location(Project(std::move(project_id)), std::move(location_id)) {}

std::string Location::FullName() const {
  return project_.FullName() + "/locations/" + location_id_;
}

bool operator==(Location const& a, Location const& b) {
  return a.project_ == b.project_ && a.location_id_ == b.location_id_;
}

bool operator!=(Location const& a, Location const& b) { return !(a == b); }

std::ostream& operator<<(std::ostream& os, Location const& in) {
  return os << in.FullName();
}

StatusOr<Location> MakeLocation(std::string const& full_name) {
  std::regex re("projects/([^/]+)/locations/([^/]+)");
  std::smatch matches;
  if (!std::regex_match(full_name, matches, re)) {
    return internal::InvalidArgumentError("Improperly formatted Location: " +
                                          full_name);
  }
  return Location(Project(std::move(matches[1])), std::move(matches[2]));
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google
