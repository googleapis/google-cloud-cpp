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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_COMMON_METADATA_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_COMMON_METADATA_H

#include "google/cloud/storage/version.h"
#include "google/cloud/status_or.h"
#include "absl/types/optional.h"
#include <chrono>
#include <map>
#include <utility>
#include <vector>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
/// A simple wrapper for the `owner` field in `internal::CommonMetadata`.
struct Owner {
  std::string entity;
  std::string entity_id;
};

inline bool operator==(Owner const& lhs, Owner const& rhs) {
  return std::tie(lhs.entity, lhs.entity_id) ==
         std::tie(rhs.entity, rhs.entity_id);
}

inline bool operator<(Owner const& lhs, Owner const& rhs) {
  return std::tie(lhs.entity, lhs.entity_id) <
         std::tie(rhs.entity, rhs.entity_id);
}

inline bool operator!=(Owner const& lhs, Owner const& rhs) {
  return std::rel_ops::operator!=(lhs, rhs);
}

inline bool operator>(Owner const& lhs, Owner const& rhs) {
  return std::rel_ops::operator>(lhs, rhs);
}

inline bool operator<=(Owner const& lhs, Owner const& rhs) {
  return std::rel_ops::operator<=(lhs, rhs);
}

inline bool operator>=(Owner const& lhs, Owner const& rhs) {
  return std::rel_ops::operator>=(lhs, rhs);
}

namespace internal {
class GrpcClient;
template <typename Derived>
struct CommonMetadataParser;

/**
 * Defines common attributes to both `BucketMetadata` and `ObjectMetadata`.
 *
 * @tparam Derived a class derived from CommonMetadata<Derived>. This class uses
 *     the Curiously recurring template pattern.
 *
 * @see https://en.wikipedia.org/wiki/Curiously_recurring_template_pattern
 */
template <typename Derived>
class CommonMetadata {
 public:
  CommonMetadata() = default;

  std::string const& etag() const { return etag_; }
  std::string const& id() const { return id_; }
  std::string const& kind() const { return kind_; }
  std::int64_t metageneration() const { return metageneration_; }

  std::string const& name() const { return name_; }
  // NOLINTNEXTLINE(performance-unnecessary-value-param)  TODO(#4112)
  void set_name(std::string value) { name_ = std::move(value); }

  bool has_owner() const { return owner_.has_value(); }
  Owner const& owner() const { return owner_.value(); }

  std::string const& self_link() const { return self_link_; }

  std::string const& storage_class() const { return storage_class_; }
  // NOLINTNEXTLINE(performance-unnecessary-value-param)  TODO(#4112)
  void set_storage_class(std::string value) {
    storage_class_ = std::move(value);
  }

  std::chrono::system_clock::time_point time_created() const {
    return time_created_;
  }
  std::chrono::system_clock::time_point updated() const { return updated_; }

 private:
  friend class GrpcClient;
  template <typename ParserDerived>
  friend struct CommonMetadataParser;

  // Keep the fields in alphabetical order.
  std::string etag_;
  std::string id_;
  std::string kind_;
  std::int64_t metageneration_{0};
  std::string name_;
  absl::optional<Owner> owner_;
  std::string self_link_;
  std::string storage_class_;
  std::chrono::system_clock::time_point time_created_;
  std::chrono::system_clock::time_point updated_;
};

template <typename T>
inline bool operator==(CommonMetadata<T> const& lhs,
                       CommonMetadata<T> const& rhs) {
  // etag changes each time the metadata changes, so that is the best field
  // to short-circuit this comparison.  The check the name, project number,
  // and metadata generation, which have the next best chance to
  // short-circuit.  The rest just put in alphabetical order.
  return lhs.name() == rhs.name() &&
         lhs.metageneration() == rhs.metageneration() && lhs.id() == rhs.id() &&
         lhs.etag() == rhs.etag() && lhs.kind() == rhs.kind() &&
         lhs.self_link() == rhs.self_link() &&
         lhs.storage_class() == rhs.storage_class() &&
         lhs.time_created() == rhs.time_created() &&
         lhs.updated() == rhs.updated() && lhs.has_owner() == rhs.has_owner() &&
         (!lhs.has_owner() || lhs.owner() == rhs.owner());
}

template <typename T>
inline bool operator!=(CommonMetadata<T> const& lhs,
                       CommonMetadata<T> const& rhs) {
  return std::rel_ops::operator!=(lhs, rhs);
}

}  // namespace internal
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_COMMON_METADATA_H
