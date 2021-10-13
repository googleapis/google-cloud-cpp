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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_OBJECT_METADATA_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_OBJECT_METADATA_H

#include "google/cloud/storage/internal/common_metadata.h"
#include "google/cloud/storage/internal/complex_option.h"
#include "google/cloud/storage/object_access_control.h"
#include "google/cloud/storage/version.h"
#include "google/cloud/optional.h"
#include "google/cloud/status_or.h"
#include "absl/types/optional.h"
#include <chrono>
#include <map>
#include <string>
#include <vector>

namespace google {
namespace cloud {
namespace storage {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {
struct ObjectMetadataParser;
class GrpcClient;
}  // namespace internal

/// A simple representation for the customerEncryption field.
struct CustomerEncryption {
  // The encryption algorithm name.
  std::string encryption_algorithm;
  // The SHA256 hash of the encryption key.
  std::string key_sha256;
};

/// Defines one of the source objects for a compose operation.
struct ComposeSourceObject {
  std::string object_name;
  absl::optional<std::int64_t> generation;
  absl::optional<std::int64_t> if_generation_match;
};

std::ostream& operator<<(std::ostream& os, ComposeSourceObject const& r);

inline bool operator==(CustomerEncryption const& lhs,
                       CustomerEncryption const& rhs) {
  return std::tie(lhs.encryption_algorithm, lhs.key_sha256) ==
         std::tie(rhs.encryption_algorithm, rhs.key_sha256);
}

inline bool operator<(CustomerEncryption const& lhs,
                      CustomerEncryption const& rhs) {
  return std::tie(lhs.encryption_algorithm, lhs.key_sha256) <
         std::tie(rhs.encryption_algorithm, rhs.key_sha256);
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
  ObjectMetadata() = default;

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

  std::int32_t component_count() const { return component_count_; }

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

  bool event_based_hold() const { return event_based_hold_; }
  ObjectMetadata& set_event_based_hold(bool v) {
    event_based_hold_ = v;
    return *this;
  }

  std::int64_t generation() const { return generation_; }

  using CommonMetadata::id;
  using CommonMetadata::kind;

  std::string const& kms_key_name() const { return kms_key_name_; }
  std::string const& md5_hash() const { return md5_hash_; }
  std::string const& media_link() const { return media_link_; }

  //@{
  /// @name Accessors and modifiers to the `metadata` labels.
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
    if (i == metadata_.end() || i->first != key) {
      metadata_.emplace_hint(i, std::move(key), std::move(value));
    } else {
      i->second = std::move(value);
    }
    return *this;
  }

  std::map<std::string, std::string> const& metadata() const {
    return metadata_;
  }
  std::map<std::string, std::string>& mutable_metadata() { return metadata_; }
  //@}

  using CommonMetadata::has_owner;
  using CommonMetadata::metageneration;
  using CommonMetadata::name;
  using CommonMetadata::owner;

  std::chrono::system_clock::time_point retention_expiration_time() const {
    return retention_expiration_time_;
  }

  using CommonMetadata::self_link;

  std::uint64_t size() const { return size_; }

  using CommonMetadata::storage_class;
  ObjectMetadata& set_storage_class(std::string v) {
    CommonMetadata::set_storage_class(std::move(v));
    return *this;
  }

  bool temporary_hold() const { return temporary_hold_; }
  ObjectMetadata& set_temporary_hold(bool v) {
    temporary_hold_ = v;
    return *this;
  }

  using CommonMetadata::time_created;

  std::chrono::system_clock::time_point time_deleted() const {
    return time_deleted_;
  }
  std::chrono::system_clock::time_point time_storage_class_updated() const {
    return time_storage_class_updated_;
  }

  using CommonMetadata::updated;

  bool has_custom_time() const { return custom_time_.has_value(); }
  std::chrono::system_clock::time_point custom_time() const {
    return custom_time_.value_or(std::chrono::system_clock::time_point{});
  }
  ObjectMetadata& set_custom_time(std::chrono::system_clock::time_point v) {
    custom_time_ = v;
    return *this;
  }
  ObjectMetadata& reset_custom_time() {
    custom_time_.reset();
    return *this;
  }

  friend bool operator==(ObjectMetadata const& lhs, ObjectMetadata const& rhs);
  friend bool operator!=(ObjectMetadata const& lhs, ObjectMetadata const& rhs) {
    return !(lhs == rhs);
  }

