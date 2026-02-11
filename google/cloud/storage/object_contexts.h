// Copyright 2026 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_OBJECT_CONTEXTS_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_OBJECT_CONTEXTS_H

#include "google/cloud/storage/version.h"
#include <chrono>
#include <map>
#include <string>

namespace google {
namespace cloud {
namespace storage {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

/**
 *  Represents the payload of a user-defined object context.
 */
struct ObjectCustomContextPayload {
  std::string value;

  std::chrono::system_clock::time_point create_time;

  std::chrono::system_clock::time_point update_time;
};

inline bool operator==(ObjectCustomContextPayload const& lhs,
                       ObjectCustomContextPayload const& rhs) {
  return std::tie(lhs.value, lhs.create_time, lhs.update_time) ==
         std::tie(rhs.value, rhs.create_time, rhs.update_time);
};

inline bool operator!=(ObjectCustomContextPayload const& lhs,
                       ObjectCustomContextPayload const& rhs) {
  return !(lhs == rhs);
}

std::ostream& operator<<(std::ostream& os,
                         ObjectCustomContextPayload const& rhs);

/**
 * Specifies the custom contexts of an object.
 */
struct ObjectContexts {
  /**
   * Represents the map of user-defined object contexts, keyed by a string
   * value.
   */
  std::map<std::string, ObjectCustomContextPayload> custom;

  /**
   * A set of helper functions to handle the custom.
   */
  bool has_custom(std::string const& key) const {
    return custom.end() != custom.find(key);
  }
  ObjectCustomContextPayload const& get_custom(std::string const& key) const {
    return custom.at(key);
  }
  void upsert_custom(std::string const& key,
                     ObjectCustomContextPayload const& value) {
    custom[key] = value;
  }
  void delete_custom(std::string const& key) { custom.erase(key); }
};

inline bool operator==(ObjectContexts const& lhs, ObjectContexts const& rhs) {
  return lhs.custom == rhs.custom;
};

inline bool operator!=(ObjectContexts const& lhs, ObjectContexts const& rhs) {
  return !(lhs == rhs);
}

std::ostream& operator<<(std::ostream& os, ObjectContexts const& rhs);

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_OBJECT_CONTEXTS_H
