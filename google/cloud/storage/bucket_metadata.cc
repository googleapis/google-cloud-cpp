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

#include "google/cloud/storage/bucket_metadata.h"
#include "google/cloud/status.h"
#include "google/cloud/internal/ios_flags_saver.h"
#include "google/cloud/storage/internal/format_rfc3339.h"
#include "google/cloud/storage/internal/metadata_parser.h"
#include "google/cloud/storage/internal/nljson.h"

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace {
CorsEntry ParseCors(internal::nl::json const& json) {
  auto parse_string_list = [](internal::nl::json const& json,
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
    result.max_age_seconds = internal::ParseLongField(json, "maxAgeSeconds");
  }
  result.method = parse_string_list(json, "method");
  result.origin = parse_string_list(json, "origin");
  result.response_header = parse_string_list(json, "responseHeader");
  return result;
}

void SetIfNotEmpty(internal::nl::json& json, char const* key,
                   std::string const& value) {
  if (value.empty()) {
    return;
  }
  json[key] = value;
}

BucketOnlyPolicy ParseBucketOnlyPolicy(internal::nl::json const& json) {
  BucketOnlyPolicy result;
  result.enabled = internal::ParseBoolField(json, "enabled");
  result.locked_time = internal::ParseTimestampField(json, "lockedTime");
  return result;
}

}  // namespace

std::ostream& operator<<(std::ostream& os, CorsEntry const& rhs) {
  auto join = [](char const* sep, std::vector<std::string> const& list) {
    if (list.empty()) {
      return std::string{};
    }
    return std::accumulate(++list.begin(), list.end(), list.front(),
                           [sep](std::string a, std::string const& b) {
                             a += sep;
                             a += b;
                             return a;
                           });
  };
  os << "CorsEntry={";
  char const* sep = "";
  if (rhs.max_age_seconds.has_value()) {
    os << "max_age_seconds=" << *rhs.max_age_seconds;
    sep = ", ";
  }
  return os << sep << "method=[" << join(", ", rhs.method) << "], origin=["
            << join(", ", rhs.origin) << "], response_header=["
            << join(", ", rhs.response_header) << "]}";
}

std::ostream& operator<<(std::ostream& os, BucketOnlyPolicy const& rhs) {
  google::cloud::internal::IosFlagsSaver save_format(os);
  return os << "BucketOnlyPolicy={enabled=" << std::boolalpha << rhs.enabled
            << ", locked_time=" << internal::FormatRfc3339(rhs.locked_time)
            << "}";
}

std::ostream& operator<<(std::ostream& os, BucketIamConfiguration const& rhs) {
  os << "BucketIamConfiguration={";
  if (rhs.bucket_only_policy.has_value()) {
    os << "bucket_only_policy=" << *rhs.bucket_only_policy;
  }
  return os << "}";
}

std::ostream& operator<<(std::ostream& os, BucketLogging const& rhs) {
  return os << "BucketLogging={log_bucket=" << rhs.log_bucket
            << ", log_object_prefix=" << rhs.log_object_prefix << "}";
}

std::ostream& operator<<(std::ostream& os, BucketRetentionPolicy const& rhs) {
  return os << "BucketRetentionPolicy={retention_period="
            << rhs.retention_period.count() << "s, effective_time="
            << internal::FormatRfc3339(rhs.effective_time)
            << ", locked=" << rhs.is_locked << "}";
}

