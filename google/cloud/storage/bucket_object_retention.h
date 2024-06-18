// Copyright 2024 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_BUCKET_OBJECT_RETENTION_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_BUCKET_OBJECT_RETENTION_H

#include "google/cloud/storage/version.h"
#include <iosfwd>

namespace google {
namespace cloud {
namespace storage {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

/**
 * The soft delete policy for a bucket.
 *
 * The soft delete policy prevents soft-deleted objects from being permanently
 * deleted.
 */
struct BucketObjectRetention {
  bool enabled;
};

inline bool operator==(BucketObjectRetention const& lhs,
                       BucketObjectRetention const& rhs) {
  return lhs.enabled == rhs.enabled;
}

inline bool operator!=(BucketObjectRetention const& lhs,
                       BucketObjectRetention const& rhs) {
  return !(lhs == rhs);
}

std::ostream& operator<<(std::ostream& os, BucketObjectRetention const& rhs);

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_BUCKET_OBJECT_RETENTION_H
