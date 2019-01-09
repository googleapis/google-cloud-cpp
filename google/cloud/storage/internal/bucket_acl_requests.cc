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

#include "google/cloud/storage/internal/bucket_acl_requests.h"
#include "google/cloud/storage/internal/nljson.h"
#include <iostream>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace internal {
std::ostream& operator<<(std::ostream& os, ListBucketAclRequest const& r) {
  os << "ListBucketAclRequest={bucket_name=" << r.bucket_name();
  r.DumpOptions(os, ", ");
  return os << "}";
}

StatusOr<ListBucketAclResponse> ListBucketAclResponse::FromHttpResponse(
    HttpResponse&& response) {
  ListBucketAclResponse result;
  auto json = nl::json::parse(response.payload, nullptr, false);
  if (not json.is_object()) {
    return Status(StatusCode::kInvalidArgument, __func__);
  }
  for (auto const& kv : json["items"].items()) {
    auto parsed = BucketAccessControl::ParseFromJson(kv.value());
    if (not parsed.ok()) {
      return std::move(parsed).status();
    }
    result.items.emplace_back(std::move(*parsed));
  }

  return result;
}

std::ostream& operator<<(std::ostream& os, ListBucketAclResponse const& r) {
  os << "ListBucketAclResponse={items={";
  char const* sep = "";
  for (auto const& acl : r.items) {
    os << sep << acl;
    sep = ", ";
  }
  return os << "}}";
}

std::ostream& operator<<(std::ostream& os, GetBucketAclRequest const& r) {
  os << "GetBucketAclRequest={bucket_name=" << r.bucket_name()
     << ", entity=" << r.entity();
  r.DumpOptions(os, ", ");
  return os << "}";
}

std::ostream& operator<<(std::ostream& os, DeleteBucketAclRequest const& r) {
  os << "GetBucketAclRequest={bucket_name=" << r.bucket_name()
     << ", entity=" << r.entity();
  r.DumpOptions(os, ", ");
  return os << "}";
}

std::ostream& operator<<(std::ostream& os, CreateBucketAclRequest const& r) {
  os << "CreateBucketAclRequest={bucket_name=" << r.bucket_name()
     << ", entity=" << r.entity() << ", role=" << r.role();
  r.DumpOptions(os, ", ");
  return os << "}";
}

std::ostream& operator<<(std::ostream& os, UpdateBucketAclRequest const& r) {
  os << "UpdateBucketAclRequest={bucket_name=" << r.bucket_name()
     << ", entity=" << r.entity() << ", role=" << r.role();
  r.DumpOptions(os, ", ");
  return os << "}";
}

PatchBucketAclRequest::PatchBucketAclRequest(
    std::string bucket, std::string entity, BucketAccessControl const& original,
    BucketAccessControl const& new_acl)
    : GenericBucketAclRequest(std::move(bucket), std::move(entity)) {
  PatchBuilder build_patch;
  build_patch.AddStringField("entity", original.entity(), new_acl.entity());
  build_patch.AddStringField("role", original.role(), new_acl.role());
  payload_ = build_patch.ToString();
}

PatchBucketAclRequest::PatchBucketAclRequest(
    std::string bucket, std::string entity,
    BucketAccessControlPatchBuilder const& patch)
    : GenericBucketAclRequest(std::move(bucket), std::move(entity)),
      payload_(patch.BuildPatch()) {}

std::ostream& operator<<(std::ostream& os, PatchBucketAclRequest const& r) {
  os << "BucketAclRequest={bucket_name=" << r.bucket_name()
     << ", entity=" << r.entity();
  r.DumpOptions(os, ", ");
  return os << ", payload=" << r.payload() << "}";
}

}  // namespace internal
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google
