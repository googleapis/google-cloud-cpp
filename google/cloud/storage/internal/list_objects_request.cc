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

#include "google/cloud/storage/internal/list_objects_request.h"
#include "google/cloud/storage/internal/nljson.h"
#include "google/cloud/storage/object_metadata.h"
#include <sstream>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace internal {
std::ostream& operator<<(std::ostream& os, ListObjectsRequest const& r) {
  os << "ListObjectsRequest={bucket_name=" << r.bucket_name();
  r.DumpOptions(os, ", ");
  return os << "}";
}

ListObjectsResponse ListObjectsResponse::FromHttpResponse(
    HttpResponse&& response) {
  auto json = storage::internal::nl::json::parse(response.payload);

  ListObjectsResponse result;
  result.next_page_token = json.value("nextPageToken", "");

  for (auto const& kv : json["items"].items()) {
    result.items.emplace_back(ObjectMetadata::ParseFromJson(kv.value()));
  }

  return result;
}

std::ostream& operator<<(std::ostream& os, ListObjectsResponse const& r) {
  os << "ListObjectsResponse={next_page_token=" << r.next_page_token
     << ", items={";
  std::copy(r.items.begin(), r.items.end(),
            std::ostream_iterator<ObjectMetadata>(os, "\n  "));
  return os << "}}";
}

}  // namespace internal
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google
