// Copyright 2020 Google LLC
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

#include "google/cloud/storage/internal/object_metadata_parser.h"
#include "google/cloud/storage/internal/common_metadata_parser.h"
#include "google/cloud/storage/internal/object_access_control_parser.h"
#include "google/cloud/internal/format_time_point.h"
#include <nlohmann/json.hpp>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace internal {
namespace {
/**
 * Sets a string field in @p json when @p value is not empty.
 *
 * This simplifies the implementation of ToJsonString() because we repeat this
 * check for many attributes.
 */
void SetIfNotEmpty(nlohmann::json& json, char const* key,
                   std::string const& value) {
  if (value.empty()) {
    return;
  }
  json[key] = value;
}
}  // namespace

StatusOr<ObjectMetadata> ObjectMetadataParser::FromJson(
    nlohmann::json const& json) {
  if (!json.is_object()) {
    return Status(StatusCode::kInvalidArgument, __func__);
  }
  ObjectMetadata result{};
  auto status = CommonMetadataParser<ObjectMetadata>::FromJson(result, json);
  if (!status.ok()) return status;

  if (json.count("acl") != 0) {
    for (auto const& kv : json["acl"].items()) {
      auto parsed = ObjectAccessControlParser::FromJson(kv.value());
      if (!parsed.ok()) {
        return std::move(parsed).status();
      }
      result.acl_.emplace_back(std::move(*parsed));
    }
  }

  result.bucket_ = json.value("bucket", "");
  result.cache_control_ = json.value("cacheControl", "");
  auto component_count = internal::ParseIntField(json, "componentCount");
  if (!component_count) return std::move(component_count).status();
  result.component_count_ = *component_count;
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
  auto event_based_hold = internal::ParseBoolField(json, "eventBasedHold");
  if (!event_based_hold) return std::move(event_based_hold).status();
  result.event_based_hold_ = *event_based_hold;
  auto generation = internal::ParseLongField(json, "generation");
  if (!generation) return std::move(generation).status();
  result.generation_ = *generation;
  result.kms_key_name_ = json.value("kmsKeyName", "");
  result.md5_hash_ = json.value("md5Hash", "");
  result.media_link_ = json.value("mediaLink", "");
  if (json.count("metadata") > 0) {
    for (auto const& kv : json["metadata"].items()) {
      result.metadata_.emplace(kv.key(), kv.value().get<std::string>());
    }
  }
  auto expiration_time =
      internal::ParseTimestampField(json, "retentionExpirationTime");
  if (!expiration_time) return std::move(expiration_time).status();
  result.retention_expiration_time_ = *expiration_time;
  auto size = internal::ParseUnsignedLongField(json, "size");
  if (!size) return std::move(size).status();
  result.size_ = *size;
  auto temporary_hold = internal::ParseBoolField(json, "temporaryHold");
  if (!temporary_hold) return std::move(temporary_hold).status();
  result.temporary_hold_ = *temporary_hold;
  auto time_deleted = internal::ParseTimestampField(json, "timeDeleted");
  if (!time_deleted) return std::move(time_deleted).status();
  result.time_deleted_ = *time_deleted;
  auto time_storage_class_updated =
      internal::ParseTimestampField(json, "timeStorageClassUpdated");
  if (!time_storage_class_updated)
    return std::move(time_storage_class_updated).status();
  result.time_storage_class_updated_ = *time_storage_class_updated;
  if (json.count("customTime") == 0) {
    result.custom_time_.reset();
  } else {
    auto v = internal::ParseTimestampField(json, "customTime");
    if (!v) return std::move(v).status();
    result.custom_time_ = *v;
  }
  return result;
}

StatusOr<ObjectMetadata> ObjectMetadataParser::FromString(
    std::string const& payload) {
  auto json = nlohmann::json::parse(payload, nullptr, false);
  return FromJson(json);
}

nlohmann::json ObjectMetadataJsonForCompose(ObjectMetadata const& meta) {
  nlohmann::json metadata_as_json({});
  if (!meta.acl().empty()) {
    for (ObjectAccessControl const& a : meta.acl()) {
      nlohmann::json entry;
      SetIfNotEmpty(entry, "entity", a.entity());
      SetIfNotEmpty(entry, "role", a.role());
      metadata_as_json["acl"].emplace_back(std::move(entry));
    }
  }

  SetIfNotEmpty(metadata_as_json, "cacheControl", meta.cache_control());
  SetIfNotEmpty(metadata_as_json, "contentDisposition",
                meta.content_disposition());
  SetIfNotEmpty(metadata_as_json, "contentEncoding", meta.content_encoding());
  SetIfNotEmpty(metadata_as_json, "contentLanguage", meta.content_language());
  SetIfNotEmpty(metadata_as_json, "contentType", meta.content_type());

  if (meta.event_based_hold()) {
    metadata_as_json["eventBasedHold"] = true;
  }

  SetIfNotEmpty(metadata_as_json, "name", meta.name());
  SetIfNotEmpty(metadata_as_json, "storageClass", meta.storage_class());

  if (!meta.metadata().empty()) {
    nlohmann::json meta_as_json;
    for (auto const& kv : meta.metadata()) {
      meta_as_json[kv.first] = kv.second;
    }
    metadata_as_json["metadata"] = std::move(meta_as_json);
  }

  if (meta.has_custom_time()) {
    metadata_as_json["customTime"] =
        google::cloud::internal::FormatRfc3339(meta.custom_time());
  }

  return metadata_as_json;
}

nlohmann::json ObjectMetadataJsonForCopy(ObjectMetadata const& meta) {
  return ObjectMetadataJsonForCompose(meta);
}

nlohmann::json ObjectMetadataJsonForInsert(ObjectMetadata const& meta) {
  auto json = ObjectMetadataJsonForCompose(meta);
  SetIfNotEmpty(json, "crc32c", meta.crc32c());
  SetIfNotEmpty(json, "md5Hash", meta.md5_hash());
  return json;
}

nlohmann::json ObjectMetadataJsonForRewrite(ObjectMetadata const& meta) {
  return ObjectMetadataJsonForCompose(meta);
}

nlohmann::json ObjectMetadataJsonForUpdate(ObjectMetadata const& meta) {
  nlohmann::json metadata_as_json({});
  if (!meta.acl().empty()) {
    for (ObjectAccessControl const& a : meta.acl()) {
      nlohmann::json entry;
      SetIfNotEmpty(entry, "entity", a.entity());
      SetIfNotEmpty(entry, "role", a.role());
      metadata_as_json["acl"].emplace_back(std::move(entry));
    }
  }

  SetIfNotEmpty(metadata_as_json, "cacheControl", meta.cache_control());
  SetIfNotEmpty(metadata_as_json, "contentDisposition",
                meta.content_disposition());
  SetIfNotEmpty(metadata_as_json, "contentEncoding", meta.content_encoding());
  SetIfNotEmpty(metadata_as_json, "contentLanguage", meta.content_language());
  SetIfNotEmpty(metadata_as_json, "contentType", meta.content_type());

  metadata_as_json["eventBasedHold"] = meta.event_based_hold();

  if (!meta.metadata().empty()) {
    nlohmann::json meta_as_json;
    for (auto const& kv : meta.metadata()) {
      meta_as_json[kv.first] = kv.second;
    }
    metadata_as_json["metadata"] = std::move(meta_as_json);
  }

  if (meta.has_custom_time()) {
    metadata_as_json["customTime"] =
        google::cloud::internal::FormatRfc3339(meta.custom_time());
  }

  return metadata_as_json;
}

}  // namespace internal
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google
