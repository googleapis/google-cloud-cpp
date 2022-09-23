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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_PROJECT_TEAM_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_PROJECT_TEAM_H

#include "google/cloud/storage/version.h"
#include <string>
#include <tuple>
#include <utility>

namespace google {
namespace cloud {
namespace storage {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

/**
 * Represents the projectTeam field in *AccessControls.
 *
 * @see
 * https://cloud.google.com/storage/docs/json_api/v1/bucketAccessControls
 * https://cloud.google.com/storage/docs/json_api/v1/objectAccessControls
 */
struct ProjectTeam {
  std::string project_number;
  std::string team;
};

inline bool operator==(ProjectTeam const& lhs, ProjectTeam const& rhs) {
  return std::tie(lhs.project_number, lhs.team) ==
         std::tie(rhs.project_number, rhs.team);
}

inline bool operator<(ProjectTeam const& lhs, ProjectTeam const& rhs) {
  return std::tie(lhs.project_number, lhs.team) <
         std::tie(rhs.project_number, rhs.team);
}

inline bool operator!=(ProjectTeam const& lhs, ProjectTeam const& rhs) {
  return std::rel_ops::operator!=(lhs, rhs);
}

inline bool operator>(ProjectTeam const& lhs, ProjectTeam const& rhs) {
  return std::rel_ops::operator>(lhs, rhs);
}

inline bool operator<=(ProjectTeam const& lhs, ProjectTeam const& rhs) {
  return std::rel_ops::operator<=(lhs, rhs);
}

inline bool operator>=(ProjectTeam const& lhs, ProjectTeam const& rhs) {
  return std::rel_ops::operator>=(lhs, rhs);
}

///@{
/**
 * @name Well-known values for the project_team().team field..
 *
 * The following functions are handy to avoid common typos in the team names.
 * We use functions instead of enums because enums are not backwards
 * compatible and are brittle to changes in the server-side.
 */
inline std::string TEAM_EDITORS() { return "editors"; }
inline std::string TEAM_OWNERS() { return "owners"; }
inline std::string TEAM_VIEWERS() { return "viewers"; }
///@}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_PROJECT_TEAM_H
