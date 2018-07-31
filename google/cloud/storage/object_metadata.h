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
#include "google/cloud/storage/object_access_control.h"
#include <map>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
/// A simple representation for the customerEncryption field.
struct CustomerEncryption {
  // The encryption algorithm name.
  std::string encryption_algorithm;
  // The SHA256 hash of the encryption key.
  std::string key_sha256;
};

inline bool operator==(CustomerEncryption const& lhs,
                       CustomerEncryption const& rhs) {
  return std::tie(lhs.encryption_algorithm, lhs.key_sha256) ==
         std::tie(rhs.encryption_algorithm, lhs.key_sha256);
}

inline bool operator<(CustomerEncryption const& lhs,
                      CustomerEncryption const& rhs) {
  return std::tie(lhs.encryption_algorithm, lhs.key_sha256) <
         std::tie(rhs.encryption_algorithm, lhs.key_sha256);
}

inline bool operator!=(CustomerEncryption const& lhs,
                       CustomerEncryption const& rhs) {
  return std::rel_ops::operator!=(lhs, rhs);
}

inline bool operator>(CustomerEncryption const& lhs,
                      CustomerEncryption const& rhs) {
  return std::rel_ops::operator>(lhs, rhs);
}

inline bool operator<=(CustomerEncryption const& lhs,
                       CustomerEncryption const& rhs) {
  return std::rel_ops::operator<=(lhs, rhs);
}

inline bool operator>=(CustomerEncryption const& lhs,
                       CustomerEncryption const& rhs) {
  return std::rel_ops::operator>=(lhs, rhs);
}

/**
 * Represents the metadata for a Google Cloud Storage Object.
 */
class ObjectMetadata : private internal::CommonMetadata<ObjectMetadata> {
 public:
  ObjectMetadata() : component_count_(0), generation_(0), size_(0) {}

  static ObjectMetadata ParseFromJson(internal::nl::json const& json);
  static ObjectMetadata ParseFromString(std::string const& payload);

  // Please keep these in alphabetical order, that make it easier to verify we
  // have actually implemented all of them.
  std::vector<ObjectAccessControl> const& acl() const { return acl_; }
  std::vector<ObjectAccessControl>& mutable_acl() { return acl_; }
  ObjectMetadata& set_acl(std::vector<ObjectAccessControl> acl) {
    acl_ = std::move(acl);
    return *this;
  }

  std::string const& bucket() const { return bucket_; }

  std::string const& cache_control() const { return cache_control_; }
  ObjectMetadata& set_cache_control(std::string cache_control) {
    cache_control_ = std::move(cache_control);
    return *this;
  }

  long component_count() const { return component_count_; }

  std::string content_disposition() const { return content_disposition_; }
  ObjectMetadata& set_content_disposition(std::string value) {
    content_disposition_ = std::move(value);
    return *this;
  }

  std::string content_encoding() const { return content_encoding_; }
  ObjectMetadata& set_content_encoding(std::string value) {
    content_encoding_ = std::move(value);
    return *this;
  }

  std::string content_language() const { return content_language_; }
  ObjectMetadata& set_content_language(std::string value) {
    content_language_ = std::move(value);
    return *this;
  }

  std::string content_type() const { return content_type_; }
  ObjectMetadata& set_content_type(std::string value) {
    content_type_ = std::move(value);
    return *this;
  }

  std::string const& crc32c() const { return crc32c_; }

  bool has_customer_encryption() const {
    return customer_encryption_.has_value();
  }
  CustomerEncryption const& customer_encryption() const {
    return customer_encryption_.value();
  }

  using CommonMetadata::etag;

  std::int64_t generation() const { return generation_; }

  using CommonMetadata::id;
  using CommonMetadata::kind;

  std::string const& kms_key_name() const { return kms_key_name_; }
  std::string const& md5_hash() const { return md5_hash_; }
  std::string const& media_link() const { return media_link_; }

  //@{
  /// @name Accessors and modifiers to the `metadata` labels.
  std::size_t metadata_count() const { return metadata_.size(); }
  bool has_metadata(std::string const& key) const {
    return metadata_.end() != metadata_.find(key);
  }

  std::string const& metadata(std::string const& key) const {
    return metadata_.at(key);
  }

  /// Delete a metadata entry. This is a no-op if the key does not exist.
  ObjectMetadata& delete_metadata(std::string const& key) {
    auto i = metadata_.find(key);
    if (i == metadata_.end()) {
      return *this;
    }
    metadata_.erase(i);
    return *this;
  }

  /// Insert or update the metadata entry.
  ObjectMetadata& upsert_metadata(std::string key, std::string value) {
    auto i = metadata_.lower_bound(key);
    if (i == metadata_.end() or i->first != key) {
      metadata_.emplace_hint(i, std::move(key), std::move(value));
    } else {
      i->second = std::move(value);
    }
    return *this;
  }
  //@}

  using CommonMetadata::metageneration;
  using CommonMetadata::name;
  using CommonMetadata::owner;
  using CommonMetadata::self_link;

  std::uint64_t size() const { return size_; }

  using CommonMetadata::storage_class;
  using CommonMetadata::time_created;

  std::chrono::system_clock::time_point time_deleted() const {
    return time_deleted_;
  }
  std::chrono::system_clock::time_point time_storage_class_updated() const {
    return time_storage_class_updated_;
  }

  using CommonMetadata::updated;

  bool operator==(ObjectMetadata const& rhs) const;
  bool operator!=(ObjectMetadata const& rhs) const { return not(*this == rhs); }

 private:
  friend std::ostream& operator<<(std::ostream& os, ObjectMetadata const& rhs);
  // Keep the fields in alphabetical order.
  std::vector<ObjectAccessControl> acl_;
  std::string bucket_;
  std::string cache_control_;
  std::int64_t component_count_;
  std::string content_disposition_;
  std::string content_encoding_;
  std::string content_language_;
  std::string content_type_;
  std::string crc32c_;
  google::cloud::internal::optional<CustomerEncryption> customer_encryption_;
  std::int64_t generation_;
  std::string kms_key_name_;
  std::string md5_hash_;
  std::string media_link_;
  std::map<std::string, std::string> metadata_;
  std::uint64_t size_;
  std::chrono::system_clock::time_point time_deleted_;
  std::chrono::system_clock::time_point time_storage_class_updated_;
};

std::ostream& operator<<(std::ostream& os, ObjectMetadata const& rhs);

}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_OBJECT_METADATA_H_
