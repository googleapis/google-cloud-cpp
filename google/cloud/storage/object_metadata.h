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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_OBJECT_METADATA_H_
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_OBJECT_METADATA_H_

#include "google/cloud/storage/internal/common_metadata.h"
#include <map>

namespace storage {
inline namespace STORAGE_CLIENT_NS {
/**
 * Represents a Google Cloud Storage Bucket Metadata object.
 *
 * @warning This is an incomplete implementation to validate the design. It does
 * not support changes to the metadata. It also lacks support for ACLs,
 * encryption keys, and other features.
 *
 * TODO(#537) - complete the implementation.
 */
class ObjectMetadata : private internal::CommonMetadata {
 public:
  ObjectMetadata() = default;

  static ObjectMetadata ParseFromJson(std::string const& payload);

  std::size_t label_count() const { return metadata_.size(); }
  bool has_label(std::string const& key) const {
    return metadata_.end() != metadata_.find(key);
  }
  std::string const& label(std::string const& key) const {
    return metadata_.at(key);
  }

  std::string const& bucket() const { return bucket_; }
  std::int64_t generation() const { return generation_; }

  using CommonMetadata::etag;
  using CommonMetadata::id;
  using CommonMetadata::kind;
  using CommonMetadata::metageneration;
  using CommonMetadata::name;
  using CommonMetadata::self_link;
  using CommonMetadata::storage_class;
  using CommonMetadata::time_created;
  using CommonMetadata::updated;

  bool operator==(ObjectMetadata const& rhs) const;
  bool operator!=(ObjectMetadata const& rhs) { return not(*this == rhs); }

 private:
  friend std::ostream& operator<<(std::ostream& os, ObjectMetadata const& rhs);
  // Keep the fields in alphabetical order.
  std::string bucket_;
  std::int64_t generation_;
  std::map<std::string, std::string> metadata_;
};

std::ostream& operator<<(std::ostream& os, ObjectMetadata const& rhs);

}  // namespace STORAGE_CLIENT_NS
}  // namespace storage

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_OBJECT_METADATA_H_
