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

#include "google/cloud/storage/internal/object_acl_requests.h"
#include "google/cloud/storage/internal/access_control_common_parser.h"
#include "google/cloud/storage/internal/metadata_parser.h"
#include "google/cloud/storage/internal/object_access_control_parser.h"
#include "google/cloud/storage/internal/patch_builder.h"
#include "google/cloud/internal/absl_str_join_quiet.h"
#include <iostream>

namespace google {
namespace cloud {
namespace storage {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {

std::ostream& operator<<(std::ostream& os, ListObjectAclRequest const& r) {
  os << "ListObjectAclRequest={bucket_name=" << r.bucket_name()
     << ", object_name=" << r.object_name();
  r.DumpOptions(os, ", ");
  return os << "}";
}

StatusOr<ListObjectAclResponse> ListObjectAclResponse::FromHttpResponse(
    std::string const& payload) {
  auto json = nlohmann::json::parse(payload, nullptr, false);
  if (!json.is_object()) {
    return Status(StatusCode::kInvalidArgument, __func__);
  }
  ListObjectAclResponse result;
  for (auto const& kv : json["items"].items()) {
    auto parsed = ObjectAccessControlParser::FromJson(kv.value());
    if (!parsed.ok()) {
      return std::move(parsed).status();
    }
    result.items.emplace_back(std::move(*parsed));
  }

  return result;
}

StatusOr<ListObjectAclResponse> ListObjectAclResponse::FromHttpResponse(
    HttpResponse const& response) {
  return FromHttpResponse(response.payload);
}

std::ostream& operator<<(std::ostream& os, ListObjectAclResponse const& r) {
  os << "ListObjectAclResponse={items={";
  os << absl::StrJoin(r.items, ", ", absl::StreamFormatter());
  return os << "}}";
}

std::ostream& operator<<(std::ostream& os, GetObjectAclRequest const& r) {
  os << "GetObjectAclRequest={bucket_name=" << r.bucket_name()
     << ", object_name=" << r.object_name() << ", entity=" << r.entity();
  r.DumpOptions(os, ", ");
  return os << "}";
}

std::ostream& operator<<(std::ostream& os, DeleteObjectAclRequest const& r) {
  os << "DeleteObjectAclRequest={bucket_name=" << r.bucket_name()
     << ", object_name=" << r.object_name() << ", entity=" << r.entity();
  r.DumpOptions(os, ", ");
  return os << "}";
}

std::ostream& operator<<(std::ostream& os, CreateObjectAclRequest const& r) {
  os << "CreateObjectAclRequest={bucket_name=" << r.bucket_name()
     << ", object_name=" << r.object_name() << ", entity=" << r.entity()
     << ", role=" << r.role();
  r.DumpOptions(os, ", ");
  return os << "}";
}

std::ostream& operator<<(std::ostream& os, UpdateObjectAclRequest const& r) {
  os << "UpdateObjectAclRequest={bucket_name=" << r.bucket_name()
     << ", object_name=" << r.object_name() << ", entity=" << r.entity()
     << ", role=" << r.role();
  r.DumpOptions(os, ", ");
  return os << "}";
}

PatchObjectAclRequest::PatchObjectAclRequest(
    std::string bucket, std::string object, std::string entity,
    ObjectAccessControl const& original, ObjectAccessControl const& new_acl)
    : PatchObjectAclRequest(std::move(bucket), std::move(object),
                            std::move(entity),
                            DiffObjectAccessControl(original, new_acl)) {}

PatchObjectAclRequest::PatchObjectAclRequest(
    std::string bucket, std::string object, std::string entity,
    ObjectAccessControlPatchBuilder patch)
    : GenericObjectAclRequest(std::move(bucket), std::move(object),
                              std::move(entity)),
      patch_(std::move(patch)) {}

std::ostream& operator<<(std::ostream& os, PatchObjectAclRequest const& r) {
  os << "ObjectAclRequest={bucket_name=" << r.bucket_name()
     << ", object_name=" << r.object_name() << ", entity=" << r.entity();
  r.DumpOptions(os, ", ");
  return os << ", payload=" << r.payload() << "}";
}

ObjectAccessControlPatchBuilder DiffObjectAccessControl(
    ObjectAccessControl const& original, ObjectAccessControl const& new_acl) {
  ObjectAccessControlPatchBuilder patch;
  if (original.entity() != new_acl.entity()) patch.set_role(new_acl.entity());
  if (original.role() != new_acl.role()) patch.set_role(new_acl.role());
  return patch;
}

}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google
