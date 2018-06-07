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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_COMMON_METADATA_H_
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_COMMON_METADATA_H_

#include "google/cloud/storage/version.h"
#include <chrono>
#include <map>
#include <vector>

namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace internal {
/**
 * Refactor common functionality in BucketMetadata and ObjectMetadata.
 *
 * @warning This is an incomplete implementation to validate the design. It does
 * not support changes to the metadata. It also lacks support for ACLs,
 * optional fields, and many other features.
 *
 * TODO(#537) - complete the implementation.
 */
class CommonMetadata {
 public:
  CommonMetadata() = default;

  static CommonMetadata ParseFromJson(std::string const& payload);

  std::string const& etag() const { return etag_; }
  std::string const& id() const { return id_; }
  std::string const& kind() const { return kind_; }
  std::string const& location() const { return location_; }
  std::int64_t metadata_generation() const { return metadata_generation_; }
  std::string const& name() const { return name_; }
  std::int64_t const& project_number() const { return project_number_; }
  std::string const& self_link() const { return self_link_; }
  std::string const& storage_class() const { return storage_class_; }
  std::chrono::system_clock::time_point time_created() const {
    return time_created_;
  }
  std::chrono::system_clock::time_point time_updated() const {
    return time_updated_;
  }

  bool operator==(CommonMetadata const& rhs) const;
  bool operator!=(CommonMetadata const& rhs) { return not(*this == rhs); }

 private:
  friend struct MetadataParser;

  friend std::ostream& operator<<(std::ostream& os, CommonMetadata const& rhs);
  // Keep the fields in alphabetical order.
  std::string etag_;
  std::string id_;
  std::string kind_;
  std::string location_;
  std::int64_t metadata_generation_;
  std::string name_;
  std::int64_t project_number_;
  std::string self_link_;
  std::string storage_class_;
  std::chrono::system_clock::time_point time_created_;
  std::chrono::system_clock::time_point time_updated_;
};

std::ostream& operator<<(std::ostream& os, CommonMetadata const& rhs);

}  // namespace internal
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_COMMON_METADATA_H_
