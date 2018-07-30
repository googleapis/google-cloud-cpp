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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_BUCKET_ACCESS_CONTROL_H_
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_BUCKET_ACCESS_CONTROL_H_

#include "google/cloud/storage/internal/access_control_common.h"

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
/**
 * A wrapper for the bucketAccessControl resource in Google Cloud Storage.
 *
 * @see
 * https://cloud.google.com/storage/docs/json_api/v1/bucketAccessControls for
 * an authoritative source of field definitions.
 */
class BucketAccessControl : private internal::AccessControlCommon {
 public:
  BucketAccessControl() = default;

  static BucketAccessControl ParseFromJson(internal::nl::json const& json);

  /// Parse from a string in JSON format.
  static BucketAccessControl ParseFromString(std::string const& payload);

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

  bool operator==(BucketAccessControl const& rhs) const;
  bool operator!=(BucketAccessControl const& rhs) const {
    return not(*this == rhs);
  }
};

std::ostream& operator<<(std::ostream& os, BucketAccessControl const& rhs);

}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_BUCKET_ACCESS_CONTROL_H_
