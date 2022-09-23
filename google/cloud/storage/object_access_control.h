// Copyright 2018 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_OBJECT_ACCESS_CONTROL_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_OBJECT_ACCESS_CONTROL_H

#include "google/cloud/storage/internal/patch_builder.h"
#include "google/cloud/storage/project_team.h"
#include "google/cloud/storage/version.h"
#include "google/cloud/status_or.h"
#include <string>

namespace google {
namespace cloud {
namespace storage {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {
struct ObjectAccessControlParser;
struct GrpcObjectAccessControlParser;
}  // namespace internal

/**
 * Wraps the objectAccessControl resource in Google Cloud Storage.
 *
 * objectAccessControl describes the access to a bucket for a single entity,
 * where the entity might be a user, group, or other role.
 *
 * @see
 * https://cloud.google.com/storage/docs/json_api/v1/objectAccessControls for
 *     an authoritative source of field definitions.
 */
class ObjectAccessControl {
 public:
  ObjectAccessControl() = default;

  ///@{
  /**
   * @name Well-known values for the role() field..
   *
   * The following functions are handy to avoid common typos in the role names.
   * We use functions instead of enums because enums are not backwards
   * compatible and are brittle to changes in the server-side.
   */
  static std::string ROLE_OWNER() { return "OWNER"; }
  static std::string ROLE_READER() { return "READER"; }
  ///@}

  ///@{
  /**
   * @name Well-known values for the project_team().team field..
   *
   * The following functions are handy to avoid common typos in the team names.
   * We use functions instead of enums because enums are not backwards
   * compatible and are brittle to changes in the server-side.
   */
  static std::string TEAM_EDITORS() { return storage::TEAM_EDITORS(); }
  static std::string TEAM_OWNERS() { return storage::TEAM_OWNERS(); }
  static std::string TEAM_VIEWERS() { return storage::TEAM_VIEWERS(); }
  ///@}

  ///@{
  /**
   * @name Accessors.
   */
  std::string const& bucket() const { return bucket_; }
  std::string const& object() const { return object_; }
  std::int64_t generation() const { return generation_; }
  std::string const& domain() const { return domain_; }
  std::string const& email() const { return email_; }
  std::string const& entity() const { return entity_; }
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
  std::string const& self_link() const { return self_link_; }
  ///@}

  ///@{
  /**
   * @name Modifiers for mutable attributes.
   *
   * The follow attributes can be changed in update and patch operations.
   */
  ObjectAccessControl& set_entity(std::string v) {
    entity_ = std::move(v);
    return *this;
  }
  ObjectAccessControl& set_role(std::string v) {
    role_ = std::move(v);
    return *this;
  }
  ///@}

  ///@{
  /**
   * @name Testing modifiers.
   *
   * The following attributes cannot be changed when updating, creating, or
   * patching an ObjectAccessControl resource. However, it is useful to change
   * them in tests, e.g., when mocking the results from the C++ client library.
   */
  ObjectAccessControl& set_bucket(std::string v) {
    bucket_ = std::move(v);
    return *this;
  }
  ObjectAccessControl& set_object(std::string v) {
    object_ = std::move(v);
    return *this;
  }
  ObjectAccessControl& set_generation(std::int64_t v) {
    generation_ = v;
    return *this;
  }
  ObjectAccessControl& set_domain(std::string v) {
    domain_ = std::move(v);
    return *this;
  }
  ObjectAccessControl& set_email(std::string v) {
    email_ = std::move(v);
    return *this;
  }
  ObjectAccessControl& set_entity_id(std::string v) {
    entity_id_ = std::move(v);
    return *this;
  }
  ObjectAccessControl& set_etag(std::string v) {
    etag_ = std::move(v);
    return *this;
  }
  ObjectAccessControl& set_id(std::string v) {
    id_ = std::move(v);
    return *this;
  }
  ObjectAccessControl& set_kind(std::string v) {
    kind_ = std::move(v);
    return *this;
  }
  ObjectAccessControl& set_project_team(ProjectTeam v) {
    project_team_ = std::move(v);
    return *this;
  }
  ObjectAccessControl& set_self_link(std::string v) {
    self_link_ = std::move(v);
    return *this;
  }
  ///@}

  friend bool operator==(ObjectAccessControl const& lhs,
                         ObjectAccessControl const& rhs);
  friend bool operator!=(ObjectAccessControl const& lhs,
                         ObjectAccessControl const& rhs) {
    return !(lhs == rhs);
  }

 private:
  std::string bucket_;
  std::string object_;
  std::int64_t generation_ = 0;
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

std::ostream& operator<<(std::ostream& os, ObjectAccessControl const& rhs);

/**
 * Prepares a patch for an ObjectAccessControl resource.
 *
 * The ObjectAccessControl resource only has two modifiable fields: entity
 * and role. This class allows application developers to setup a PATCH message,
 * note that some of the possible PATCH messages may result in errors from the
 * server, for example: while it is possible to express "change the value of the
 * entity field" with a PATCH request, the server rejects such changes.
 *
 * @see
 * https://cloud.google.com/storage/docs/json_api/v1/how-tos/performance#patch
 *     for general information on PATCH requests for the Google Cloud Storage
 *     JSON API.
 */
class ObjectAccessControlPatchBuilder {
 public:
  ObjectAccessControlPatchBuilder() = default;

  std::string BuildPatch() const { return impl_.ToString(); }

  ObjectAccessControlPatchBuilder& set_entity(std::string const& v) {
    impl_.SetStringField("entity", v);
    return *this;
  }

  ObjectAccessControlPatchBuilder& delete_entity() {
    impl_.RemoveField("entity");
    return *this;
  }

  ObjectAccessControlPatchBuilder& set_role(std::string const& v) {
    impl_.SetStringField("role", v);
    return *this;
  }

  ObjectAccessControlPatchBuilder& delete_role() {
    impl_.RemoveField("role");
    return *this;
  }

 private:
  friend struct internal::GrpcObjectAccessControlParser;

  internal::PatchBuilder impl_;
};

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_OBJECT_ACCESS_CONTROL_H
