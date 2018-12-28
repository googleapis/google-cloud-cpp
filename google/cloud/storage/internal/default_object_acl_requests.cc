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

StatusOr<ListDefaultObjectAclResponse>
ListDefaultObjectAclResponse::FromHttpResponse(HttpResponse&& response) {
  auto json = nl::json::parse(response.payload, nullptr, false);
  if (not json.is_object()) {
    return Status(StatusCode::INVALID_ARGUMENT, __func__);
  }
  ListDefaultObjectAclResponse result;
  for (auto const& kv : json["items"].items()) {
    auto parsed = ObjectAccessControl::ParseFromJson(kv.value());
    if (not parsed.ok()) {
      return std::move(parsed).status();
    }
    result.items.emplace_back(std::move(*parsed));
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

std::ostream& operator<<(std::ostream& os,
                         UpdateDefaultObjectAclRequest const& r) {
  os << "UpdateDefaultObjectAclRequest={bucket_name=" << r.bucket_name()
     << ", entity=" << r.entity() << ", role=" << r.role();
  r.DumpOptions(os, ", ");
  return os << "}";
}

PatchDefaultObjectAclRequest::PatchDefaultObjectAclRequest(
    std::string bucket, std::string entity, ObjectAccessControl const& original,
    ObjectAccessControl const& new_acl)
    : GenericDefaultObjectAclRequest(std::move(bucket), std::move(entity)) {
  PatchBuilder build_patch;
  build_patch.AddStringField("entity", original.entity(), new_acl.entity());
  build_patch.AddStringField("role", original.role(), new_acl.role());
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
  return os << ", payload=" << r.payload() << "}";
}

}  // namespace internal
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google
