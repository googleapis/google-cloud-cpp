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

#include "google/cloud/storage/bucket_metadata.h"
#include "google/cloud/storage/internal/bucket_metadata_parser.h"
#include "google/cloud/internal/absl_str_join_quiet.h"
#include "google/cloud/internal/format_time_point.h"
#include "google/cloud/internal/ios_flags_saver.h"
#include "google/cloud/status.h"
#include "absl/strings/str_format.h"
#include <nlohmann/json.hpp>
#include <string>
#include <utility>
#include <vector>

namespace google {
namespace cloud {
namespace storage {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {
nlohmann::json ConditionAsPatch(LifecycleRuleCondition const& c) {
  nlohmann::json condition;
  if (c.age.has_value()) {
    condition["age"] = *c.age;
  }
  if (c.created_before.has_value()) {
    condition["createdBefore"] =
        absl::StrFormat("%04d-%02d-%02d", c.created_before->year(),
                        c.created_before->month(), c.created_before->day());
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
  if (c.days_since_noncurrent_time.has_value()) {
    condition["daysSinceNoncurrentTime"] = *c.days_since_noncurrent_time;
  }
  if (c.noncurrent_time_before.has_value()) {
    condition["noncurrentTimeBefore"] =
        internal::ToJsonString(*c.noncurrent_time_before);
  }
  if (c.days_since_custom_time.has_value()) {
    condition["daysSinceCustomTime"] = *c.days_since_custom_time;
  }
  if (c.custom_time_before.has_value()) {
    condition["customTimeBefore"] =
        internal::ToJsonString(*c.custom_time_before);
  }
  if (c.matches_prefix.has_value()) {
    condition["matchesPrefix"] = *c.matches_prefix;
  }
  if (c.matches_suffix.has_value()) {
    condition["matchesSuffix"] = *c.matches_suffix;
  }
  return condition;
}

nlohmann::json ActionAsPatch(LifecycleRuleAction const& a) {
  nlohmann::json action;
  if (!a.type.empty()) {
    action["type"] = a.type;
  }
  if (!a.storage_class.empty()) {
    action["storageClass"] = a.storage_class;
  }
  return action;
}

}  // namespace

bool operator==(BucketMetadata const& lhs, BucketMetadata const& rhs) {
  return lhs.acl_ == rhs.acl_                                               //
         && lhs.autoclass_ == rhs.autoclass_                                //
         && lhs.billing_ == rhs.billing_                                    //
         && lhs.cors_ == rhs.cors_                                          //
         && lhs.custom_placement_config_ == rhs.custom_placement_config_    //
         && lhs.default_acl_ == rhs.default_acl_                            //
         && lhs.default_event_based_hold_ == rhs.default_event_based_hold_  //
         && lhs.encryption_ == rhs.encryption_                              //
         && lhs.etag_ == rhs.etag_                                          //
         && lhs.hierarchical_namespace_ == rhs.hierarchical_namespace_      //
         && lhs.iam_configuration_ == rhs.iam_configuration_                //
         && lhs.id_ == rhs.id_                                              //
         && lhs.kind_ == rhs.kind_                                          //
         && lhs.labels_ == rhs.labels_                                      //
         && lhs.lifecycle_ == rhs.lifecycle_                                //
         && lhs.location_ == rhs.location_                                  //
         && lhs.location_type_ == rhs.location_type_                        //
         && lhs.logging_ == rhs.logging_                                    //
         && lhs.metageneration_ == rhs.metageneration_                      //
         && lhs.name_ == rhs.name_                                          //
         && lhs.object_retention_ == rhs.object_retention_                  //
         && lhs.owner_ == rhs.owner_                                        //
         && lhs.project_number_ == rhs.project_number_                      //
         && lhs.retention_policy_ == rhs.retention_policy_                  //
         && lhs.rpo_ == rhs.rpo_                                            //
         && lhs.self_link_ == rhs.self_link_                                //
         && lhs.soft_delete_policy_ == rhs.soft_delete_policy_              //
         && lhs.storage_class_ == rhs.storage_class_                        //
         && lhs.time_created_ == rhs.time_created_                          //
         && lhs.updated_ == rhs.updated_                                    //
         && lhs.versioning_ == rhs.versioning_                              //
         && lhs.website_ == rhs.website_                                    //
      ;
}

std::ostream& operator<<(std::ostream& os, BucketMetadata const& rhs) {
  google::cloud::internal::IosFlagsSaver save_format(os);
  os << "BucketMetadata={name=" << rhs.name();

  os << ", acl=[";
  os << absl::StrJoin(rhs.acl(), ", ", absl::StreamFormatter());
  os << "]";

  if (rhs.has_autoclass()) {
    os << ", autoclass=" << rhs.autoclass();
  }
  if (rhs.has_billing()) {
    auto previous_flags = os.flags();
    os << ", billing.requesterPays=" << std::boolalpha
       << rhs.billing().requester_pays;
    os.flags(previous_flags);
  }

  os << ", cors=[";
  os << absl::StrJoin(rhs.cors(), ", ", absl::StreamFormatter());
  os << "]";

  os << ", default_event_based_hold=" << std::boolalpha
     << rhs.default_event_based_hold();

  os << ", default_acl=[";
  os << absl::StrJoin(rhs.default_acl(), ", ", absl::StreamFormatter());
  os << "]";

  if (rhs.has_encryption()) {
    os << ", encryption.default_kms_key_name="
       << rhs.encryption().default_kms_key_name;
  }

  os << ", etag=" << rhs.etag();

  if (rhs.has_hierarchical_namespace()) {
    os << ", hierarchical_namespace=" << rhs.hierarchical_namespace();
  }

  if (rhs.has_iam_configuration()) {
    os << ", iam_configuration=" << rhs.iam_configuration();
  }

  os << ", id=" << rhs.id() << ", kind=" << rhs.kind();

  for (auto const& kv : rhs.labels_) {
    os << ", labels." << kv.first << "=" << kv.second;
  }

  if (rhs.has_lifecycle()) {
    os << ", lifecycle.rule=[";
    os << absl::StrJoin(rhs.lifecycle().rule, ", ", absl::StreamFormatter());
    os << "]";
  }

  os << ", location=" << rhs.location();

  os << ", location_type=" << rhs.location_type();

  if (rhs.has_logging()) {
    os << ", logging=" << rhs.logging();
  }

  os << ", metageneration=" << rhs.metageneration() << ", name=" << rhs.name();

  if (rhs.has_object_retention()) {
    os << ", object_retention=" << rhs.object_retention();
  }
  if (rhs.has_owner()) {
    os << ", owner.entity=" << rhs.owner().entity
       << ", owner.entity_id=" << rhs.owner().entity_id;
  }

  os << ", project_number=" << rhs.project_number()
     << ", self_link=" << rhs.self_link();
  if (rhs.has_soft_delete_policy()) {
    os << ", soft_delete_policy=" << rhs.soft_delete_policy();
  }
  os << ", storage_class=" << rhs.storage_class() << ", time_created="
     << google::cloud::internal::FormatRfc3339(rhs.time_created())
     << ", updated=" << google::cloud::internal::FormatRfc3339(rhs.updated());

  if (rhs.has_retention_policy()) {
    os << ", retention_policy.retention_period="
       << rhs.retention_policy().retention_period.count()
       << ", retention_policy.effective_time="
       << google::cloud::internal::FormatRfc3339(
              rhs.retention_policy().effective_time)
       << ", retention_policy.is_locked=" << std::boolalpha
       << rhs.retention_policy().is_locked;
  }

  os << ", rpo=" << rhs.rpo();

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

  if (rhs.has_custom_placement_config()) {
    os << ", custom_placement_config.data_locations=["
       << absl::StrJoin(rhs.custom_placement_config().data_locations, ", ")
       << "]";
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
  auto array = nlohmann::json::array();
  for (auto const& a : v) {
    array.emplace_back(nlohmann::json{
        {"entity", a.entity()},
        {"role", a.role()},
    });
  }
  impl_.SetArrayField("acl", array.dump());
  return *this;
}

BucketMetadataPatchBuilder& BucketMetadataPatchBuilder::ResetAcl() {
  impl_.RemoveField("acl");
  return *this;
}

BucketMetadataPatchBuilder& BucketMetadataPatchBuilder::SetAutoclass(
    BucketAutoclass const& v) {
  auto builder = internal::PatchBuilder().SetBoolField("enabled", v.enabled);
  if (!v.terminal_storage_class.empty()) {
    builder.SetStringField("terminalStorageClass", v.terminal_storage_class);
  }
  impl_.AddSubPatch("autoclass", std::move(builder));
  return *this;
}

BucketMetadataPatchBuilder& BucketMetadataPatchBuilder::ResetAutoclass() {
  impl_.RemoveField("autoclass");
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
  auto array = nlohmann::json::array();
  for (auto const& a : v) {
    nlohmann::json entry;
    if (a.max_age_seconds.has_value()) {
      entry["maxAgeSeconds"] = *a.max_age_seconds;
    }
    if (!a.method.empty()) {
      entry["method"] = a.method;
    }
    if (!a.origin.empty()) {
      entry["origin"] = a.origin;
    }
    if (!a.response_header.empty()) {
      entry["responseHeader"] = a.response_header;
    }
    array.emplace_back(std::move(entry));
  }
  impl_.SetArrayField("cors", array.dump());
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
  auto array = nlohmann::json::array();
  for (auto const& a : v) {
    array.emplace_back(nlohmann::json{
        {"entity", a.entity()},
        {"role", a.role()},
    });
  }
  impl_.SetArrayField("defaultObjectAcl", array.dump());
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

  if (v.public_access_prevention.has_value()) {
    iam_configuration.SetStringField("publicAccessPrevention",
                                     *v.public_access_prevention);
  }
  if (v.uniform_bucket_level_access.has_value()) {
    internal::PatchBuilder uniform_bucket_level_access;
    uniform_bucket_level_access.SetBoolField(
        "enabled", v.uniform_bucket_level_access->enabled);
    // The lockedTime field should not be set, this is not a mutable field, it
    // is set by the server when the policy is enabled.
    iam_configuration.AddSubPatch("uniformBucketLevelAccess",
                                  uniform_bucket_level_access);
    impl_.AddSubPatch("iamConfiguration", iam_configuration);
    return *this;
  }
  impl_.AddSubPatch("iamConfiguration", iam_configuration);
  return *this;
}

BucketMetadataPatchBuilder&
BucketMetadataPatchBuilder::ResetIamConfiguration() {
  impl_.RemoveField("iamConfiguration");
  return *this;
}

BucketMetadataPatchBuilder&
BucketMetadataPatchBuilder::SetHierarchicalNamespace(
    BucketHierarchicalNamespace const& v) {
  internal::PatchBuilder subpatch;
  subpatch.SetBoolField("enabled", v.enabled);
  impl_.AddSubPatch("hierarchicalNamespace", subpatch);
  return *this;
}

BucketMetadataPatchBuilder&
BucketMetadataPatchBuilder::ResetHierarchicalNamespace() {
  impl_.RemoveField("hierarchicalNamespace");
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
  auto array = nlohmann::json::array();
  for (auto const& a : v.rule) {
    auto condition = ConditionAsPatch(a.condition());
    auto action = ActionAsPatch(a.action());
    array.emplace_back(nlohmann::json{
        {"action", action},
        {"condition", condition},
    });
  }
  subpatch.SetArrayField("rule", array.dump());
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
                        "retentionPeriod", static_cast<std::uint64_t>(
                                               v.retention_period.count())));
  return *this;
}

BucketMetadataPatchBuilder& BucketMetadataPatchBuilder::ResetRetentionPolicy() {
  impl_.RemoveField("retentionPolicy");
  return *this;
}

BucketMetadataPatchBuilder& BucketMetadataPatchBuilder::SetRpo(
    std::string const& v) {
  if (v.empty()) return ResetRpo();
  impl_.SetStringField("rpo", v);
  return *this;
}

BucketMetadataPatchBuilder& BucketMetadataPatchBuilder::ResetRpo() {
  impl_.RemoveField("rpo");
  return *this;
}

BucketMetadataPatchBuilder& BucketMetadataPatchBuilder::SetSoftDeletePolicy(
    BucketSoftDeletePolicy const& v) {
  // Only the retentionDurationSeconds field is writeable, so do not modify the
  // other fields.
  impl_.AddSubPatch(
      "softDeletePolicy",
      internal::PatchBuilder().SetIntField(
          "retentionDurationSeconds",
          static_cast<std::uint64_t>(v.retention_duration.count())));
  return *this;
}

BucketMetadataPatchBuilder&
BucketMetadataPatchBuilder::ResetSoftDeletePolicy() {
  impl_.RemoveField("softDeletePolicy");
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

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google
