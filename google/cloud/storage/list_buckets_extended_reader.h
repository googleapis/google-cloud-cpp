// Copyright 2025 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_LIST_BUCKETS_EXTENDED_READER_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_LIST_BUCKETS_EXTENDED_READER_H

#include "google/cloud/storage/bucket_metadata.h"
#include "google/cloud/storage/version.h"
#include "google/cloud/internal/pagination_range.h"
#include <string>
#include <vector>

namespace google {
namespace cloud {
namespace storage {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

struct BucketsExtended {
  std::vector<BucketMetadata> buckets;
  std::vector<std::string> unreachable;
};

using ListBucketsExtendedReader =
    google::cloud::internal::PaginationRange<BucketsExtended>;

using ListBucketsExtendedIterator = ListBucketsExtendedReader::iterator;

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_LIST_BUCKETS_EXTENDED_READER_H
