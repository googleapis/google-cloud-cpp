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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_NOTIFICATION_EVENT_TYPE_H_
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_NOTIFICATION_EVENT_TYPE_H_

#include "google/cloud/storage/version.h"

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
/**
 * Define functions with the well-know storage classes.
 *
 * These functions are provided to avoid typos when setting or comparing storage
 * classes.
 */
namespace event_type {
inline char const* ObjectFinalize() {
  static constexpr char kEventType[] = "OBJECT_FINALIZE";
  return kEventType;
}

inline char const* ObjectMetadataUpdate() {
  static constexpr char kEventType[] = "OBJECT_METADATA_UPDATE";
  return kEventType;
}

inline char const* ObjectDelete() {
  static constexpr char kEventType[] = "OBJECT_DELETE";
  return kEventType;
}

inline char const* ObjectArchive() {
  static constexpr char kEventType[] = "OBJECT_ARCHIVE";
  return kEventType;
}

}  // namespace event_type
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_NOTIFICATION_EVENT_TYPE_H_
