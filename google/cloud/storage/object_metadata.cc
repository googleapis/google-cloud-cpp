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

#include "google/cloud/storage/object_metadata.h"
#include "google/cloud/storage/internal/metadata_parser.h"
#include "google/cloud/storage/internal/nljson.h"

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace {
/**
 * Set a string field in @p json when @p value is not empty.
 *
 * This simplifies the implementation of ToJsonString() because we repeat this
 * check for many attributes.
 */
void SetIfNotEmpty(internal::nl::json& json, char const* key,
                   std::string const& value) {
  if (value.empty()) {
    return;
  }
  json[key] = value;
}
}  // namespace

ObjectMetadata ObjectMetadata::ParseFromJson(internal::nl::json const& json) {
  ObjectMetadata result{};
  static_cast<CommonMetadata<ObjectMetadata>&>(result) =
      CommonMetadata<ObjectMetadata>::ParseFromJson(json);

  if (json.count("acl") != 0) {
    for (auto const& kv : json["acl"].items()) {
      result.acl_.emplace_back(ObjectAccessControl::ParseFromJson(kv.value()));
    }
  }

  result.bucket_ = json.value("bucket", "");
  result.cache_control_ = json.value("cacheControl", "");
  result.component_count_ = internal::ParseLongField(json, "componentCount");
  result.content_disposition_ = json.value("contentDisposition", "");
  result.content_encoding_ = json.value("contentEncoding", "");
  result.content_language_ = json.value("contentLanguage", "");
  result.content_type_ = json.value("contentType", "");
  result.crc32c_ = json.value("crc32c", "");
  if (json.count("customerEncryption") != 0) {
    auto field = json["customerEncryption"];
    CustomerEncryption e;
    e.encryption_algorithm = field.value("encryptionAlgorithm", "");
    e.key_sha256 = field.value("keySha256", "");
    result.customer_encryption_ = std::move(e);
  }
  result.generation_ = internal::ParseLongField(json, "generation");
  result.kms_key_name_ = json.value("kmsKeyName", "");
  result.md5_hash_ = json.value("md5Hash", "");
  result.media_link_ = json.value("mediaLink", "");
  if (json.count("metadata") > 0) {
    for (auto const& kv : json["metadata"].items()) {
      result.metadata_.emplace(kv.key(), kv.value().get<std::string>());
    }
  }
  result.size_ = internal::ParseUnsignedLongField(json, "size");
  result.time_deleted_ = internal::ParseTimestampField(json, "timeDeleted");
  result.time_storage_class_updated_ =
      internal::ParseTimestampField(json, "timeStorageClassUpdated");
  return result;
}

ObjectMetadata ObjectMetadata::ParseFromString(std::string const& payload) {
  auto json = internal::nl::json::parse(payload);
  return ParseFromJson(json);
}

std::string ObjectMetadata::JsonPayloadForUpdate() const {
  using internal::nl::json;
  json metadata_as_json;
  if (not acl().empty()) {
    for (ObjectAccessControl const& a : acl()) {
      json entry;
      SetIfNotEmpty(entry, "entity", a.entity());
      SetIfNotEmpty(entry, "role", a.role());
      metadata_as_json["acl"].emplace_back(std::move(entry));
    }
  }

  SetIfNotEmpty(metadata_as_json, "cacheControl", cache_control());
  SetIfNotEmpty(metadata_as_json, "contentDisposition", content_disposition());
  SetIfNotEmpty(metadata_as_json, "contentEncoding", content_encoding());
  SetIfNotEmpty(metadata_as_json, "contentLanguage", content_language());
  SetIfNotEmpty(metadata_as_json, "contentType", content_type());

  if (not metadata().empty()) {
    json meta_as_json;
    for (auto const& kv : metadata()) {
      meta_as_json[kv.first] = kv.second;
    }
    metadata_as_json["metadata"] = std::move(meta_as_json);
  }

  return metadata_as_json.dump();
}

bool ObjectMetadata::operator==(ObjectMetadata const& rhs) const {
  return static_cast<internal::CommonMetadata<ObjectMetadata> const&>(*this) ==
             rhs and
         acl_ == rhs.acl_ and bucket_ == rhs.bucket_ and
         cache_control_ == rhs.cache_control_ and
         component_count_ == rhs.component_count_ and
         content_disposition_ == rhs.content_disposition_ and
         content_encoding_ == rhs.content_encoding_ and
         content_language_ == rhs.content_language_ and
         content_type_ == rhs.content_type_ and crc32c_ == rhs.crc32c_ and
         customer_encryption_ == customer_encryption_ and
         generation_ == rhs.generation_ and
         kms_key_name_ == rhs.kms_key_name_ and md5_hash_ == rhs.md5_hash_ and
         media_link_ == rhs.media_link_ and metadata_ == rhs.metadata_ and
         time_deleted_ == rhs.time_deleted_ and
         time_storage_class_updated_ == rhs.time_storage_class_updated_ and
         size_ == rhs.size_;
}

