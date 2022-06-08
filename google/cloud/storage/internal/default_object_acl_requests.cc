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

#include "google/cloud/storage/internal/default_object_acl_requests.h"
#include "google/cloud/storage/internal/object_access_control_parser.h"
#include "google/cloud/storage/internal/object_acl_requests.h"
#include "google/cloud/internal/absl_str_join_quiet.h"
#include <nlohmann/json.hpp>
#include <iostream>

namespace google {
namespace cloud {
namespace storage {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {

std::ostream& operator<<(std::ostream& os,
                         ListDefaultObjectAclRequest const& r) {
  os << "ListDefaultObjectAclRequest={bucket_name=" << r.bucket_name();
  r.DumpOptions(os, ", ");
  return os << "}";
}

StatusOr<ListDefaultObjectAclResponse>
ListDefaultObjectAclResponse::FromHttpResponse(std::string const& payload) {
  auto json = nlohmann::json::parse(payload, nullptr, false);
  if (!json.is_object()) {
    return Status(StatusCode::kInvalidArgument, __func__);
  }
  ListDefaultObjectAclResponse result;
  for (auto const& kv : json["items"].items()) {
    auto parsed = internal::ObjectAccessControlParser::FromJson(kv.value());
    if (!parsed.ok()) {
      return std::move(parsed).status();
    }
    result.items.emplace_back(std::move(*parsed));
  }

  return result;
}

StatusOr<ListDefaultObjectAclResponse>
ListDefaultObjectAclResponse::FromHttpResponse(HttpResponse const& response) {
  return FromHttpResponse(response.payload);
}

std::ostream& operator<<(std::ostream& os,
                         ListDefaultObjectAclResponse const& r) {
  os << "ListDefaultObjectAclResponse={items={";
  os << absl::StrJoin(r.items, ", ", absl::StreamFormatter());
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
    : PatchDefaultObjectAclRequest(std::move(bucket), std::move(entity),
                                   DiffObjectAccessControl(original, new_acl)) {
}

PatchDefaultObjectAclRequest::PatchDefaultObjectAclRequest(
    std::string bucket, std::string entity,
    ObjectAccessControlPatchBuilder patch)
    : GenericDefaultObjectAclRequest(std::move(bucket), std::move(entity)),
      patch_(std::move(patch)) {}

std::ostream& operator<<(std::ostream& os,
                         PatchDefaultObjectAclRequest const& r) {
  os << "DefaultObjectAclRequest={bucket_name=" << r.bucket_name()
     << ", entity=" << r.entity();
  r.DumpOptions(os, ", ");
  return os << ", payload=" << r.payload() << "}";
}

}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google
