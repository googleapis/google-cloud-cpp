// Copyright 2018 Google LLC
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
struct GrpcObjectMetadataParser;
struct GrpcObjectRequestParser;
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
 *
 * Note that all modifiers just change the local representation of the Object's
 * metadata.  Applications should use `Client::PatchObject()`, or a similar
 * operation, to actually modify the metadata stored by GCS.
 *
 * @see https://cloud.google.com/storage/docs/json_api/v1/objects for a more
 *     detailed description of each attribute and their effects.
 */
class ObjectMetadata : private internal::CommonMetadata<ObjectMetadata> {
 public:
  ObjectMetadata() = default;

  // Please keep these in alphabetical order, that make it easier to verify we
  // have actually implemented all of them.

  /// The access control list for this object.
  std::vector<ObjectAccessControl> const& acl() const { return acl_; }

  /// The access control list for this object.
  std::vector<ObjectAccessControl>& mutable_acl() { return acl_; }

  /// Change the access control list.
  ObjectMetadata& set_acl(std::vector<ObjectAccessControl> acl) {
    acl_ = std::move(acl);
    return *this;
  }

  /// The name of the bucket containing this object.
  std::string const& bucket() const { return bucket_; }

  /// The cacheControl attribute.
  std::string const& cache_control() const { return cache_control_; }

  /// Set the cacheControl attributes.
  ObjectMetadata& set_cache_control(std::string cache_control) {
    cache_control_ = std::move(cache_control);
    return *this;
  }

  /// The number of components, for objects built using `ComposeObject()`.
  std::int32_t component_count() const { return component_count_; }

  /// The `contentDisposition` attribute.
  std::string content_disposition() const { return content_disposition_; }

  /// Change the `contentDisposition` attribute.
  ObjectMetadata& set_content_disposition(std::string value) {
    content_disposition_ = std::move(value);
    return *this;
  }

  /// The `contentEncoding` attribute.
  std::string content_encoding() const { return content_encoding_; }

  /// Change the `contentEncoding` attribute.
  ObjectMetadata& set_content_encoding(std::string value) {
    content_encoding_ = std::move(value);
    return *this;
  }

  /// The `contentLanguage` attribute.
  std::string content_language() const { return content_language_; }

  /// Change the `contentLanguage` attribute.
  ObjectMetadata& set_content_language(std::string value) {
    content_language_ = std::move(value);
    return *this;
  }

  /// The `contentType` attribute.
  std::string content_type() const { return content_type_; }

  /// Change the `contentLanguage` attribute.
  ObjectMetadata& set_content_type(std::string value) {
    content_type_ = std::move(value);
    return *this;
  }

  /// The `CRC32C` checksum for the object contents.
  std::string const& crc32c() const { return crc32c_; }

  /// Returns `true` if the object uses CSEK (Customer-Supplied Encryption
  /// Keys).
  bool has_customer_encryption() const {
    return customer_encryption_.has_value();
  }

  /**
   * Returns the CSEK metadata (algorithm and key SHA256).
   *
   * It is undefined behavior to call this member function if
   * `has_customer_encryption() == false`.
   */
  CustomerEncryption const& customer_encryption() const {
    return customer_encryption_.value();
  }

  /// The `Etag` attribute.
  using CommonMetadata::etag;

  /// The `eventBasedHold` attribute.
  bool event_based_hold() const { return event_based_hold_; }

  /// Changes the `eventBasedHold` attribute.
  ObjectMetadata& set_event_based_hold(bool v) {
    event_based_hold_ = v;
    return *this;
  }

  /**
   * The object generation.
   *
   * In buckets with object versioning enabled, each object may have multiple
   * generations. Each generation data (the object contents) is immutable, but
   * the metadata associated with each generation can be changed.
   */
  std::int64_t generation() const { return generation_; }

  /// The `id` attribute (the object name)
  using CommonMetadata::id;

  /// The `kind` attribute, that is, `storage#object`.
  using CommonMetadata::kind;

