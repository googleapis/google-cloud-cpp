// Copyright 2018 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_ACCESS_CONTROL_COMMON_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_ACCESS_CONTROL_COMMON_H

#include "google/cloud/storage/internal/common_metadata.h"
#include "google/cloud/storage/version.h"
#include "google/cloud/status.h"
#include "absl/types/optional.h"
#include <string>
#include <utility>

namespace google {
namespace cloud {
namespace storage {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {
struct AccessControlCommonParser;
}  // namespace internal

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

namespace internal {
class GrpcClient;

/**
 * Defines common code to both `BucketAccessControl` and `ObjectAccessControl`.
 *
 * @see
 * https://cloud.google.com/storage/docs/json_api/v1/bucketAccessControls
 * https://cloud.google.com/storage/docs/json_api/v1/objectAccessControls
 */
class AccessControlCommon {
 public:
  AccessControlCommon() = default;

  //@{
  /**
   * @name Well-known values for the role() field..
   *
   * The following functions are handy to avoid common typos in the role names.
   * We use functions instead of enums because enums are not backwards
   * compatible and are brittle to changes in the server-side.
   */
  static std::string ROLE_OWNER() { return "OWNER"; }
  static std::string ROLE_READER() { return "READER"; }
  //@}

  //@{
  /**
   * @name Well-known values for the project_team().team field..
   *
   * The following functions are handy to avoid common typos in the team names.
   * We use functions instead of enums because enums are not backwards
   * compatible and are brittle to changes in the server-side.
   */
  static std::string TEAM_EDITORS() { return "editors"; }
  static std::string TEAM_OWNERS() { return "owners"; }
  static std::string TEAM_VIEWERS() { return "viewers"; }
  //@}

  std::string const& bucket() const { return bucket_; }
  std::string const& domain() const { return domain_; }
  std::string const& email() const { return email_; }

  std::string const& entity() const { return entity_; }
  void set_entity(std::string e) { entity_ = std::move(e); }

  std::string const& entity_id() const { return entity_id_; }
  std::string const& etag() const { return etag_; }
  std::string const& id() const { return id_; }
  std::string const& kind() const { return kind_; }

  bool has_project_team() const { return project_team_.has_value(); }
  ProjectTeam const& project_team() const { return *project_team_; }
  absl::optional<ProjectTeam> const& project_team_as_optional() const {
    return project_team_;
  }

  std::string const& role() const { return role_; }
  void set_role(std::string r) { role_ = std::move(r); }

  std::string const& self_link() const { return self_link_; }

 private:
  friend class GrpcClient;
  friend struct internal::AccessControlCommonParser;

  std::string bucket_;
  std::string domain_;
  std::string email_;
  std::string entity_;
  std::string entity_id_;
  std::string etag_;
  std::string id_;
  std::string kind_;
  absl::optional<ProjectTeam> project_team_;
  std::string role_;
  std::string self_link_;
};

inline bool operator==(AccessControlCommon const& lhs,
                       AccessControlCommon const& rhs) {
  // Start with id, bucket, etag because they should fail early, then
  // alphabetical for readability.
  return lhs.id() == rhs.id() && lhs.bucket() == rhs.bucket() &&
         lhs.etag() == rhs.etag() && lhs.domain() == rhs.domain() &&
         lhs.email() == rhs.email() && lhs.entity() == rhs.entity() &&
         lhs.entity_id() == rhs.entity_id() && lhs.kind() == rhs.kind() &&
         lhs.has_project_team() == rhs.has_project_team() &&
         (!lhs.has_project_team() ||
          lhs.project_team() == rhs.project_team()) &&
         lhs.role() == rhs.role() && lhs.self_link() == rhs.self_link();
}

inline bool operator!=(AccessControlCommon const& lhs,
                       AccessControlCommon const& rhs) {
  return std::rel_ops::operator!=(lhs, rhs);
}

}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_ACCESS_CONTROL_COMMON_H
