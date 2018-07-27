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

#include "google/cloud/storage/internal/list_buckets_request.h"
#include "google/cloud/storage/internal/nljson.h"
#include "google/cloud/storage/object_metadata.h"
#include <sstream>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace internal {
std::ostream& operator<<(std::ostream& os, ListBucketsRequest const& r) {
  os << "ListBucketsRequest={project_id=" << r.project_id();
  r.DumpOptions(os, ", ");
  return os << "}";
}

ListBucketsResponse ListBucketsResponse::FromHttpResponse(
    HttpResponse&& response) {
  auto json = storage::internal::nl::json::parse(response.payload);

  ListBucketsResponse result;
  result.next_page_token = json.value("nextPageToken", "");

  for (auto const& kv : json["items"].items()) {
    result.items.emplace_back(BucketMetadata::ParseFromJson(kv.value()));
  }

  return result;
}

std::ostream& operator<<(std::ostream& os, ListBucketsResponse const& r) {
  os << "ListBucketsResponse={next_page_token=" << r.next_page_token
     << ", items={";
  std::copy(r.items.begin(), r.items.end(),
            std::ostream_iterator<BucketMetadata>(os, "\n  "));
  return os << "}}";
}

}  // namespace internal
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google