 private:
  friend struct internal::ObjectMetadataParser;
  friend class internal::GrpcClient;

  friend std::ostream& operator<<(std::ostream& os, ObjectMetadata const& rhs);
  // Keep the fields in alphabetical order.
  std::vector<ObjectAccessControl> acl_;
  std::string bucket_;
  std::string cache_control_;
  std::int32_t component_count_{0};
  std::string content_disposition_;
  std::string content_encoding_;
  std::string content_language_;
  std::string content_type_;
  std::string crc32c_;
  absl::optional<CustomerEncryption> customer_encryption_;
  bool event_based_hold_{false};
  std::int64_t generation_{0};
  std::string kms_key_name_;
  std::string md5_hash_;
  std::string media_link_;
  std::map<std::string, std::string> metadata_;
  std::chrono::system_clock::time_point retention_expiration_time_;
  std::uint64_t size_{0};
  bool temporary_hold_{false};
  std::chrono::system_clock::time_point time_deleted_;
  std::chrono::system_clock::time_point time_storage_class_updated_;
  absl::optional<std::chrono::system_clock::time_point> custom_time_;
};

std::ostream& operator<<(std::ostream& os, ObjectMetadata const& rhs);

/**
 * Prepares a patch for the Bucket resource.
 *
 * The Bucket resource has many modifiable fields. The application may send a
 * patch request to change (or delete) a small fraction of these fields by using
 * this object.
 *
 * @see
 * https://cloud.google.com/storage/docs/json_api/v1/how-tos/performance#patch
 *     for general information on PATCH requests for the Google Cloud Storage
 *     JSON API.
 */
class ObjectMetadataPatchBuilder {
 public:
  ObjectMetadataPatchBuilder() = default;

  std::string BuildPatch() const;

  ObjectMetadataPatchBuilder& SetAcl(std::vector<ObjectAccessControl> const& v);

  /**
   * Clears the ACL.
   *
   * @warning Currently the server ignores requests to reset the full ACL.
   */
  ObjectMetadataPatchBuilder& ResetAcl();

  ObjectMetadataPatchBuilder& SetCacheControl(std::string const& v);
  ObjectMetadataPatchBuilder& ResetCacheControl();
  ObjectMetadataPatchBuilder& SetContentDisposition(std::string const& v);
  ObjectMetadataPatchBuilder& ResetContentDisposition();
  ObjectMetadataPatchBuilder& SetContentEncoding(std::string const& v);
  ObjectMetadataPatchBuilder& ResetContentEncoding();
  ObjectMetadataPatchBuilder& SetContentLanguage(std::string const& v);
  ObjectMetadataPatchBuilder& ResetContentLanguage();
  ObjectMetadataPatchBuilder& SetContentType(std::string const& v);
  ObjectMetadataPatchBuilder& ResetContentType();
  ObjectMetadataPatchBuilder& SetEventBasedHold(bool v);
  ObjectMetadataPatchBuilder& ResetEventBasedHold();

  ObjectMetadataPatchBuilder& SetMetadata(std::string const& key,
                                          std::string const& value);
  ObjectMetadataPatchBuilder& ResetMetadata(std::string const& key);
  ObjectMetadataPatchBuilder& ResetMetadata();

  ObjectMetadataPatchBuilder& SetTemporaryHold(bool v);
  ObjectMetadataPatchBuilder& ResetTemporaryHold();

  /**
   * Change the `custom_time` field.
   *
   * @par Example
   * @snippet storage_object_samples.cc object custom time
   */
  ObjectMetadataPatchBuilder& SetCustomTime(
      std::chrono::system_clock::time_point tp);
  ObjectMetadataPatchBuilder& ResetCustomTime();

 private:
  internal::PatchBuilder impl_;
  bool metadata_subpatch_dirty_{false};
  internal::PatchBuilder metadata_subpatch_;
};

/**
 * A request option to define the object metadata attributes.
 */
struct WithObjectMetadata
    : public internal::ComplexOption<WithObjectMetadata, ObjectMetadata> {
  using ComplexOption<WithObjectMetadata, ObjectMetadata>::ComplexOption;
  // GCC <= 7.0 does not use the inherited default constructor, redeclare it
  // explicitly
  WithObjectMetadata() = default;
  static char const* name() { return "object-metadata"; }
};

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_OBJECT_METADATA_H
