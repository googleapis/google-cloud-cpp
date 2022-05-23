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
namespace storage {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {

google::storage::v2::ObjectAccessControl GrpcObjectAccessControlParser::ToProto(
    ObjectAccessControl const& acl) {
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
  return result;
}

ObjectAccessControl GrpcObjectAccessControlParser::FromProto(
    google::storage::v2::ObjectAccessControl acl,
    std::string const& bucket_name, std::string const& object_name,
    std::uint64_t generation) {
  ObjectAccessControl result;
  result.kind_ = "storage#objectAccessControl";
  result.bucket_ = bucket_name;
  result.object_ = object_name;
  result.generation_ = generation;
  result.domain_ = std::move(*acl.mutable_domain());
  result.email_ = std::move(*acl.mutable_email());
  result.entity_ = std::move(*acl.mutable_entity());
  result.entity_id_ = std::move(*acl.mutable_entity_id());
  result.id_ = std::move(*acl.mutable_id());
  if (acl.has_project_team()) {
    result.project_team_ = ProjectTeam{
        std::move(*acl.mutable_project_team()->mutable_project_number()),
        std::move(*acl.mutable_project_team()->mutable_team()),
    };
  }
  result.role_ = std::move(*acl.mutable_role());
  result.self_link_.clear();

  return result;
}

std::string GrpcObjectAccessControlParser::Role(
    ObjectAccessControlPatchBuilder const& patch) {
  return PatchBuilderDetails::GetPatch(patch.impl_).value("role", "");
}

}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google
