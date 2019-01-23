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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_ACCESS_CONTROL_COMMON_H_
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_ACCESS_CONTROL_COMMON_H_

#include "google/cloud/optional.h"
#include "google/cloud/status.h"
#include "google/cloud/storage/internal/common_metadata.h"
#include "google/cloud/storage/internal/nljson.h"
#include <utility>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
/**
 * Represents the projectTeam field in *AcccessControls.
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
         std::tie(lhs.project_number, lhs.team);
}

inline bool operator<(ProjectTeam const& lhs, ProjectTeam const& rhs) {
  return std::tie(lhs.project_number, lhs.team) <
         std::tie(lhs.project_number, lhs.team);
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
  google::cloud::optional<ProjectTeam> const& project_team_as_optional() const {
    return project_team_;
  }

  std::string const& role() const { return role_; }
  void set_role(std::string r) { role_ = std::move(r); }

  std::string const& self_link() const { return self_link_; }

  bool operator==(AccessControlCommon const& rhs) const {
    // Start with id, bucket, etag because they should fail early, then
    // alphabetical for readability.
    return id_ == rhs.id_ && bucket_ == rhs.bucket_ && etag_ == rhs.etag_ &&
           domain_ == rhs.domain_ && email_ == rhs.email_ &&
           entity_ == rhs.entity_ && entity_id_ == rhs.entity_id_ &&
           kind_ == rhs.kind_ && project_team_ == rhs.project_team_ &&
           role_ == rhs.role_ && self_link_ == rhs.self_link_;
  }
  bool operator!=(AccessControlCommon const& rhs) { return !(*this == rhs); }

  static Status ParseFromJson(AccessControlCommon& result,
                              nl::json const& json);

 private:
  std::string bucket_;
  std::string domain_;
  std::string email_;
  std::string entity_;
  std::string entity_id_;
  std::string etag_;
  std::string id_;
  std::string kind_;
  google::cloud::optional<ProjectTeam> project_team_;
  std::string role_;
  std::string self_link_;
};

}  // namespace internal
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_ACCESS_CONTROL_COMMON_H_
