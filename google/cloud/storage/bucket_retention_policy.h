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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_BUCKET_RETENTION_POLICY_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_BUCKET_RETENTION_POLICY_H

#include "google/cloud/storage/version.h"
#include <chrono>
#include <iosfwd>
#include <tuple>
#include <utility>

namespace google {
namespace cloud {
namespace storage {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

/**
 * The retention policy for a bucket.
 *
 * The Bucket Lock feature of Google Cloud Storage allows you to configure a
 * data retention policy for a Cloud Storage bucket. This policy governs how
 * long objects in the bucket must be retained. The feature also allows you to
 * lock the data retention policy, permanently preventing the policy from from
 * being reduced or removed.
 *
 * @see https://cloud.google.com/storage/docs/bucket-lock for a general
 *     overview
 */
struct BucketRetentionPolicy {
  std::chrono::seconds retention_period;
  std::chrono::system_clock::time_point effective_time;
  bool is_locked;
};

inline bool operator==(BucketRetentionPolicy const& lhs,
                       BucketRetentionPolicy const& rhs) {
  return std::tie(lhs.retention_period, lhs.effective_time, lhs.is_locked) ==
         std::tie(rhs.retention_period, rhs.effective_time, rhs.is_locked);
}

inline bool operator<(BucketRetentionPolicy const& lhs,
                      BucketRetentionPolicy const& rhs) {
  return std::tie(lhs.retention_period, lhs.effective_time, lhs.is_locked) <
         std::tie(rhs.retention_period, rhs.effective_time, rhs.is_locked);
}

inline bool operator!=(BucketRetentionPolicy const& lhs,
                       BucketRetentionPolicy const& rhs) {
  return std::rel_ops::operator!=(lhs, rhs);
}

inline bool operator>(BucketRetentionPolicy const& lhs,
                      BucketRetentionPolicy const& rhs) {
  return std::rel_ops::operator>(lhs, rhs);
}

inline bool operator<=(BucketRetentionPolicy const& lhs,
                       BucketRetentionPolicy const& rhs) {
  return std::rel_ops::operator<=(lhs, rhs);
}

inline bool operator>=(BucketRetentionPolicy const& lhs,
                       BucketRetentionPolicy const& rhs) {
  return std::rel_ops::operator>=(lhs, rhs);
}

std::ostream& operator<<(std::ostream& os, BucketRetentionPolicy const& rhs);

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_BUCKET_RETENTION_POLICY_H
