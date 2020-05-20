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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_BUCKET_ACCESS_CONTROL_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_BUCKET_ACCESS_CONTROL_H

#include "google/cloud/storage/internal/access_control_common.h"
#include "google/cloud/storage/internal/patch_builder.h"
#include "google/cloud/storage/version.h"
#include "google/cloud/status_or.h"

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace internal {
struct BucketAccessControlParser;
}  // namespace internal

/**
 * Wraps the bucketAccessControl resource in Google Cloud Storage.
 *
 * BucketAccessControl describes the access to a bucket for a single entity,
 * where the entity might be a user, group, or other role.
 *
 * @see
 * https://cloud.google.com/storage/docs/json_api/v1/bucketAccessControls for
 *     an authoritative source of field definitions.
 */
class BucketAccessControl : private internal::AccessControlCommon {
 public:
  BucketAccessControl() = default;

  using AccessControlCommon::ROLE_OWNER;
  using AccessControlCommon::ROLE_READER;
  using AccessControlCommon::TEAM_EDITORS;
  using AccessControlCommon::TEAM_OWNERS;
  using AccessControlCommon::TEAM_VIEWERS;

  using AccessControlCommon::bucket;
  using AccessControlCommon::domain;
  using AccessControlCommon::email;

  using AccessControlCommon::entity;
  BucketAccessControl& set_entity(std::string v) {
    AccessControlCommon::set_entity(std::move(v));
    return *this;
  }

  using AccessControlCommon::entity_id;
  using AccessControlCommon::etag;
  using AccessControlCommon::has_project_team;
  using AccessControlCommon::id;
  using AccessControlCommon::kind;
  using AccessControlCommon::project_team;

  using AccessControlCommon::role;
  BucketAccessControl& set_role(std::string v) {
    AccessControlCommon::set_role(std::move(v));
    return *this;
  }

  using AccessControlCommon::self_link;

  friend bool operator==(BucketAccessControl const& lhs,
                         BucketAccessControl const& rhs);
  friend bool operator!=(BucketAccessControl const& lhs,
                         BucketAccessControl const& rhs) {
    return !(lhs == rhs);
  }

  friend struct internal::BucketAccessControlParser;
  friend class internal::GrpcClient;
};

std::ostream& operator<<(std::ostream& os, BucketAccessControl const& rhs);

/**
 * Prepares a patch for a BucketAccessControl resource.
 *
 * The BucketAccessControl resource only has two modifiable fields: entity
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
class BucketAccessControlPatchBuilder {
 public:
  BucketAccessControlPatchBuilder() = default;

  std::string BuildPatch() const { return impl_.ToString(); }

  BucketAccessControlPatchBuilder& set_entity(std::string const& v) {
    impl_.SetStringField("entity", v);
    return *this;
  }

  BucketAccessControlPatchBuilder& delete_entity() {
    impl_.RemoveField("entity");
    return *this;
  }

  BucketAccessControlPatchBuilder& set_role(std::string const& v) {
    impl_.SetStringField("role", v);
    return *this;
  }

  BucketAccessControlPatchBuilder& delete_role() {
    impl_.RemoveField("role");
    return *this;
  }

 private:
  internal::PatchBuilder impl_;
};

}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_BUCKET_ACCESS_CONTROL_H
