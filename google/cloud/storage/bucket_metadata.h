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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_BUCKET_METADATA_H_
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_BUCKET_METADATA_H_

#include "google/cloud/storage/version.h"
#include <chrono>
#include <map>
#include <vector>

namespace storage {
inline namespace STORAGE_CLIENT_NS {
/**
 * Represents a Google Cloud Storage Bucket Metadata object.
 *
 * @warning This is an incomplete implementation to validate the design. It does
 * not support changes to the metadata. It also lacks support for ACLs, CORS,
 * encryption keys, lifecycle rules, and other features.
 * TODO(#537) - complete the implementation.
 */
class BucketMetadata {
 public:
  BucketMetadata() = default;

  static BucketMetadata ParseFromJson(std::string const& payload);

  std::string const& etag() const { return etag_; }
  std::string const& id() const { return id_; }
  std::string const& kind() const { return kind_; }
  std::size_t label_count() const { return labels_.size(); }
  bool has_label(std::string const& key) const {
    return labels_.end() != labels_.find(key);
  }
  std::string const& label(std::string const& key) const {
    return labels_.at(key);
  }
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

  bool operator==(BucketMetadata const& rhs) const;
  bool operator!=(BucketMetadata const& rhs) { return not(*this == rhs); }

  constexpr static char STORAGE_CLASS_STANDARD[] = "STANDARD";
  constexpr static char STORAGE_CLASS_MULTI_REGIONAL[] = "MULTI_REGIONAL";
  constexpr static char STORAGE_CLASS_REGIONAL[] = "REGIONAL";
  constexpr static char STORAGE_CLASS_NEARLINE[] = "NEARLINE";
  constexpr static char STORAGE_CLASS_COLDLINE[] = "COLDLINE";
  constexpr static char STORAGE_CLASS_DURABLE_REDUCED_AVAILABILITY[] =
      "DURABLE_REDUCED_AVAILABILITY";

 private:
  friend std::ostream& operator<<(std::ostream& os, BucketMetadata const& rhs);
  // Keep the fields in alphabetical order.
  std::string etag_;
  std::string id_;
  std::string kind_;
  std::map<std::string, std::string> labels_;
  std::string location_;
  std::int64_t metadata_generation_;
  std::string name_;
  std::int64_t project_number_;
  std::string self_link_;
  std::string storage_class_;
  std::chrono::system_clock::time_point time_created_;
  std::chrono::system_clock::time_point time_updated_;
};

std::ostream& operator<<(std::ostream& os, BucketMetadata const& rhs);

}  // namespace STORAGE_CLIENT_NS
}  // namespace storage

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_BUCKET_METADATA_H_
