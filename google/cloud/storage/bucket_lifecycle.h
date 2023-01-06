// Copyright 2022 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_BUCKET_LIFECYCLE_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_BUCKET_LIFECYCLE_H

#include "google/cloud/storage/lifecycle_rule.h"
#include "google/cloud/storage/version.h"
#include <utility>
#include <vector>

namespace google {
namespace cloud {
namespace storage {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

/**
 * The Object Lifecycle configuration for a Bucket.
 *
 * @see https://cloud.google.com/storage/docs/managing-lifecycles for general
 *     information on object lifecycle rules.
 */
struct BucketLifecycle {
  std::vector<LifecycleRule> rule;
};

///@{
/// @name Comparison operators for BucketLifecycle.
inline bool operator==(BucketLifecycle const& lhs, BucketLifecycle const& rhs) {
  return lhs.rule == rhs.rule;
}

inline bool operator<(BucketLifecycle const& lhs, BucketLifecycle const& rhs) {
  return lhs.rule < rhs.rule;
}

inline bool operator!=(BucketLifecycle const& lhs, BucketLifecycle const& rhs) {
  return std::rel_ops::operator!=(lhs, rhs);
}

inline bool operator>(BucketLifecycle const& lhs, BucketLifecycle const& rhs) {
  return std::rel_ops::operator>(lhs, rhs);
}

inline bool operator<=(BucketLifecycle const& lhs, BucketLifecycle const& rhs) {
  return std::rel_ops::operator<=(lhs, rhs);
}

inline bool operator>=(BucketLifecycle const& lhs, BucketLifecycle const& rhs) {
  return std::rel_ops::operator>=(lhs, rhs);
}
///@}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_BUCKET_LIFECYCLE_H
