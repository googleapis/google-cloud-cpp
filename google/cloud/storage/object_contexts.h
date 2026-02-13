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
#include "absl/types/optional.h"
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
 public:
  // Returns true if the map itself exists.
  bool has_custom() const { return custom_map_.has_value(); }

  /**
   * Returns true if the map exists AND the key is present AND the value is
   * a valid value.
   */
  bool has_custom(std::string const& key) const {
    if (!has_custom()) {
      return false;
    }
    return custom_map_->find(key) != custom_map_->end() &&
           custom_map_->at(key).has_value();
  }

  /**
   * The `custom` attribute of the object contexts.
   * Values are now absl::optional.
   *
   * It is undefined behavior to call this member function if
   * `has_custom() == false`.
   */
  std::map<std::string, absl::optional<ObjectCustomContextPayload>> const&
  custom() const {
    return *custom_map_;
  }

  /**
   * Upserts a context. Passing absl::nullopt for the value
   * represents a "null" entry in the map.
   */
  void upsert_custom_context(std::string const& key,
                             absl::optional<ObjectCustomContextPayload> value) {
    if (!has_custom()) {
      custom_map_.emplace();
    }

    (*custom_map_)[key] = std::move(value);
  }

  void reset_custom() { custom_map_.reset(); }

  bool operator==(ObjectContexts const& other) const {
    return custom_map_ == other.custom_map_;
  }

  bool operator!=(ObjectContexts const& other) const {
    return !(*this == other);
  }

 private:
  /**
   * Represents the map of user-defined object contexts.
   * Inner optional allows keys to point to a "null" value.
   */
  absl::optional<
      std::map<std::string, absl::optional<ObjectCustomContextPayload>>>
      custom_map_;
};

std::ostream& operator<<(std::ostream& os, ObjectContexts const& rhs);

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_OBJECT_CONTEXTS_H
