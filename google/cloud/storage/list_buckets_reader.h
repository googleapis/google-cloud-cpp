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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_LIST_BUCKETS_READER_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_LIST_BUCKETS_READER_H

#include "google/cloud/storage/internal/bucket_requests.h"
#include "google/cloud/storage/internal/range_from_pagination.h"
#include "google/cloud/storage/internal/raw_client.h"
#include "google/cloud/storage/version.h"

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
using ListBucketsReader =
    internal::PaginationRange<BucketMetadata, internal::ListBucketsRequest,
                              internal::ListBucketsResponse>;

using ListBucketsIterator = ListBucketsReader::iterator;

}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_LIST_BUCKETS_READER_H
