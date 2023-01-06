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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_BUCKET_CUSTOM_PLACEMENT_CONFIG_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_BUCKET_CUSTOM_PLACEMENT_CONFIG_H

#include "google/cloud/storage/version.h"
#include <iosfwd>
#include <string>
#include <utility>
#include <vector>

namespace google {
namespace cloud {
namespace storage {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

/**
 * Configuration for Custom Dual Regions.
 *
 * It should specify precisely two eligible regions within the same Multiregion.
 *
 * @see Additional information on custom dual regions in the
 *     [feature documentation][cdr-link].
 *
 * [cdr-link]: https://cloud.google.com/storage/docs/locations
 */
struct BucketCustomPlacementConfig {
  std::vector<std::string> data_locations;
};

//@{
/// @name Comparison operators for BucketCustomPlacementConfig.
inline bool operator==(BucketCustomPlacementConfig const& lhs,
                       BucketCustomPlacementConfig const& rhs) {
  return lhs.data_locations == rhs.data_locations;
}

inline bool operator<(BucketCustomPlacementConfig const& lhs,
                      BucketCustomPlacementConfig const& rhs) {
  return lhs.data_locations < rhs.data_locations;
}

inline bool operator!=(BucketCustomPlacementConfig const& lhs,
                       BucketCustomPlacementConfig const& rhs) {
  return std::rel_ops::operator!=(lhs, rhs);
}

inline bool operator>(BucketCustomPlacementConfig const& lhs,
                      BucketCustomPlacementConfig const& rhs) {
  return std::rel_ops::operator>(lhs, rhs);
}

inline bool operator<=(BucketCustomPlacementConfig const& lhs,
                       BucketCustomPlacementConfig const& rhs) {
  return std::rel_ops::operator<=(lhs, rhs);
}

inline bool operator>=(BucketCustomPlacementConfig const& lhs,
                       BucketCustomPlacementConfig const& rhs) {
  return std::rel_ops::operator>=(lhs, rhs);
}
//@}

std::ostream& operator<<(std::ostream& os,
                         BucketCustomPlacementConfig const& rhs);

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_BUCKET_CUSTOM_PLACEMENT_CONFIG_H
