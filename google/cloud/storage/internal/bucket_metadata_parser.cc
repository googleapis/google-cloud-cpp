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

#include "google/cloud/storage/internal/bucket_metadata_parser.h"
#include "google/cloud/storage/internal/bucket_access_control_parser.h"
#include "google/cloud/storage/internal/common_metadata_parser.h"
#include "google/cloud/storage/internal/lifecycle_rule_parser.h"
#include "google/cloud/storage/internal/object_access_control_parser.h"
#include "absl/strings/str_format.h"
#include <nlohmann/json.hpp>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace internal {
namespace {
StatusOr<CorsEntry> ParseCors(nlohmann::json const& json) {
  auto parse_string_list = [](nlohmann::json const& json,
                              char const* field_name) {
    std::vector<std::string> list;
    if (json.count(field_name) != 0) {
      for (auto const& kv : json[field_name].items()) {
        list.emplace_back(kv.value().get<std::string>());
      }
    }
    return list;
  };
  CorsEntry result;
  if (json.count("maxAgeSeconds") != 0) {
    auto v = internal::ParseLongField(json, "maxAgeSeconds");
    if (!v) return std::move(v).status();
    result.max_age_seconds = *v;
  }
  result.method = parse_string_list(json, "method");
  result.origin = parse_string_list(json, "origin");
  result.response_header = parse_string_list(json, "responseHeader");
  return result;
}

void SetIfNotEmpty(nlohmann::json& json, char const* key,
                   std::string const& value) {
  if (value.empty()) {
    return;
  }
  json[key] = value;
}

StatusOr<UniformBucketLevelAccess> ParseUniformBucketLevelAccess(
    nlohmann::json const& json) {
  auto enabled = internal::ParseBoolField(json, "enabled");
  if (!enabled) return std::move(enabled).status();
  auto locked_time = internal::ParseTimestampField(json, "lockedTime");
  if (!locked_time) return std::move(locked_time).status();

  return UniformBucketLevelAccess{*enabled, *locked_time};
}

}  // namespace

