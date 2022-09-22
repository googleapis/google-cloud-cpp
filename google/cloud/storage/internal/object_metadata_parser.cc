// Copyright 2020 Google LLC
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

#include "google/cloud/storage/internal/object_metadata_parser.h"
#include "google/cloud/storage/internal/metadata_parser.h"
#include "google/cloud/storage/internal/object_access_control_parser.h"
#include "google/cloud/internal/format_time_point.h"
#include <nlohmann/json.hpp>
#include <functional>

namespace google {
namespace cloud {
namespace storage {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
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

Status ParseAcl(ObjectMetadata& meta, nlohmann::json const& json) {
  auto i = json.find("acl");
  if (i == json.end()) return Status{};
  std::vector<ObjectAccessControl> acl;
  for (auto const& kv : i->items()) {
    auto parsed = ObjectAccessControlParser::FromJson(kv.value());
    if (!parsed) return std::move(parsed).status();
    acl.push_back(*std::move(parsed));
  }
  meta.set_acl(std::move(acl));
  return Status{};
}

Status ParseComponentCount(ObjectMetadata& meta, nlohmann::json const& json) {
  auto v = internal::ParseIntField(json, "componentCount");
  if (!v) return std::move(v).status();
  meta.set_component_count(*v);
  return Status{};
}

Status ParseCustomTime(ObjectMetadata& meta, nlohmann::json const& json) {
  auto f = json.find("customTime");
  if (f == json.end()) return Status{};
  auto v = internal::ParseTimestampField(json, "customTime");
  if (!v) return std::move(v).status();
  meta.set_custom_time(*v);
  return Status{};
}

Status ParseCustomerEncryption(ObjectMetadata& meta,
                               nlohmann::json const& json) {
  auto f = json.find("customerEncryption");
  if (f == json.end()) return Status{};
  CustomerEncryption e;
  e.encryption_algorithm = f->value("encryptionAlgorithm", "");
  e.key_sha256 = f->value("keySha256", "");
  meta.set_customer_encryption(std::move(e));
  return Status{};
}

Status ParseEventBasedHold(ObjectMetadata& meta, nlohmann::json const& json) {
  auto v = internal::ParseBoolField(json, "eventBasedHold");
  if (!v) return std::move(v).status();
  meta.set_event_based_hold(*v);
  return Status{};
}

Status ParseGeneration(ObjectMetadata& meta, nlohmann::json const& json) {
  auto v = internal::ParseLongField(json, "generation");
  if (!v) return std::move(v).status();
  meta.set_generation(*v);
  return Status{};
}

Status ParseMetageneration(ObjectMetadata& meta, nlohmann::json const& json) {
  auto v = internal::ParseLongField(json, "metageneration");
  if (!v) return std::move(v).status();
  meta.set_metageneration(*v);
  return Status{};
}

Status ParseMetadata(ObjectMetadata& meta, nlohmann::json const& json) {
  auto f = json.find("metadata");
  if (f == json.end()) return Status{};
  std::map<std::string, std::string> metadata;
  for (auto const& kv : f->items()) {
    metadata.emplace(kv.key(), kv.value().get<std::string>());
  }
  meta.mutable_metadata() = std::move(metadata);
  return Status{};
}

Status ParseOwner(ObjectMetadata& meta, nlohmann::json const& json) {
  auto f = json.find("owner");
  if (f == json.end()) return Status{};
  Owner owner;
  owner.entity = f->value("entity", "");
  owner.entity_id = f->value("entityId", "");
  meta.set_owner(std::move(owner));
  return Status{};
}

Status ParseRetentionExpirationTime(ObjectMetadata& meta,
                                    nlohmann::json const& json) {
  auto v = internal::ParseTimestampField(json, "retentionExpirationTime");
  if (!v) return std::move(v).status();
  meta.set_retention_expiration_time(*v);
  return Status{};
}

Status ParseSize(ObjectMetadata& meta, nlohmann::json const& json) {
  auto v = internal::ParseUnsignedLongField(json, "size");
  if (!v) return std::move(v).status();
  meta.set_size(*v);
  return Status{};
}

Status ParseTemporaryHold(ObjectMetadata& meta, nlohmann::json const& json) {
  auto v = internal::ParseBoolField(json, "temporaryHold");
  if (!v) return std::move(v).status();
  meta.set_temporary_hold(*v);
  return Status{};
}

Status ParseTimeCreated(ObjectMetadata& meta, nlohmann::json const& json) {
  auto v = ParseTimestampField(json, "timeCreated");
  if (!v) return std::move(v).status();
  meta.set_time_created(*std::move(v));
  return Status{};
}

Status ParseTimeDeleted(ObjectMetadata& meta, nlohmann::json const& json) {
  auto v = ParseTimestampField(json, "timeDeleted");
  if (!v) return std::move(v).status();
  meta.set_time_deleted(*std::move(v));
  return Status{};
}

Status ParseTimeStorageClassUpdated(ObjectMetadata& meta,
                                    nlohmann::json const& json) {
  auto v = ParseTimestampField(json, "timeStorageClassUpdated");
  if (!v) return std::move(v).status();
  meta.set_time_storage_class_updated(*std::move(v));
  return Status{};
}

Status ParseUpdated(ObjectMetadata& meta, nlohmann::json const& json) {
  auto v = ParseTimestampField(json, "updated");
  if (!v) return std::move(v).status();
  meta.set_updated(*std::move(v));
  return Status{};
}

}  // namespace

StatusOr<ObjectMetadata> ObjectMetadataParser::FromJson(
    nlohmann::json const& json) {
  if (!json.is_object()) return Status(StatusCode::kInvalidArgument, __func__);

  using Parser = std::function<Status(ObjectMetadata&, nlohmann::json const&)>;
  Parser parsers[] = {
      ParseAcl,
      [](ObjectMetadata& meta, nlohmann::json const& json) {
        meta.set_bucket(json.value("bucket", ""));
        return Status{};
      },
      [](ObjectMetadata& meta, nlohmann::json const& json) {
        meta.set_cache_control(json.value("cacheControl", ""));
        return Status{};
      },
      ParseComponentCount,
      [](ObjectMetadata& meta, nlohmann::json const& json) {
        meta.set_content_disposition(json.value("contentDisposition", ""));
        return Status{};
      },
      [](ObjectMetadata& meta, nlohmann::json const& json) {
        meta.set_content_encoding(json.value("contentEncoding", ""));
        return Status{};
      },
      [](ObjectMetadata& meta, nlohmann::json const& json) {
        meta.set_content_language(json.value("contentLanguage", ""));
        return Status{};
      },
      [](ObjectMetadata& meta, nlohmann::json const& json) {
        meta.set_content_type(json.value("contentType", ""));
        return Status{};
      },
      [](ObjectMetadata& meta, nlohmann::json const& json) {
        meta.set_crc32c(json.value("crc32c", ""));
        return Status{};
      },
      ParseCustomTime,
      ParseCustomerEncryption,
      [](ObjectMetadata& meta, nlohmann::json const& json) {
        meta.set_etag(json.value("etag", ""));
        return Status{};
      },
      ParseEventBasedHold,
      ParseGeneration,
      [](ObjectMetadata& meta, nlohmann::json const& json) {
        meta.set_id(json.value("id", ""));
        return Status{};
      },
      [](ObjectMetadata& meta, nlohmann::json const& json) {
        meta.set_kind(json.value("kind", ""));
        return Status{};
      },
      [](ObjectMetadata& meta, nlohmann::json const& json) {
        meta.set_kms_key_name(json.value("kmsKeyName", ""));
        return Status{};
      },
      ParseMetageneration,
      [](ObjectMetadata& meta, nlohmann::json const& json) {
        meta.set_md5_hash(json.value("md5Hash", ""));
        return Status{};
      },
      [](ObjectMetadata& meta, nlohmann::json const& json) {
        meta.set_media_link(json.value("mediaLink", ""));
        return Status{};
      },
      ParseMetadata,
      [](ObjectMetadata& meta, nlohmann::json const& json) {
        meta.set_name(json.value("name", ""));
        return Status{};
      },
      ParseOwner,
      ParseRetentionExpirationTime,
      [](ObjectMetadata& meta, nlohmann::json const& json) {
        meta.set_self_link(json.value("selfLink", ""));
        return Status{};
      },
      [](ObjectMetadata& meta, nlohmann::json const& json) {
        meta.set_storage_class(json.value("storageClass", ""));
        return Status{};
      },
      ParseSize,
      ParseTemporaryHold,
      ParseTimeCreated,
      ParseTimeDeleted,
      ParseTimeStorageClassUpdated,
      ParseUpdated,
  };
  ObjectMetadata meta;
  for (auto const& p : parsers) {
    auto status = p(meta, json);
    if (!status.ok()) return status;
  }
  return meta;
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
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google
