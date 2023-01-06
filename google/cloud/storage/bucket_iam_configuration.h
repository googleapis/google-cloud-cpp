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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_BUCKET_IAM_CONFIGURATION_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_BUCKET_IAM_CONFIGURATION_H

#include "google/cloud/storage/version.h"
#include "absl/types/optional.h"
#include <chrono>
#include <iosfwd>
#include <utility>

namespace google {
namespace cloud {
namespace storage {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

/**
 * Configure if only the IAM policies are used for access control.
 *
 * @see Before enabling Uniform Bucket Level Access please
 *     review the [feature documentation][ubla-link], as well as
 *     ["Should you use uniform bucket-level access ?"][ubla-should-link].
 *
 * [ubla-link]:
 * https://cloud.google.com/storage/docs/uniform-bucket-level-access
 * [ubla-should-link]:
 * https://cloud.google.com/storage/docs/uniform-bucket-level-access#should-you-use
 */
struct UniformBucketLevelAccess {
  bool enabled;
  std::chrono::system_clock::time_point locked_time;
};
using BucketPolicyOnly = UniformBucketLevelAccess;

//@{
/// @name Comparison operators For UniformBucketLevelAccess
inline bool operator==(UniformBucketLevelAccess const& lhs,
                       UniformBucketLevelAccess const& rhs) {
  return std::tie(lhs.enabled, lhs.locked_time) ==
         std::tie(rhs.enabled, rhs.locked_time);
}

inline bool operator<(UniformBucketLevelAccess const& lhs,
                      UniformBucketLevelAccess const& rhs) {
  return std::tie(lhs.enabled, lhs.locked_time) <
         std::tie(rhs.enabled, rhs.locked_time);
}

inline bool operator!=(UniformBucketLevelAccess const& lhs,
                       UniformBucketLevelAccess const& rhs) {
  return std::rel_ops::operator!=(lhs, rhs);
}

inline bool operator>(UniformBucketLevelAccess const& lhs,
                      UniformBucketLevelAccess const& rhs) {
  return std::rel_ops::operator>(lhs, rhs);
}

inline bool operator<=(UniformBucketLevelAccess const& lhs,
                       UniformBucketLevelAccess const& rhs) {
  return std::rel_ops::operator<=(lhs, rhs);
}

inline bool operator>=(UniformBucketLevelAccess const& lhs,
                       UniformBucketLevelAccess const& rhs) {
  return std::rel_ops::operator>=(lhs, rhs);
}
//@}

std::ostream& operator<<(std::ostream& os, UniformBucketLevelAccess const& rhs);

/**
 * The IAM configuration for a Bucket.
 *
 * Currently this only holds the UniformBucketLevelAccess. In the future, we may
 * define additional IAM which would be included in this object.
 *
 * @see Before enabling Uniform Bucket Level Access please review the
 *     [feature documentation][ubla-link], as well as
 *     ["Should you use uniform bucket-level access ?"][ubla-should-link].
 *
 * [ubla-link]:
 * https://cloud.google.com/storage/docs/uniform-bucket-level-access
 * [ubla-should-link]:
 * https://cloud.google.com/storage/docs/uniform-bucket-level-access#should-you-use
 */
struct BucketIamConfiguration {
  absl::optional<UniformBucketLevelAccess> uniform_bucket_level_access;
  absl::optional<std::string> public_access_prevention;
};

//@{
/// @name Public Access Prevention helper functions
inline std::string PublicAccessPreventionEnforced() { return "enforced"; }
inline std::string PublicAccessPreventionInherited() { return "inherited"; }
GOOGLE_CLOUD_CPP_DEPRECATED("Use PublicAccessPreventionInherited()")
inline std::string PublicAccessPreventionUnspecified() { return "unspecified"; }
//@}

//@{
/// @name Comparison operators for BucketIamConfiguration.
inline bool operator==(BucketIamConfiguration const& lhs,
                       BucketIamConfiguration const& rhs) {
  return std::tie(lhs.uniform_bucket_level_access,
                  lhs.public_access_prevention) ==
         std::tie(rhs.uniform_bucket_level_access,
                  rhs.public_access_prevention);
}

inline bool operator<(BucketIamConfiguration const& lhs,
                      BucketIamConfiguration const& rhs) {
  return std::tie(lhs.uniform_bucket_level_access,
                  lhs.public_access_prevention) <
         std::tie(rhs.uniform_bucket_level_access,
                  rhs.public_access_prevention);
}

inline bool operator!=(BucketIamConfiguration const& lhs,
                       BucketIamConfiguration const& rhs) {
  return std::rel_ops::operator!=(lhs, rhs);
}

inline bool operator>(BucketIamConfiguration const& lhs,
                      BucketIamConfiguration const& rhs) {
  return std::rel_ops::operator>(lhs, rhs);
}

inline bool operator<=(BucketIamConfiguration const& lhs,
                       BucketIamConfiguration const& rhs) {
  return std::rel_ops::operator<=(lhs, rhs);
}

inline bool operator>=(BucketIamConfiguration const& lhs,
                       BucketIamConfiguration const& rhs) {
  return std::rel_ops::operator>=(lhs, rhs);
}
//@}

std::ostream& operator<<(std::ostream& os, BucketIamConfiguration const& rhs);

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_BUCKET_IAM_CONFIGURATION_H
