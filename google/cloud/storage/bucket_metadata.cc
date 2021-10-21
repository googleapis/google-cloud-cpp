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
#include "google/cloud/storage/internal/bucket_acl_requests.h"
#include "google/cloud/storage/internal/bucket_requests.h"
#include "google/cloud/storage/internal/metadata_parser.h"
#include "google/cloud/storage/internal/object_acl_requests.h"
#include "google/cloud/internal/absl_str_join_quiet.h"
#include "google/cloud/internal/format_time_point.h"
#include "google/cloud/internal/ios_flags_saver.h"
#include "google/cloud/status.h"
#include "absl/strings/str_format.h"
#include <nlohmann/json.hpp>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {

std::ostream& operator<<(std::ostream& os, CorsEntry const& rhs) {
  os << "CorsEntry={";
  char const* sep = "";
  if (rhs.max_age_seconds.has_value()) {
    os << sep << "max_age_seconds=" << *rhs.max_age_seconds;
    sep = ", ";
  }
  return os << sep << "method=[" << absl::StrJoin(rhs.method, ", ")
            << "], origin=[" << absl::StrJoin(rhs.origin, ", ")
            << "], response_header=["
            << absl::StrJoin(rhs.response_header, ", ") << "]}";
}

std::ostream& operator<<(std::ostream& os,
                         UniformBucketLevelAccess const& rhs) {
  google::cloud::internal::IosFlagsSaver save_format(os);
  return os << "UniformBucketLevelAccess={enabled=" << std::boolalpha
            << rhs.enabled << ", locked_time="
            << google::cloud::internal::FormatRfc3339(rhs.locked_time) << "}";
}

std::ostream& operator<<(std::ostream& os, BucketIamConfiguration const& rhs) {
  os << "BucketIamConfiguration={";
  char const* sep = "";
  if (rhs.public_access_prevention.has_value()) {
    os << sep << "public_access_prevention=" << *rhs.public_access_prevention;
    sep = ", ";
  }
  if (rhs.uniform_bucket_level_access.has_value()) {
    os << sep
       << "uniform_bucket_level_access=" << *rhs.uniform_bucket_level_access;
    return os << "}";
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
            << google::cloud::internal::FormatRfc3339(rhs.effective_time)
            << ", locked=" << rhs.is_locked << "}";
}

bool operator==(BucketMetadata const& lhs, BucketMetadata const& rhs) {
  return static_cast<internal::CommonMetadata<BucketMetadata> const&>(lhs) ==
             rhs &&
         lhs.acl_ == rhs.acl_ && lhs.billing_ == rhs.billing_ &&
         lhs.cors_ == rhs.cors_ &&
         lhs.default_event_based_hold_ == rhs.default_event_based_hold_ &&
         lhs.default_acl_ == rhs.default_acl_ &&
         lhs.encryption_ == rhs.encryption_ &&
         lhs.iam_configuration_ == rhs.iam_configuration_ &&
         lhs.project_number_ == rhs.project_number_ &&
         lhs.lifecycle_ == rhs.lifecycle_ && lhs.location_ == rhs.location_ &&
         lhs.location_type_ == rhs.location_type_ &&
         lhs.logging_ == rhs.logging_ && lhs.labels_ == rhs.labels_ &&
         lhs.retention_policy_ == rhs.retention_policy_ &&
         lhs.versioning_ == rhs.versioning_ && lhs.website_ == rhs.website_;
}

std::ostream& operator<<(std::ostream& os, BucketMetadata const& rhs) {
  google::cloud::internal::IosFlagsSaver save_format(os);
  os << "BucketMetadata={name=" << rhs.name();

  os << ", acl=[";
  os << absl::StrJoin(rhs.acl(), ", ", absl::StreamFormatter());
  os << "]";

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

  if (rhs.has_owner()) {
    os << ", owner.entity=" << rhs.owner().entity
       << ", owner.entity_id=" << rhs.owner().entity_id;
  }

  os << ", project_number=" << rhs.project_number()
     << ", self_link=" << rhs.self_link()
     << ", storage_class=" << rhs.storage_class() << ", time_created="
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
    nlohmann::json condition;
    auto const& c = a.condition();
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
    nlohmann::json action;
    if (!a.action().type.empty()) {
      action["type"] = a.action().type;
    }
    if (!a.action().storage_class.empty()) {
      action["storageClass"] = a.action().storage_class;
    }
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