  /**
   * The name of the KMS (Key Management Service) key used in this object.
   *
   * This is empty for objects not using CMEK (Customer Managed Encryption
   * Keys).
   */
  std::string const& kms_key_name() const { return kms_key_name_; }

  /// The MD5 hash of the object contents. Can be empty.
  std::string const& md5_hash() const { return md5_hash_; }

  /// The HTTPS link to access the object contents.
  std::string const& media_link() const { return media_link_; }

  /**
   * @name Accessors and modifiers for metadata metadata.
   *
   * The object metadata contains a user-defined set of `key`, `value` pairs,
   * which are also called "metadata". Applications can use these fields to
   * add custom annotations to each object.
   */
  ///@{
  /// Returns `true` if the key is present in the object metadata entries.
  bool has_metadata(std::string const& key) const {
    return metadata_.end() != metadata_.find(key);
  }

  /**
   * Returns the value of @p key in the Object's metadata entries.
   *
   * It is undefined behavior to call `metadata(key)` if `has_metadata(key) ==
   * false`.
   */
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

  /// Returns all the Object's metadata entries,
  std::map<std::string, std::string> const& metadata() const {
    return metadata_;
  }

  /// Returns all the Object's metadata entries.
  std::map<std::string, std::string>& mutable_metadata() { return metadata_; }
  ///@}

  /// Returns `true` if the object has a `owner` attributes.
  using CommonMetadata::has_owner;

  /**
   * The generation of the object metadata.
   *
   * Note that changes to the object metadata (e.g. changing the `cacheControl`
   * attribute) increases the metageneration, but does not change the object
   * generation.
   */
  using CommonMetadata::metageneration;

  /// The object name, including bucket and generation.
  using CommonMetadata::name;

  /**
   * The object's owner attributes.
   *
   * It is undefined behavior to call this member function if
   * `has_owner() == false`.
   */
  using CommonMetadata::owner;

  /// The retention expiration time, or the system clock's epoch, if not set.
  std::chrono::system_clock::time_point retention_expiration_time() const {
    return retention_expiration_time_;
  }

  /// An HTTPS link to the object metadata.
  using CommonMetadata::self_link;

  /// The size of the object's data.
  std::uint64_t size() const { return size_; }

  /// The `storageClass` attribute.
  using CommonMetadata::storage_class;

  /// Changes the `storageClass` attribute.
  ObjectMetadata& set_storage_class(std::string v) {
    CommonMetadata::set_storage_class(std::move(v));
    return *this;
  }

  /// The `temporaryHold` attribute.
  bool temporary_hold() const { return temporary_hold_; }

  /// Changes the `temporaryHold` attribute.
  ObjectMetadata& set_temporary_hold(bool v) {
    temporary_hold_ = v;
    return *this;
  }

  /// The object creation timestamp.
  using CommonMetadata::time_created;

  /// The object's deletion timestamp.
  std::chrono::system_clock::time_point time_deleted() const {
    return time_deleted_;
  }

  /// The timestamp for the last storage class change.
  std::chrono::system_clock::time_point time_storage_class_updated() const {
    return time_storage_class_updated_;
  }

  /// The timestamp for the last object *metadata* update.
  using CommonMetadata::updated;

  /// Returns `true` if the object has a `customTime` attribute.
  bool has_custom_time() const { return custom_time_.has_value(); }

  /// Returns the object's `customTime` or the system clock's epoch.
  std::chrono::system_clock::time_point custom_time() const {
    return custom_time_.value_or(std::chrono::system_clock::time_point{});
  }

  /// Changes the `customTime` attribute.
  ObjectMetadata& set_custom_time(std::chrono::system_clock::time_point v) {
    custom_time_ = v;
    return *this;
  }

  /// Reset (clears) the `customTime` attribute. `has_custom_time()` returns
  /// `false` after calling this function.
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
  friend struct internal::GrpcObjectMetadataParser;

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
  friend struct internal::GrpcObjectRequestParser;

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
