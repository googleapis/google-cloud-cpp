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

#include "google/cloud/storage/internal/complex_option.h"
#include "google/cloud/storage/object_access_control.h"
#include "google/cloud/storage/owner.h"
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
class ObjectMetadata {
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

  /// @note This is only intended for mocking.
  ObjectMetadata& set_bucket(std::string v) {
    bucket_ = std::move(v);
    return *this;
  }

  /// The `cacheControl` attribute.
  std::string const& cache_control() const { return cache_control_; }

  /// Set the `cacheControl` attribute.
  ObjectMetadata& set_cache_control(std::string cache_control) {
    cache_control_ = std::move(cache_control);
    return *this;
  }

  /// The number of components, for objects built using `ComposeObject()`.
  std::int32_t component_count() const { return component_count_; }

  /// @note This is only intended for mocking.
  ObjectMetadata& set_component_count(std::int32_t v) {
    component_count_ = v;
    return *this;
  }

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

  /// @note This is only intended for mocking.
  ObjectMetadata& set_crc32c(std::string v) {
    crc32c_ = std::move(v);
    return *this;
  }

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

  /// @note This is only intended for mocking.
  ObjectMetadata& set_customer_encryption(CustomerEncryption v) {
    customer_encryption_ = std::move(v);
    return *this;
  }

  /// @note This is only intended for mocking.
  ObjectMetadata& reset_customer_encryption() {
    customer_encryption_.reset();
    return *this;
  }

  /// The `Etag` attribute.
  std::string const& etag() const { return etag_; }

  /// @note This is only intended for mocking.
  ObjectMetadata& set_etag(std::string v) {
    etag_ = std::move(v);
    return *this;
  }

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

  /// @note This is only intended for mocking.
  ObjectMetadata& set_generation(std::int64_t v) {
    generation_ = v;
    return *this;
  }

  /// The `id` attribute (the object name)
  std::string const& id() const { return id_; }

  /// @note This is only intended for mocking.
  ObjectMetadata& set_id(std::string v) {
    id_ = std::move(v);
    return *this;
  }

  /// The `kind` attribute, that is, `storage#object`.
  std::string const& kind() const { return kind_; }

  /// @note This is only intended for mocking.
  ObjectMetadata& set_kind(std::string v) {
    kind_ = std::move(v);
    return *this;
  }

  /**
   * The name of the KMS (Key Management Service) key used in this object.
   *
   * This is empty for objects not using CMEK (Customer Managed Encryption
   * Keys).
   */
  std::string const& kms_key_name() const { return kms_key_name_; }

  /// @note This is only intended for mocking.
  ObjectMetadata& set_kms_key_name(std::string v) {
    kms_key_name_ = std::move(v);
    return *this;
  }

  /// The MD5 hash of the object contents. Can be empty.
  std::string const& md5_hash() const { return md5_hash_; }

  /// @note This is only intended for mocking.
  ObjectMetadata& set_md5_hash(std::string v) {
    md5_hash_ = std::move(v);
    return *this;
  }

  /// The HTTPS link to access the object contents.
  std::string const& media_link() const { return media_link_; }

  /// @note This is only intended for mocking.
  ObjectMetadata& set_media_link(std::string v) {
    media_link_ = std::move(v);
    return *this;
  }

  /**
   * @name Accessors and modifiers for metadata entries.
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

  /// Returns all the Object's metadata entries.
  std::map<std::string, std::string> const& metadata() const {
    return metadata_;
  }

  /// Returns all the Object's metadata entries.
  std::map<std::string, std::string>& mutable_metadata() { return metadata_; }
  ///@}

  /**
   * The generation of the object metadata.
   *
   * @note Changes to the object metadata (e.g. changing the `cacheControl`
   * attribute) increases the metageneration, but does not change the object
   * generation.
   */
  std::int64_t metageneration() const { return metageneration_; }

  /// @note This is only intended for mocking.
  ObjectMetadata& set_metageneration(std::int64_t v) {
    metageneration_ = v;
    return *this;
  }

  /// The object name, including bucket and generation.
  std::string const& name() const { return name_; }

  /// @note This is only intended for mocking.
  ObjectMetadata& set_name(std::string v) {
    name_ = std::move(v);
    return *this;
  }

