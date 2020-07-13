// Copyright 2020 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_LIST_OBJECTS_AND_PREFIXES_READER_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_LIST_OBJECTS_AND_PREFIXES_READER_H

#include "google/cloud/storage/internal/object_requests.h"
#include "google/cloud/storage/internal/range_from_pagination.h"
#include "absl/types/variant.h"
#include <iterator>
#include <string>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
using ObjectOrPrefix = absl::variant<ObjectMetadata, std::string>;

using ListObjectsAndPrefixesReader =
    google::cloud::storage::internal::PaginationRange<
        ObjectOrPrefix, internal::ListObjectsRequest,
        internal::ListObjectsResponse>;

using ListObjectsAndPrefixesIterator = ListObjectsAndPrefixesReader::iterator;

namespace internal {
inline void SortObjectsAndPrefixes(std::vector<ObjectOrPrefix>& in) {
  struct GetNameOrPrefix {
    std::string const& operator()(std::string const& v) { return v; }
    std::string const& operator()(ObjectMetadata const& v) { return v.name(); }
  };
  std::sort(in.begin(), in.end(),
            [](ObjectOrPrefix const& a, ObjectOrPrefix const& b) {
              return (absl::visit(GetNameOrPrefix{}, a) <
                      absl::visit(GetNameOrPrefix{}, b));
            });
}
}  // namespace internal

}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_LIST_OBJECTS_AND_PREFIXES_READER_H
