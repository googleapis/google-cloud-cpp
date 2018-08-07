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

#include "google/cloud/storage/internal/default_object_acl_requests.h"
#include "google/cloud/storage/internal/nljson.h"
#include <iostream>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace internal {
std::ostream& operator<<(std::ostream& os,
                         ListDefaultObjectAclRequest const& r) {
  os << "ListDefaultObjectAclRequest={bucket_name=" << r.bucket_name();
  r.DumpOptions(os, ", ");
  return os << "}";
}

ListDefaultObjectAclResponse ListDefaultObjectAclResponse::FromHttpResponse(
    HttpResponse&& response) {
  ListDefaultObjectAclResponse result;
  auto json = nl::json::parse(response.payload);
  for (auto const& kv : json["items"].items()) {
    result.items.emplace_back(ObjectAccessControl::ParseFromJson(kv.value()));
  }

  return result;
}

std::ostream& operator<<(std::ostream& os,
                         ListDefaultObjectAclResponse const& r) {
  os << "ListDefaultObjectAclResponse={items={";
  char const* sep = "";
  for (auto const& acl : r.items) {
    os << sep << acl;
    sep = ", ";
  }
  return os << "}}";
}

std::ostream& operator<<(std::ostream& os,
                         GetDefaultObjectAclRequest const& r) {
  os << "GetDefaultObjectAclRequest={bucket_name=" << r.bucket_name()
     << ", entity=" << r.entity();
  r.DumpOptions(os, ", ");
  return os << "}";
}

std::ostream& operator<<(std::ostream& os,
                         DeleteDefaultObjectAclRequest const& r) {
  os << "GetDefaultObjectAclRequest={bucket_name=" << r.bucket_name()
     << ", entity=" << r.entity();
  r.DumpOptions(os, ", ");
  return os << "}";
}

std::ostream& operator<<(std::ostream& os,
                         CreateDefaultObjectAclRequest const& r) {
  os << "CreateDefaultObjectAclRequest={bucket_name=" << r.bucket_name()
     << ", entity=" << r.entity() << ", role=" << r.role();
  r.DumpOptions(os, ", ");
  return os << "}";
}

PatchDefaultObjectAclRequest::PatchDefaultObjectAclRequest(
    std::string bucket, std::string entity, ObjectAccessControl const& original,
    ObjectAccessControl const& new_acl)
    : GenericDefaultObjectAclRequest(std::move(bucket), std::move(entity)) {
  PatchBuilder build_patch;
  build_patch.AddStringField("bucket", original.bucket(), new_acl.bucket());
  build_patch.AddStringField("domain", original.domain(), new_acl.domain());
  build_patch.AddStringField("email", original.email(), new_acl.email());
  build_patch.AddStringField("entity", original.entity(), new_acl.entity());
  build_patch.AddStringField("entityId", original.entity_id(),
                             new_acl.entity_id());
  build_patch.AddStringField("etag", original.etag(), new_acl.etag());
  build_patch.AddIntField("generation", original.generation(),
                          new_acl.generation());
  build_patch.AddStringField("id", original.id(), new_acl.id());
  build_patch.AddStringField("kind", original.kind(), new_acl.kind());
  build_patch.AddStringField("object", original.object(), new_acl.object());

  if (original.project_team() != new_acl.project_team()) {
    auto empty = [](ProjectTeam const& p) {
      return p.project_number.empty() and p.team.empty();
    };
    if (empty(new_acl.project_team())) {
      if (not empty(original.project_team())) {
        build_patch.RemoveField("projectTeam");
      }
    } else {
      PatchBuilder project_team_patch;
      project_team_patch
          .AddStringField("project_number",
                          original.project_team().project_number,
                          new_acl.project_team().project_number)
          .AddStringField("team", original.project_team().team,
                          new_acl.project_team().team);
      build_patch.AddSubPatch("projectTeam", project_team_patch);
    }
  }

  build_patch.AddStringField("role", original.role(), new_acl.role());
  build_patch.AddStringField("selfLink", original.self_link(),
                             new_acl.self_link());
  payload_ = build_patch.ToString();
}

PatchDefaultObjectAclRequest::PatchDefaultObjectAclRequest(
    std::string bucket, std::string entity,
    ObjectAccessControlPatchBuilder const& patch)
    : GenericDefaultObjectAclRequest(std::move(bucket), std::move(entity)),
      payload_(patch.BuildPatch()) {}

std::ostream& operator<<(std::ostream& os,
                         PatchDefaultObjectAclRequest const& r) {
  os << "DefaultObjectAclRequest={bucket_name=" << r.bucket_name()
     << ", entity=" << r.entity();
  r.DumpOptions(os, ", ");
  os << ", payload=" << r.payload();
  return os << "}";
}

}  // namespace internal
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google
