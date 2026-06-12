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
#include "google/cloud/status.h"
#include "absl/types/optional.h"
#include <chrono>
#include <map>
#include <string>

namespace google {
namespace cloud {
namespace storage {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

/**
 * Represents the payload of a user-defined object context.
 */
struct ObjectCustomContextPayload {
  // The value of the object context.
  std::string value;

  // The time at which the object context was created. Output only.
  std::chrono::system_clock::time_point create_time;

  // The time at which the object context was last updated. Output only.
  std::chrono::system_clock::time_point update_time;
};

inline bool operator==(ObjectCustomContextPayload const& lhs,
                       ObjectCustomContextPayload const& rhs) {
  return lhs.value == rhs.value;
};

inline bool operator!=(ObjectCustomContextPayload const& lhs,
                       ObjectCustomContextPayload const& rhs) {
  return !(lhs == rhs);
}

inline bool operator<(ObjectCustomContextPayload const& lhs,
                      ObjectCustomContextPayload const& rhs) {
  return lhs.value < rhs.value;
}

std::ostream& operator<<(std::ostream& os,
                         ObjectCustomContextPayload const& rhs);

/**
 * Specifies the custom contexts of an object.
 */
class ObjectContexts {
 public:
  bool has_key(std::string const& key) const {
    return custom_.find(key) != custom_.end();
  }

  ObjectContexts& upsert(std::string const& key,
                         ObjectCustomContextPayload const& value) {
    custom_[key] = value;
    return *this;
  }

  bool delete_key(std::string const& key) { return custom_.erase(key) > 0; }

  std::map<std::string, ObjectCustomContextPayload> const& custom() const {
    return custom_;
  }

  friend bool operator==(ObjectContexts const& lhs, ObjectContexts const& rhs) {
    return lhs.custom_ == rhs.custom_;
  }

  friend bool operator!=(ObjectContexts const& lhs, ObjectContexts const& rhs) {
    return !(lhs == rhs);
  }

 private:
  /**
   * Represents the map of user-defined object contexts.
   */
  std::map<std::string, ObjectCustomContextPayload> custom_;
};

std::ostream& operator<<(std::ostream& os, ObjectContexts const& rhs);

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_OBJECT_CONTEXTS_H
