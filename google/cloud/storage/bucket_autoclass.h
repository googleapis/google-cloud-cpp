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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_BUCKET_AUTOCLASS_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_BUCKET_AUTOCLASS_H

#include "google/cloud/storage/version.h"
#include <chrono>
#include <iosfwd>
#include <tuple>

namespace google {
namespace cloud {
namespace storage {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

/**
 * The autoclass configuration for a Bucket.
 *
 * @par Example
 *
 * @snippet storage_bucket_autoclass_samples.cc get-autoclass
 *
 * @par Example
 *
 * @snippet storage_bucket_autoclass_samples.cc set-autoclass
 */
struct BucketAutoclass {
  explicit BucketAutoclass(bool e) : enabled(e) {}
  explicit BucketAutoclass(bool e, std::chrono::system_clock::time_point tp)
      : enabled(e), toggle_time(tp) {}

  bool enabled;
  std::chrono::system_clock::time_point toggle_time;
};

inline bool operator==(BucketAutoclass const& lhs, BucketAutoclass const& rhs) {
  return std::tie(lhs.enabled, lhs.toggle_time) ==
         std::tie(rhs.enabled, rhs.toggle_time);
}

inline bool operator!=(BucketAutoclass const& lhs, BucketAutoclass const& rhs) {
  return !(lhs == rhs);
}

std::ostream& operator<<(std::ostream& os, BucketAutoclass const& rhs);

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_BUCKET_AUTOCLASS_H