StatusOr<BucketMetadata> BucketMetadataParser::FromJson(
    nlohmann::json const& json) {
  if (!json.is_object()) {
    return Status(StatusCode::kInvalidArgument, __func__);
  }
  BucketMetadata result{};
  auto status = CommonMetadataParser<BucketMetadata>::FromJson(result, json);
  if (!status.ok()) {
    return status;
  }

  if (json.count("acl") != 0) {
    for (auto const& kv : json["acl"].items()) {
      auto parsed = internal::BucketAccessControlParser::FromJson(kv.value());
      if (!parsed.ok()) {
        return std::move(parsed).status();
      }
      result.acl_.emplace_back(std::move(*parsed));
    }
  }
  if (json.count("billing") != 0) {
    auto billing = json["billing"];
    auto requester_pays = internal::ParseBoolField(billing, "requesterPays");
    if (!requester_pays) return std::move(requester_pays).status();
    BucketBilling b;
    b.requester_pays = *requester_pays;
    result.billing_ = b;
  }
  if (json.count("cors") != 0) {
    for (auto const& kv : json["cors"].items()) {
      auto cors = ParseCors(kv.value());
      if (!cors) return std::move(cors).status();
      result.cors_.push_back(*std::move(cors));
    }
  }
  if (json.count("defaultEventBasedHold") != 0) {
    result.default_event_based_hold_ =
        json.value("defaultEventBasedHold", false);
  }
  if (json.count("defaultObjectAcl") != 0) {
    for (auto const& kv : json["defaultObjectAcl"].items()) {
      auto parsed = ObjectAccessControlParser::FromJson(kv.value());
      if (!parsed.ok()) {
        return std::move(parsed).status();
      }
      result.default_acl_.emplace_back(std::move(*parsed));
    }
  }
  if (json.count("encryption") != 0) {
    BucketEncryption e;
    e.default_kms_key_name = json["encryption"].value("defaultKmsKeyName", "");
    result.encryption_ = std::move(e);
  }

  if (json.count("iamConfiguration") != 0) {
    BucketIamConfiguration c;
    auto config = json["iamConfiguration"];
    if (config.count("uniformBucketLevelAccess") != 0) {
      auto ubla =
          ParseUniformBucketLevelAccess(config["uniformBucketLevelAccess"]);
      if (!ubla) return std::move(ubla).status();
      c.uniform_bucket_level_access = *ubla;
    }
    if (config.count("publicAccessPrevention") != 0) {
      c.public_access_prevention = config.value("publicAccessPrevention", "");
    }
    result.iam_configuration_ = c;
  }

  if (json.count("lifecycle") != 0) {
    auto lifecycle = json["lifecycle"];
    BucketLifecycle value;
    if (lifecycle.count("rule") != 0) {
      for (auto const& kv : lifecycle["rule"].items()) {
        auto parsed = internal::LifecycleRuleParser::FromJson(kv.value());
        if (!parsed.ok()) {
          return std::move(parsed).status();
        }
        value.rule.emplace_back(std::move(*parsed));
      }
    }
    result.lifecycle_ = std::move(value);
  }

  result.location_ = json.value("location", "");

  result.location_type_ = json.value("locationType", "");

  if (json.count("logging") != 0) {
    auto logging = json["logging"];
    BucketLogging l;
    l.log_bucket = logging.value("logBucket", "");
    l.log_object_prefix = logging.value("logObjectPrefix", "");
    result.logging_ = std::move(l);
  }
  auto project_number = internal::ParseLongField(json, "projectNumber");
  if (!project_number) return std::move(project_number).status();
  result.project_number_ = *project_number;
  if (json.count("labels") > 0) {
    for (auto const& kv : json["labels"].items()) {
      result.labels_.emplace(kv.key(), kv.value().get<std::string>());
    }
  }

  if (json.count("retentionPolicy") != 0) {
    auto retention_policy = json["retentionPolicy"];
    auto is_locked = internal::ParseBoolField(retention_policy, "isLocked");
    if (!is_locked) return std::move(is_locked).status();
    auto retention_period =
        internal::ParseLongField(retention_policy, "retentionPeriod");
    if (!retention_period) return std::move(retention_period).status();
    auto effective_time =
        internal::ParseTimestampField(retention_policy, "effectiveTime");
    if (!effective_time) return std::move(effective_time).status();

    result.retention_policy_ = BucketRetentionPolicy{
        std::chrono::seconds(*retention_period), *effective_time, *is_locked};
  }

  if (json.count("versioning") != 0) {
    auto versioning = json["versioning"];
    if (versioning.count("enabled") != 0) {
      auto enabled = internal::ParseBoolField(versioning, "enabled");
      if (!enabled) return std::move(enabled).status();
      result.versioning_ = BucketVersioning{*enabled};
    }
  }

  if (json.count("website") != 0) {
    auto website = json["website"];
    BucketWebsite w;
    w.main_page_suffix = website.value("mainPageSuffix", "");
    w.not_found_page = website.value("notFoundPage", "");
    result.website_ = std::move(w);
  }
  return result;
}

StatusOr<BucketMetadata> BucketMetadataParser::FromString(
    std::string const& payload) {
  auto json = nlohmann::json::parse(payload, nullptr, false);
  return FromJson(json);
}

