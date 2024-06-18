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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_OBJECT_RETENTION_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_OBJECT_RETENTION_H

#include "google/cloud/storage/version.h"
#include <chrono>
#include <iosfwd>
#include <string>
#include <tuple>

namespace google {
namespace cloud {
namespace storage {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

/**
 * Specifies the retention parameters of an object.
 *
 * Objects under retention cannot be deleted or overwritten until their
 * retention expires. Objects with a "Locked" retention cannot have their
 * retention period decreased or incce. The soft delete policy prevents
 * soft-deleted objects from being permanently deleted.
 */
struct ObjectRetention {
  std::string mode;
  std::chrono::system_clock::time_point retain_until_time;
};

/// A helper function to avoid typos in the ObjectRetention configuration
inline std::string ObjectRetentionUnlocked() { return "Unlocked"; }

/// A helper function to avoid typos in the ObjectRetention configuration
inline std::string ObjectRetentionLocked() { return "Locked"; }

inline bool operator==(ObjectRetention const& lhs, ObjectRetention const& rhs) {
  return std::tie(lhs.mode, lhs.retain_until_time) ==
         std::tie(rhs.mode, rhs.retain_until_time);
}

inline bool operator!=(ObjectRetention const& lhs, ObjectRetention const& rhs) {
  return !(lhs == rhs);
}

std::ostream& operator<<(std::ostream& os, ObjectRetention const& rhs);

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_OBJECT_RETENTION_H