std::ostream& operator<<(std::ostream& os, ObjectMetadata const& rhs) {
  // TODO(#536) - convert back to JSON for a nicer format.
  os << "ObjectMetadata={name=" << rhs.name() << ", acl=[";
  char const* sep = "";
  for (auto const& acl : rhs.acl()) {
    os << sep << acl;
    sep = ", ";
  }
  os << "], bucket=" << rhs.bucket()
     << ", cache_control=" << rhs.cache_control()
     << ", component_count=" << rhs.component_count()
     << ", content_disposition=" << rhs.content_disposition()
     << ", content_encoding=" << rhs.content_encoding()
     << ", content_language=" << rhs.content_language()
     << ", content_type=" << rhs.content_type() << ", crc32c=" << rhs.crc32c();

  if (rhs.has_customer_encryption()) {
    os << ", customer_encryption.encryption_algorithm="
       << rhs.customer_encryption().encryption_algorithm
       << ", customer_encryption.key_sha256="
       << rhs.customer_encryption().key_sha256;
  }

  os << ", etag=" << rhs.etag() << ", generation=" << rhs.generation()
     << ", id=" << rhs.id() << ", kind=" << rhs.kind()
     << ", kms_key_name=" << rhs.kms_key_name()
     << ", md5_hash=" << rhs.md5_hash() << ", media_link=" << rhs.media_link();
  sep = "metadata.";
  for (auto const& kv : rhs.metadata_) {
    os << sep << kv.first << "=" << kv.second;
    sep = ", metadata.";
  }
  os << ", metageneration=" << rhs.metageneration() << ", name=" << rhs.name();

  if (rhs.has_owner()) {
    os << ", owner.entity=" << rhs.owner().entity
       << ", owner.entity_id=" << rhs.owner().entity_id;
  }

  os << ", self_link=" << rhs.self_link() << ", size=" << rhs.size()
     << ", storage_class=" << rhs.storage_class()
     << ", time_created=" << rhs.time_created().time_since_epoch().count()
     << ", time_deleted=" << rhs.time_deleted().time_since_epoch().count()
     << ", time_storage_class_updated="
     << rhs.time_storage_class_updated().time_since_epoch().count()
     << ", updated=" << rhs.updated().time_since_epoch().count() << "}";
  return os;
}

std::string ObjectMetadataPatchBuilder::BuildPatch() const {
  internal::PatchBuilder tmp = impl_;
  if (metadata_subpatch_dirty_) {
    if (metadata_subpatch_.empty()) {
      tmp.RemoveField("metadata");
    } else {
      tmp.AddSubPatch("metadata", metadata_subpatch_);
    }
  }
  return tmp.ToString();
}

ObjectMetadataPatchBuilder& ObjectMetadataPatchBuilder::SetAcl(
    std::vector<ObjectAccessControl> const& v) {
  if (v.empty()) {
    return ResetAcl();
  }
  std::vector<internal::nl::json> array;
  array.reserve(v.size());
  for (auto const& a : v) {
    array.emplace_back(internal::nl::json{
        {"entity", a.entity()},
        {"role", a.role()},
    });
  }
  impl_.SetArrayField("acl", array);
  return *this;
}

ObjectMetadataPatchBuilder& ObjectMetadataPatchBuilder::ResetAcl() {
  impl_.RemoveField("acl");
  return *this;
}

ObjectMetadataPatchBuilder& ObjectMetadataPatchBuilder::SetCacheControl(
    std::string const& v) {
  if (v.empty()) {
    return ResetCacheControl();
  }
  impl_.SetStringField("cacheControl", v);
  return *this;
}

ObjectMetadataPatchBuilder& ObjectMetadataPatchBuilder::ResetCacheControl() {
  impl_.RemoveField("cacheControl");
  return *this;
}

ObjectMetadataPatchBuilder& ObjectMetadataPatchBuilder::SetContentDisposition(
    std::string const& v) {
  if (v.empty()) {
    return ResetContentDisposition();
  }
  impl_.SetStringField("contentDisposition", v);
  return *this;
}

ObjectMetadataPatchBuilder&
ObjectMetadataPatchBuilder::ResetContentDisposition() {
  impl_.RemoveField("contentDisposition");
  return *this;
}

ObjectMetadataPatchBuilder& ObjectMetadataPatchBuilder::SetContentEncoding(
    std::string const& v) {
  if (v.empty()) {
    return ResetContentEncoding();
  }
  impl_.SetStringField("contentEncoding", v);
  return *this;
}

ObjectMetadataPatchBuilder& ObjectMetadataPatchBuilder::ResetContentEncoding() {
  impl_.RemoveField("contentEncoding");
  return *this;
}

ObjectMetadataPatchBuilder& ObjectMetadataPatchBuilder::SetContentLanguage(
    std::string const& v) {
  if (v.empty()) {
    return ResetContentLanguage();
  }
  impl_.SetStringField("contentLanguage", v);
  return *this;
}

ObjectMetadataPatchBuilder& ObjectMetadataPatchBuilder::ResetContentLanguage() {
  impl_.RemoveField("contentLanguage");
  return *this;
}

ObjectMetadataPatchBuilder& ObjectMetadataPatchBuilder::SetContentType(
    std::string const& v) {
  if (v.empty()) {
    return ResetContentType();
  }
  impl_.SetStringField("contentType", v);
  return *this;
}

ObjectMetadataPatchBuilder& ObjectMetadataPatchBuilder::ResetContentType() {
  impl_.RemoveField("contentType");
  return *this;
}

ObjectMetadataPatchBuilder& ObjectMetadataPatchBuilder::SetMetadata(
    std::string const& key, std::string const& value) {
  metadata_subpatch_.SetStringField(key.c_str(), value);
  metadata_subpatch_dirty_ = true;
  return *this;
}

ObjectMetadataPatchBuilder& ObjectMetadataPatchBuilder::ResetMetadata(
    std::string const& key) {
  metadata_subpatch_.RemoveField(key.c_str());
  metadata_subpatch_dirty_ = true;
  return *this;
}

ObjectMetadataPatchBuilder& ObjectMetadataPatchBuilder::ResetMetadata() {
  metadata_subpatch_.clear();
  metadata_subpatch_dirty_ = true;
  return *this;
}

}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google