std::string BucketMetadataToJsonString(BucketMetadata const& meta) {
  nlohmann::json metadata_as_json;
  if (!meta.acl().empty()) {
    for (BucketAccessControl const& a : meta.acl()) {
      nlohmann::json entry;
      SetIfNotEmpty(entry, "entity", a.entity());
      SetIfNotEmpty(entry, "role", a.role());
      metadata_as_json["acl"].emplace_back(std::move(entry));
    }
  }

  if (!meta.cors().empty()) {
    for (CorsEntry const& v : meta.cors()) {
      nlohmann::json cors_as_json;
      if (v.max_age_seconds.has_value()) {
        cors_as_json["maxAgeSeconds"] = *v.max_age_seconds;
      }
      if (!v.method.empty()) {
        cors_as_json["method"] = v.method;
      }
      if (!v.origin.empty()) {
        cors_as_json["origin"] = v.origin;
      }
      if (!v.response_header.empty()) {
        cors_as_json["responseHeader"] = v.response_header;
      }
      metadata_as_json["cors"].emplace_back(std::move(cors_as_json));
    }
  }

  if (meta.has_billing()) {
    nlohmann::json b{
        {"requesterPays", meta.billing().requester_pays},
    };
    metadata_as_json["billing"] = std::move(b);
  }

  metadata_as_json["defaultEventBasedHold"] = meta.default_event_based_hold();

  if (!meta.default_acl().empty()) {
    for (ObjectAccessControl const& a : meta.default_acl()) {
      nlohmann::json entry;
      SetIfNotEmpty(entry, "entity", a.entity());
      SetIfNotEmpty(entry, "role", a.role());
      metadata_as_json["defaultObjectAcl"].emplace_back(std::move(entry));
    }
  }

  if (meta.has_encryption()) {
    nlohmann::json e;
    SetIfNotEmpty(e, "defaultKmsKeyName",
                  meta.encryption().default_kms_key_name);
    metadata_as_json["encryption"] = std::move(e);
  }

  if (meta.has_iam_configuration()) {
    nlohmann::json c;
    if (meta.iam_configuration().uniform_bucket_level_access.has_value()) {
      nlohmann::json ubla;
      ubla["enabled"] =
          meta.iam_configuration().uniform_bucket_level_access->enabled;
      // The lockedTime field is not mutable and should not be set by the client
      // the server will provide a value.
      c["uniformBucketLevelAccess"] = std::move(ubla);
    }
    if (meta.iam_configuration().public_access_prevention.has_value()) {
      c["publicAccessPrevention"] =
          *meta.iam_configuration().public_access_prevention;
    }
    metadata_as_json["iamConfiguration"] = std::move(c);
  }

  if (!meta.labels().empty()) {
    nlohmann::json labels_as_json;
    for (auto const& kv : meta.labels()) {
      labels_as_json[kv.first] = kv.second;
    }
    metadata_as_json["labels"] = std::move(labels_as_json);
  }

  if (meta.has_lifecycle()) {
    nlohmann::json rule;
    for (LifecycleRule const& v : meta.lifecycle().rule) {
      nlohmann::json condition;
      auto const& c = v.condition();
      if (c.age) {
        condition["age"] = *c.age;
      }
      if (c.created_before.has_value()) {
        condition["createdBefore"] =
            absl::StrFormat("%04d-%02d-%02d", c.created_before->year(),
                            c.created_before->month(), c.created_before->day());
      }
      if (c.is_live) {
        condition["isLive"] = *c.is_live;
      }
      if (c.matches_storage_class) {
        condition["matchesStorageClass"] = *c.matches_storage_class;
      }
      if (c.num_newer_versions) {
        condition["numNewerVersions"] = *c.num_newer_versions;
      }
      nlohmann::json action{{"type", v.action().type}};
      if (!v.action().storage_class.empty()) {
        action["storageClass"] = v.action().storage_class;
      }
      rule.emplace_back(nlohmann::json{{"condition", std::move(condition)},
                                       {"action", std::move(action)}});
    }
    metadata_as_json["lifecycle"] = nlohmann::json{{"rule", std::move(rule)}};
  }

  SetIfNotEmpty(metadata_as_json, "location", meta.location());

  SetIfNotEmpty(metadata_as_json, "locationType", meta.location_type());

  if (meta.has_logging()) {
    nlohmann::json l;
    SetIfNotEmpty(l, "logBucket", meta.logging().log_bucket);
    SetIfNotEmpty(l, "logObjectPrefix", meta.logging().log_object_prefix);
    metadata_as_json["logging"] = std::move(l);
  }

  SetIfNotEmpty(metadata_as_json, "name", meta.name());

  if (meta.has_retention_policy()) {
    nlohmann::json r{
        {"retentionPeriod", meta.retention_policy().retention_period.count()}};
    metadata_as_json["retentionPolicy"] = std::move(r);
  }

  SetIfNotEmpty(metadata_as_json, "storageClass", meta.storage_class());

  if (meta.versioning().has_value()) {
    metadata_as_json["versioning"] =
        nlohmann::json{{"enabled", meta.versioning()->enabled}};
  }

  if (meta.has_website()) {
    nlohmann::json w;
    SetIfNotEmpty(w, "mainPageSuffix", meta.website().main_page_suffix);
    SetIfNotEmpty(w, "notFoundPage", meta.website().not_found_page);
    metadata_as_json["website"] = std::move(w);
  }

  return metadata_as_json.dump();
}

}  // namespace internal
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google
