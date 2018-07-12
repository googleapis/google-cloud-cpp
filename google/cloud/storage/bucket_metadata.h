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

#include "google/cloud/storage/internal/common_metadata.h"
#include "google/cloud/storage/internal/nljson.h"
#include <map>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
/**
 * Represents a Google Cloud Storage Bucket Metadata object.
 *
 * @warning This is an incomplete implementation to validate the design. It does
 * not support changes to the metadata. It also lacks support for ACLs, CORS,
 * encryption keys, lifecycle rules, and other features.
 *
 * TODO(#537) - complete the implementation.
 */
class BucketMetadata : private internal::CommonMetadata {
 public:
  BucketMetadata() = default;

  static BucketMetadata ParseFromJson(internal::nl::json const& json);
  static BucketMetadata ParseFromString(std::string const& payload);

  std::size_t label_count() const { return labels_.size(); }
  bool has_label(std::string const& key) const {
    return labels_.end() != labels_.find(key);
  }
  std::string const& label(std::string const& key) const {
    return labels_.at(key);
  }
  std::string const& location() const { return location_; }
  std::int64_t const& project_number() const { return project_number_; }

  using CommonMetadata::etag;
  using CommonMetadata::id;
  using CommonMetadata::kind;
  using CommonMetadata::metageneration;
  using CommonMetadata::name;
  using CommonMetadata::self_link;
  using CommonMetadata::storage_class;
  using CommonMetadata::time_created;
  using CommonMetadata::updated;

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
  std::map<std::string, std::string> labels_;
  std::string location_;
  std::int64_t project_number_;
};

std::ostream& operator<<(std::ostream& os, BucketMetadata const& rhs);

}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_BUCKET_METADATA_H_
