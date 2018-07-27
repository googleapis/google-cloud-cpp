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

#include "google/cloud/storage/internal/list_object_acl_request.h"
#include "google/cloud/storage/internal/nljson.h"
#include <iostream>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace internal {
std::ostream& operator<<(std::ostream& os, ListObjectAclRequest const& r) {
  os << "ListObjectAclRequest={bucket_name=" << r.bucket_name()
     << ", object_name=" << r.object_name();
  r.DumpOptions(os, ", ");
  return os << "}";
}

ListObjectAclResponse ListObjectAclResponse::FromHttpResponse(
    HttpResponse&& response) {
  ListObjectAclResponse result;
  auto json = nl::json::parse(response.payload);
  for (auto const& kv : json["items"].items()) {
    result.items.emplace_back(ObjectAccessControl::ParseFromJson(kv.value()));
  }

  return result;
}

std::ostream& operator<<(std::ostream& os, ListObjectAclResponse const& r) {
  os << "ListObjectAclResponse={items={";
  char const* sep = "";
  for (auto const& acl : r.items) {
    os << sep << acl;
    sep = ", ";
  }
  return os << "}}";
}

}  // namespace internal
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google
