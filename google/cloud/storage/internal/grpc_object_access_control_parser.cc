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

#include "google/cloud/storage/internal/grpc_object_access_control_parser.h"
#include "google/cloud/storage/internal/patch_builder_details.h"
#include "google/cloud/storage/version.h"

namespace google {
namespace cloud {
namespace storage_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

google::storage::v2::ObjectAccessControl ToProto(
    storage::ObjectAccessControl const& acl) {
  google::storage::v2::ObjectAccessControl result;
  result.set_role(acl.role());
  result.set_id(acl.id());
  result.set_entity(acl.entity());
  result.set_entity_id(acl.entity_id());
  result.set_email(acl.email());
  result.set_domain(acl.domain());
  if (acl.has_project_team()) {
    result.mutable_project_team()->set_project_number(
        acl.project_team().project_number);
    result.mutable_project_team()->set_team(acl.project_team().team);
  }
  result.set_etag(acl.etag());
  return result;
}

storage::ObjectAccessControl FromProto(
    google::storage::v2::ObjectAccessControl acl,
    std::string const& bucket_name, std::string const& object_name,
    std::uint64_t generation) {
  storage::ObjectAccessControl result;
  result.set_kind("storage#objectAccessControl");
  result.set_bucket(bucket_name);
  result.set_object(object_name);
  result.set_generation(generation);
  result.set_domain(std::move(*acl.mutable_domain()));
  result.set_email(std::move(*acl.mutable_email()));
  result.set_entity(std::move(*acl.mutable_entity()));
  result.set_entity_id(std::move(*acl.mutable_entity_id()));
  result.set_id(std::move(*acl.mutable_id()));
  if (acl.has_project_team()) {
    result.set_project_team(storage::ProjectTeam{
        std::move(*acl.mutable_project_team()->mutable_project_number()),
        std::move(*acl.mutable_project_team()->mutable_team()),
    });
  }
  result.set_role(std::move(*acl.mutable_role()));
  result.set_etag(std::move(*acl.mutable_etag()));

  return result;
}

std::string Role(storage::ObjectAccessControlPatchBuilder const& patch) {
  return storage::internal::PatchBuilderDetails::GetPatch(patch).value("role",
                                                                       "");
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_internal
}  // namespace cloud
}  // namespace google