  /// Returns `true` if the object has an `owner` attribute.
  bool has_owner() const { return owner_.has_value(); }

  /**
   * The object's `owner` attribute.
   *
   * It is undefined behavior to call this member function if
   * `has_owner() == false`.
   */
  Owner const& owner() const { return *owner_; }

  /// @note This is only intended for mocking.
  ObjectMetadata& set_owner(Owner v) {
    owner_ = std::move(v);
    return *this;
  }

  /// @note This is only intended for mocking.
  ObjectMetadata& reset_owner() {
    owner_.reset();
    return *this;
  }

  /// The retention expiration time, or the system clock's epoch, if not set.
  std::chrono::system_clock::time_point retention_expiration_time() const {
    return retention_expiration_time_;
  }

  /// @note This is only intended for mocking.
  ObjectMetadata& set_retention_expiration_time(
      std::chrono::system_clock::time_point v) {
    retention_expiration_time_ = v;
    return *this;
  }

  /// An HTTPS link to the object metadata.
  std::string const& self_link() const { return self_link_; }

  /// @note This is only intended for mocking.
  ObjectMetadata& set_self_link(std::string v) {
    self_link_ = std::move(v);
    return *this;
  }

  /// The size of the object's data.
  std::uint64_t size() const { return size_; }

  /// @note This is only intended for mocking.
  ObjectMetadata& set_size(std::uint64_t v) {
    size_ = v;
    return *this;
  }

  /// The `storageClass` attribute.
  std::string const& storage_class() const { return storage_class_; }

  /// Changes the `storageClass` attribute.
  ObjectMetadata& set_storage_class(std::string v) {
    storage_class_ = std::move(v);
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
  std::chrono::system_clock::time_point time_created() const {
    return time_created_;
  }

  /// @note This is only intended for mocking.
  ObjectMetadata& set_time_created(std::chrono::system_clock::time_point v) {
    time_created_ = v;
    return *this;
  }

  /// The object's deletion timestamp.
  std::chrono::system_clock::time_point time_deleted() const {
    return time_deleted_;
  }

  /// @note This is only intended for mocking.
  ObjectMetadata& set_time_deleted(std::chrono::system_clock::time_point v) {
    time_deleted_ = v;
    return *this;
  }

  /// The timestamp for the last storage class change.
  std::chrono::system_clock::time_point time_storage_class_updated() const {
    return time_storage_class_updated_;
  }

  /// @note This is only intended for mocking.
  ObjectMetadata& set_time_storage_class_updated(
      std::chrono::system_clock::time_point v) {
    time_storage_class_updated_ = v;
    return *this;
  }

  /// The timestamp for the last object *metadata* update.
  std::chrono::system_clock::time_point updated() const { return updated_; }

  /// @note This is only intended for mocking.
  ObjectMetadata& set_updated(std::chrono::system_clock::time_point v) {
    updated_ = v;
    return *this;
  }

  friend bool operator==(ObjectMetadata const& lhs, ObjectMetadata const& rhs);
  friend bool operator!=(ObjectMetadata const& lhs, ObjectMetadata const& rhs) {
    return !(lhs == rhs);
  }

 private:
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
  absl::optional<std::chrono::system_clock::time_point> custom_time_;
  absl::optional<CustomerEncryption> customer_encryption_;
  std::string etag_;
  bool event_based_hold_{false};
  std::int64_t generation_{0};
  std::string id_;
  std::string kind_;
  std::string kms_key_name_;
  std::int64_t metageneration_{0};
  std::string md5_hash_;
  std::string media_link_;
  std::map<std::string, std::string> metadata_;
  std::string name_;
  absl::optional<Owner> owner_;
  std::chrono::system_clock::time_point retention_expiration_time_;
  std::string self_link_;
  std::uint64_t size_{0};
  std::string storage_class_;
  bool temporary_hold_{false};
  std::chrono::system_clock::time_point time_created_;
  std::chrono::system_clock::time_point time_deleted_;
  std::chrono::system_clock::time_point time_storage_class_updated_;
  std::chrono::system_clock::time_point updated_;
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
  friend struct internal::PatchBuilderDetails;

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