StatusOr<BucketMetadata> BucketMetadata::ParseFromJson(
    internal::nl::json const& json) {
  if (not json.is_object()) {
    return Status(StatusCode::kInvalidArgument, __func__);
  }
  BucketMetadata result{};
  auto status = CommonMetadata<BucketMetadata>::ParseFromJson(result, json);
  if (not status.ok()) {
    return status;
  }

  if (json.count("acl") != 0) {
    for (auto const& kv : json["acl"].items()) {
      auto parsed = BucketAccessControl::ParseFromJson(kv.value());
      if (not parsed.ok()) {
        return std::move(parsed).status();
      }
      result.acl_.emplace_back(std::move(*parsed));
    }
  }
  if (json.count("billing") != 0) {
    auto billing = json["billing"];
    BucketBilling b;
    b.requester_pays = internal::ParseBoolField(billing, "requesterPays");
    result.billing_ = b;
  }
  if (json.count("cors") != 0) {
    for (auto const& kv : json["cors"].items()) {
      result.cors_.emplace_back(ParseCors(kv.value()));
    }
  }
  if (json.count("defaultEventBasedHold") != 0) {
    result.default_event_based_hold_ =
        json.value("defaultEventBasedHold", false);
  }
  if (json.count("defaultObjectAcl") != 0) {
    for (auto const& kv : json["defaultObjectAcl"].items()) {
      auto parsed = ObjectAccessControl::ParseFromJson(kv.value());
      if (not parsed.ok()) {
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
    if (config.count("bucketOnlyPolicy") != 0) {
      c.bucket_only_policy = ParseBucketOnlyPolicy(config["bucketOnlyPolicy"]);
    }
    result.iam_configuration_ = c;
  }

  if (json.count("lifecycle") != 0) {
    auto lifecycle = json["lifecycle"];
    BucketLifecycle value;
    if (lifecycle.count("rule") != 0) {
      for (auto const& kv : lifecycle["rule"].items()) {
        auto parsed = LifecycleRule::ParseFromJson(kv.value());
        if (not parsed.ok()) {
          return std::move(parsed).status();
        }
        value.rule.emplace_back(std::move(*parsed));
      }
    }
    result.lifecycle_ = std::move(value);
  }

  result.location_ = json.value("location", "");

  if (json.count("logging") != 0) {
    auto logging = json["logging"];
    BucketLogging l;
    l.log_bucket = logging.value("logBucket", "");
    l.log_object_prefix = logging.value("logObjectPrefix", "");
    result.logging_ = std::move(l);
  }
  result.project_number_ = internal::ParseLongField(json, "projectNumber");
  if (json.count("labels") > 0) {
    for (auto const& kv : json["labels"].items()) {
      result.labels_.emplace(kv.key(), kv.value().get<std::string>());
    }
  }

  if (json.count("retentionPolicy") != 0) {
    auto retention_policy = json["retentionPolicy"];
    BucketRetentionPolicy r;
    r.retention_period = std::chrono::seconds(
        internal::ParseLongField(retention_policy, "retentionPeriod"));
    r.effective_time =
        internal::ParseTimestampField(retention_policy, "effectiveTime");
    r.is_locked = internal::ParseBoolField(retention_policy, "isLocked");
    result.retention_policy_ = r;
  }

  if (json.count("versioning") != 0) {
    auto versioning = json["versioning"];
    if (versioning.count("enabled") != 0) {
      BucketVersioning v{internal::ParseBoolField(versioning, "enabled")};
      result.versioning_ = v;
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

StatusOr<BucketMetadata> BucketMetadata::ParseFromString(
    std::string const& payload) {
  auto json = storage::internal::nl::json::parse(payload, nullptr, false);
  return ParseFromJson(json);
}

std::string BucketMetadata::ToJsonString() const {
  using internal::nl::json;
  json metadata_as_json;
  if (not acl().empty()) {
    for (BucketAccessControl const& a : acl()) {
      json entry;
      SetIfNotEmpty(entry, "entity", a.entity());
      SetIfNotEmpty(entry, "role", a.role());
      metadata_as_json["acl"].emplace_back(std::move(entry));
    }
  }

  if (not cors().empty()) {
    for (CorsEntry const& v : cors()) {
      json cors_as_json;
      if (v.max_age_seconds.has_value()) {
        cors_as_json["maxAgeSeconds"] = *v.max_age_seconds;
      }
      if (not v.method.empty()) {
        cors_as_json["method"] = v.method;
      }
      if (not v.origin.empty()) {
        cors_as_json["origin"] = v.origin;
      }
      if (not v.response_header.empty()) {
        cors_as_json["responseHeader"] = v.response_header;
      }
      metadata_as_json["cors"].emplace_back(std::move(cors_as_json));
    }
  }

  if (has_billing()) {
    json b{
        {"requesterPays", billing().requester_pays},
    };
    metadata_as_json["billing"] = std::move(b);
  }

  metadata_as_json["defaultEventBasedHold"] = default_event_based_hold();

  if (not default_acl().empty()) {
    for (ObjectAccessControl const& a : default_acl()) {
      json entry;
      SetIfNotEmpty(entry, "entity", a.entity());
      SetIfNotEmpty(entry, "role", a.role());
      metadata_as_json["defaultObjectAcl"].emplace_back(std::move(entry));
    }
  }

  if (has_encryption()) {
    json e;
    SetIfNotEmpty(e, "defaultKmsKeyName", encryption().default_kms_key_name);
    metadata_as_json["encryption"] = std::move(e);
  }

  if (has_iam_configuration()) {
    json c;
    if (iam_configuration().bucket_only_policy.has_value()) {
      json bop;
      bop["enabled"] = iam_configuration().bucket_only_policy->enabled;
      // The lockedTime field is not mutable and should not be set by the client
      // the server will provide a value.
      c["bucketOnlyPolicy"] = std::move(bop);
    }
    metadata_as_json["iamConfiguration"] = std::move(c);
  }

  if (not labels_.empty()) {
    json labels_as_json;
    for (auto const& kv : labels_) {
      labels_as_json[kv.first] = kv.second;
    }
    metadata_as_json["labels"] = std::move(labels_as_json);
  }

  if (has_lifecycle()) {
    json rule;
    for (LifecycleRule const& v : lifecycle().rule) {
      json condition;
      auto const& c = v.condition();
      if (c.age) {
        condition["age"] = *c.age;
      }
      if (c.created_before.has_value()) {
        condition["createdBefore"] = internal::FormatRfc3339(*c.created_before);
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
      json action{{"type", v.action().type}};
      if (not v.action().storage_class.empty()) {
        action["storageClass"] = v.action().storage_class;
      }
      rule.emplace_back(json{{"condition", std::move(condition)},
                             {"action", std::move(action)}});
    }
    metadata_as_json["lifecycle"] = json{{"rule", std::move(rule)}};
  }

  SetIfNotEmpty(metadata_as_json, "location", location());

  if (has_logging()) {
    json l;
    SetIfNotEmpty(l, "logBucket", logging().log_bucket);
    SetIfNotEmpty(l, "logObjectPrefix", logging().log_object_prefix);
    metadata_as_json["logging"] = std::move(l);
  }

  SetIfNotEmpty(metadata_as_json, "name", name());

  if (has_retention_policy()) {
    json r{{"retentionPeriod", retention_policy().retention_period.count()}};
    metadata_as_json["retentionPolicy"] = std::move(r);
  }

  SetIfNotEmpty(metadata_as_json, "storageClass", storage_class());

  if (versioning().has_value()) {
    metadata_as_json["versioning"] = json{{"enabled", versioning()->enabled}};
  }

  if (has_website()) {
    json w;
    SetIfNotEmpty(w, "mainPageSuffix", website().main_page_suffix);
    SetIfNotEmpty(w, "notFoundPage", website().not_found_page);
    metadata_as_json["website"] = std::move(w);
  }

  return metadata_as_json.dump();
}

bool BucketMetadata::operator==(BucketMetadata const& rhs) const {
  return static_cast<internal::CommonMetadata<BucketMetadata> const&>(*this) ==
             rhs and
         acl_ == rhs.acl_ and billing_ == rhs.billing_ and
         cors_ == rhs.cors_ and
         default_event_based_hold_ == rhs.default_event_based_hold_ and
         default_acl_ == rhs.default_acl_ and encryption_ == rhs.encryption_ and
         iam_configuration_ == rhs.iam_configuration_ and
         project_number_ == rhs.project_number_ and
         lifecycle_ == rhs.lifecycle_ and location_ == rhs.location_ and
         logging_ == rhs.logging_ and labels_ == rhs.labels_ and
         retention_policy_ == rhs.retention_policy_ and
         versioning_ == rhs.versioning_ and website_ == rhs.website_;
}

std::ostream& operator<<(std::ostream& os, BucketMetadata const& rhs) {
  google::cloud::internal::IosFlagsSaver save_format(os);
  os << "BucketMetadata={name=" << rhs.name();

  os << ", acl=[";
  char const* sep = "";
  for (auto const& acl : rhs.acl()) {
    os << sep << acl;
    sep = ", ";
  }
  os << "]";

  if (rhs.has_billing()) {
    auto previous_flags = os.flags();
    os << ", billing.requesterPays=" << std::boolalpha
       << rhs.billing().requester_pays;
    os.flags(previous_flags);
  }

  os << ", cors=[";
  sep = "";
  for (auto const& cors : rhs.cors()) {
    os << sep << cors;
    sep = ", ";
  }
  os << "]";

  os << ", default_event_based_hold=" << std::boolalpha
     << rhs.default_event_based_hold();

  os << ", default_acl=[";
  sep = "";
  for (auto const& acl : rhs.default_acl()) {
    os << sep << acl;
    sep = ", ";
  }
  os << "]";

  if (rhs.has_encryption()) {
    os << ", encryption.default_kms_key_name="
       << rhs.encryption().default_kms_key_name;
  }

  os << ", etag=" << rhs.etag();

  if (rhs.has_iam_configuration()) {
    os << ", iam_configuration=" << rhs.iam_configuration();
  }

  os << ", id=" << rhs.id() << ", kind=" << rhs.kind();

  for (auto const& kv : rhs.labels_) {
    os << ", labels." << kv.first << "=" << kv.second;
  }

  if (rhs.has_lifecycle()) {
    os << ", lifecycle.rule=[";
    sep = "";
    for (auto const& r : rhs.lifecycle().rule) {
      os << sep << r;
      sep = ", ";
    }
    os << "]";
  }

  os << ", location=" << rhs.location();

  if (rhs.has_logging()) {
    os << ", logging=" << rhs.logging();
  }

  os << ", metageneration=" << rhs.metageneration() << ", name=" << rhs.name();

  if (rhs.has_owner()) {
    os << ", owner.entity=" << rhs.owner().entity
       << ", owner.entity_id=" << rhs.owner().entity_id;
  }

  os << ", project_number=" << rhs.project_number()
     << ", self_link=" << rhs.self_link()
     << ", storage_class=" << rhs.storage_class()
     << ", time_created=" << rhs.time_created().time_since_epoch().count()
     << ", updated=" << rhs.updated().time_since_epoch().count();

  if (rhs.has_retention_policy()) {
    os << ", retention_policy.retention_period="
       << rhs.retention_policy().retention_period.count()
       << ", retention_policy.effective_time="
       << internal::FormatRfc3339(rhs.retention_policy().effective_time)
       << ", retention_policy.is_locked=" << std::boolalpha
       << rhs.retention_policy().is_locked;
  }

  if (rhs.versioning().has_value()) {
    auto previous_flags = os.flags();
    os << ", versioning.enabled=" << std::boolalpha
       << rhs.versioning()->enabled;
    os.flags(previous_flags);
  }

  if (rhs.has_website()) {
    os << ", website.main_page_suffix=" << rhs.website().main_page_suffix
       << ", website.not_found_page=" << rhs.website().not_found_page;
  }

  return os << "}";
}

std::string BucketMetadataPatchBuilder::BuildPatch() const {
  internal::PatchBuilder tmp = impl_;
  if (labels_subpatch_dirty_) {
    if (labels_subpatch_.empty()) {
      tmp.RemoveField("labels");
    } else {
      tmp.AddSubPatch("labels", labels_subpatch_);
    }
  }
  return tmp.ToString();
}

BucketMetadataPatchBuilder& BucketMetadataPatchBuilder::SetAcl(
    std::vector<BucketAccessControl> const& v) {
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

BucketMetadataPatchBuilder& BucketMetadataPatchBuilder::ResetAcl() {
  impl_.RemoveField("acl");
  return *this;
}

BucketMetadataPatchBuilder& BucketMetadataPatchBuilder::SetBilling(
    BucketBilling const& v) {
  impl_.AddSubPatch("billing", internal::PatchBuilder().SetBoolField(
                                   "requesterPays", v.requester_pays));
  return *this;
}

BucketMetadataPatchBuilder& BucketMetadataPatchBuilder::ResetBilling() {
  impl_.RemoveField("billing");
  return *this;
}

BucketMetadataPatchBuilder& BucketMetadataPatchBuilder::SetCors(
    std::vector<CorsEntry> const& v) {
  if (v.empty()) {
    return ResetCors();
  }
  std::vector<internal::nl::json> array;
  array.reserve(v.size());
  for (auto const& a : v) {
    internal::nl::json entry;
    if (a.max_age_seconds.has_value()) {
      entry["maxAgeSeconds"] = *a.max_age_seconds;
    }
    if (not a.method.empty()) {
      entry["method"] = a.method;
    }
    if (not a.origin.empty()) {
      entry["origin"] = a.origin;
    }
    if (not a.response_header.empty()) {
      entry["responseHeader"] = a.response_header;
    }
    array.emplace_back(std::move(entry));
  }
  impl_.SetArrayField("cors", array);
  return *this;
}

BucketMetadataPatchBuilder& BucketMetadataPatchBuilder::ResetCors() {
  impl_.RemoveField("cors");
  return *this;
}

BucketMetadataPatchBuilder&
BucketMetadataPatchBuilder::SetDefaultEventBasedHold(bool v) {
  impl_.SetBoolField("defaultEventBasedHold", v);
  return *this;
}

BucketMetadataPatchBuilder&
BucketMetadataPatchBuilder::ResetDefaultEventBasedHold() {
  impl_.RemoveField("defaultEventBasedHold");
  return *this;
}

BucketMetadataPatchBuilder& BucketMetadataPatchBuilder::SetDefaultAcl(
    std::vector<ObjectAccessControl> const& v) {
  if (v.empty()) {
    return ResetDefaultAcl();
  }
  std::vector<internal::nl::json> array;
  array.reserve(v.size());
  for (auto const& a : v) {
    array.emplace_back(internal::nl::json{
        {"entity", a.entity()},
        {"role", a.role()},
    });
  }
  impl_.SetArrayField("defaultObjectAcl", array);
  return *this;
}

BucketMetadataPatchBuilder& BucketMetadataPatchBuilder::ResetDefaultAcl() {
  impl_.RemoveField("defaultObjectAcl");
  return *this;
}

BucketMetadataPatchBuilder& BucketMetadataPatchBuilder::SetEncryption(
    BucketEncryption const& v) {
  impl_.AddSubPatch("encryption",
                    internal::PatchBuilder().SetStringField(
                        "defaultKmsKeyName", v.default_kms_key_name));
  return *this;
}

BucketMetadataPatchBuilder& BucketMetadataPatchBuilder::ResetEncryption() {
  impl_.RemoveField("encryption");
  return *this;
}

BucketMetadataPatchBuilder& BucketMetadataPatchBuilder::SetIamConfiguration(
    BucketIamConfiguration const& v) {
  internal::PatchBuilder iam_configuration;
  if (v.bucket_only_policy.has_value()) {
    internal::PatchBuilder bucket_only_policy;
    bucket_only_policy.SetBoolField("enabled", v.bucket_only_policy->enabled);
    // The lockedTime field should not be set, this is not a mutable field, it
    // is set by the server when the policy is enabled.
    iam_configuration.AddSubPatch("bucketOnlyPolicy", bucket_only_policy);
  }
  impl_.AddSubPatch("iamConfiguration", iam_configuration);
  return *this;
}

BucketMetadataPatchBuilder&
BucketMetadataPatchBuilder::ResetIamConfiguration() {
  impl_.RemoveField("iamConfiguration");
  return *this;
}

BucketMetadataPatchBuilder& BucketMetadataPatchBuilder::SetLabel(
    std::string const& label, std::string const& value) {
  labels_subpatch_.SetStringField(label.c_str(), value);
  labels_subpatch_dirty_ = true;
  return *this;
}

BucketMetadataPatchBuilder& BucketMetadataPatchBuilder::ResetLabel(
    std::string const& label) {
  labels_subpatch_.RemoveField(label.c_str());
  labels_subpatch_dirty_ = true;
  return *this;
}

BucketMetadataPatchBuilder& BucketMetadataPatchBuilder::ResetLabels() {
  labels_subpatch_.clear();
  labels_subpatch_dirty_ = true;
  return *this;
}

BucketMetadataPatchBuilder& BucketMetadataPatchBuilder::SetLifecycle(
    BucketLifecycle const& v) {
  if (v.rule.empty()) {
    return ResetLifecycle();
  }
  internal::PatchBuilder subpatch;
  std::vector<internal::nl::json> array;
  array.reserve(v.rule.size());
  for (auto const& a : v.rule) {
    internal::nl::json condition;
    auto const& c = a.condition();
    if (c.age.has_value()) {
      condition["age"] = *c.age;
    }
    if (c.created_before.has_value()) {
      condition["createdBefore"] = internal::FormatRfc3339(*c.created_before);
    }
    if (c.is_live.has_value()) {
      condition["isLive"] = *c.is_live;
    }
    if (c.matches_storage_class.has_value()) {
      condition["matchesStorageClass"] = *c.matches_storage_class;
    }
    if (c.num_newer_versions.has_value()) {
      condition["numNewerVersions"] = *c.num_newer_versions;
    }
    internal::nl::json action;
    if (not a.action().type.empty()) {
      action["type"] = a.action().type;
    }
    if (not a.action().storage_class.empty()) {
      action["storageClass"] = a.action().storage_class;
    }
    array.emplace_back(internal::nl::json{
        {"action", action},
        {"condition", condition},
    });
  }
  subpatch.SetArrayField("rule", array);
  impl_.AddSubPatch("lifecycle", subpatch);
  return *this;
}

BucketMetadataPatchBuilder& BucketMetadataPatchBuilder::ResetLifecycle() {
  impl_.RemoveField("lifecycle");
  return *this;
}

BucketMetadataPatchBuilder& BucketMetadataPatchBuilder::SetLogging(
    BucketLogging const& v) {
  impl_.AddSubPatch(
      "logging", internal::PatchBuilder()
                     .SetStringField("logBucket", v.log_bucket)
                     .SetStringField("logObjectPrefix", v.log_object_prefix));
  return *this;
}

BucketMetadataPatchBuilder& BucketMetadataPatchBuilder::ResetLogging() {
  impl_.RemoveField("logging");
  return *this;
}

BucketMetadataPatchBuilder& BucketMetadataPatchBuilder::SetName(
    std::string const& v) {
  if (v.empty()) {
    return ResetName();
  }
  impl_.SetStringField("name", v);
  return *this;
}

BucketMetadataPatchBuilder& BucketMetadataPatchBuilder::ResetName() {
  impl_.RemoveField("name");
  return *this;
}

BucketMetadataPatchBuilder& BucketMetadataPatchBuilder::SetRetentionPolicy(
    BucketRetentionPolicy const& v) {
  // Only the retentionPeriod field is writeable, so do not modify the other
  // fields.
  impl_.AddSubPatch("retentionPolicy",
                    internal::PatchBuilder().SetIntField(
                        "retentionPeriod", static_cast<std::uint64_t>(v.retention_period.count())));
  return *this;
}

BucketMetadataPatchBuilder& BucketMetadataPatchBuilder::ResetRetentionPolicy() {
  impl_.RemoveField("retentionPolicy");
  return *this;
}

BucketMetadataPatchBuilder& BucketMetadataPatchBuilder::SetStorageClass(
    std::string const& v) {
  if (v.empty()) {
    return ResetStorageClass();
  }
  impl_.SetStringField("storageClass", v);
  return *this;
}

BucketMetadataPatchBuilder& BucketMetadataPatchBuilder::ResetStorageClass() {
  impl_.RemoveField("storageClass");
  return *this;
}

BucketMetadataPatchBuilder& BucketMetadataPatchBuilder::SetVersioning(
    BucketVersioning const& v) {
  impl_.AddSubPatch("versioning", internal::PatchBuilder().SetBoolField(
                                      "enabled", v.enabled));
  return *this;
}

BucketMetadataPatchBuilder& BucketMetadataPatchBuilder::ResetVersioning() {
  impl_.RemoveField("versioning");
  return *this;
}

BucketMetadataPatchBuilder& BucketMetadataPatchBuilder::SetWebsite(
    BucketWebsite const& v) {
  impl_.AddSubPatch("website",
                    internal::PatchBuilder()
                        .SetStringField("mainPageSuffix", v.main_page_suffix)
                        .SetStringField("notFoundPage", v.not_found_page));
  return *this;
}

BucketMetadataPatchBuilder& BucketMetadataPatchBuilder::ResetWebsite() {
  impl_.RemoveField("website");
  return *this;
}

}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google
