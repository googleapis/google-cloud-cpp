// Copyright 2023 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "google/cloud/bigquery/v2/minimal/internal/dataset.h"
#include "google/cloud/bigquery/v2/minimal/internal/json_utils.h"
#include "google/cloud/internal/debug_string.h"
#include "google/cloud/internal/format_time_point.h"

namespace google {
namespace cloud {
namespace bigquery_v2_minimal_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

StorageBillingModel StorageBillingModel::UnSpecified() {
  return StorageBillingModel{"STORAGE_BILLING_MODEL_UNSPECIFIED"};
}

StorageBillingModel StorageBillingModel::Logical() {
  return StorageBillingModel{"LOGICAL"};
}

StorageBillingModel StorageBillingModel::Physical() {
  return StorageBillingModel{"PHYSICAL"};
}

TargetType TargetType::UnSpecified() {
  return TargetType{"TARGET_TYPE_UNSPECIFIED"};
}

TargetType TargetType::Views() { return TargetType{"VIEWS"}; }

TargetType TargetType::Routines() { return TargetType{"ROUTINES"}; }

void to_json(nlohmann::json& j, Dataset const& d) {
  j = nlohmann::json{{"kind", d.kind},
                     {"etag", d.etag},
                     {"id", d.id},
                     {"selfLink", d.self_link},
                     {"friendlyName", d.friendly_name},
                     {"description", d.description},
                     {"type", d.type},
                     {"location", d.location},
                     {"defaultCollation", d.default_collation},
                     {"published", d.published},
                     {"isCaseInsensitive", d.is_case_insensitive},
                     {"labels", d.labels},
                     {"access", d.access},
                     {"tags", d.tags},
                     {"datasetReference", d.dataset_reference},
                     {"linkedDatasetSource", d.linked_dataset_source},
                     {"defaultRoundingMode", d.default_rounding_mode},
                     {"storageBillingModel", d.storage_billing_model}};

  ToJson(d.default_table_expiration, j, "defaultTableExpirationMs");
  ToJson(d.default_partition_expiration, j, "defaultPartitionExpirationMs");
  ToJson(d.creation_time, j, "creationTime");
  ToJson(d.last_modified_time, j, "lastModifiedTime");
  ToJson(d.max_time_travel, j, "maxTimeTravelHours");
}

void from_json(nlohmann::json const& j, Dataset& d) {
  if (j.contains("kind")) j.at("kind").get_to(d.kind);
  if (j.contains("etag")) j.at("etag").get_to(d.etag);
  if (j.contains("id")) j.at("id").get_to(d.id);
  if (j.contains("selfLink")) j.at("selfLink").get_to(d.self_link);
  if (j.contains("friendlyName")) j.at("friendlyName").get_to(d.friendly_name);
  if (j.contains("description")) j.at("description").get_to(d.description);
  if (j.contains("type")) j.at("type").get_to(d.type);
  if (j.contains("location")) j.at("location").get_to(d.location);
  if (j.contains("defaultCollation"))
    j.at("defaultCollation").get_to(d.default_collation);
  if (j.contains("published")) j.at("published").get_to(d.published);
  if (j.contains("isCaseInsensitive"))
    j.at("isCaseInsensitive").get_to(d.is_case_insensitive);
  if (j.contains("labels")) j.at("labels").get_to(d.labels);
  if (j.contains("access")) j.at("access").get_to(d.access);
  if (j.contains("tags")) j.at("tags").get_to(d.tags);
  if (j.contains("datasetReference"))
    j.at("datasetReference").get_to(d.dataset_reference);
  if (j.contains("linkedDatasetSource"))
    j.at("linkedDatasetSource").get_to(d.linked_dataset_source);
  if (j.contains("defaultRoundingMode"))
    j.at("defaultRoundingMode").get_to(d.default_rounding_mode);
  if (j.contains("storageBillingModel"))
    j.at("storageBillingModel").get_to(d.storage_billing_model);

  FromJson(d.default_table_expiration, j, "defaultTableExpirationMs");
  FromJson(d.default_partition_expiration, j, "defaultPartitionExpirationMs");
  FromJson(d.creation_time, j, "creationTime");
  FromJson(d.last_modified_time, j, "lastModifiedTime");
  FromJson(d.max_time_travel, j, "maxTimeTravelHours");
}

std::string LinkedDatasetSource::DebugString(absl::string_view name,
                                             TracingOptions const& options,
                                             int indent) const {
  return internal::DebugFormatter(name, options, indent)
      .SubMessage("source_dataset", source_dataset)
      .Build();
}

std::string TargetType::DebugString(absl::string_view name,
                                    TracingOptions const& options,
                                    int indent) const {
  return internal::DebugFormatter(name, options, indent)
      .StringField("target_type_value", value)
      .Build();
}

std::string StorageBillingModel::DebugString(absl::string_view name,
                                             TracingOptions const& options,
                                             int indent) const {
  return internal::DebugFormatter(name, options, indent)
      .StringField("storage_billing_model_value", value)
      .Build();
}

std::string DatasetAccessEntry::DebugString(absl::string_view name,
                                            TracingOptions const& options,
                                            int indent) const {
  return internal::DebugFormatter(name, options, indent)
      .SubMessage("dataset", dataset)
      .Field("target_types", target_types)
      .Build();
}

std::string Access::DebugString(absl::string_view name,
                                TracingOptions const& options,
                                int indent) const {
  return internal::DebugFormatter(name, options, indent)
      .StringField("role", role)
      .StringField("user_by_email", user_by_email)
      .StringField("group_by_email", group_by_email)
      .StringField("domain", domain)
      .StringField("special_group", special_group)
      .StringField("iam_member", iam_member)
      .SubMessage("view", view)
      .SubMessage("routine", routine)
      .SubMessage("dataset", dataset)
      .Build();
}

std::string GcpTag::DebugString(absl::string_view name,
                                TracingOptions const& options,
                                int indent) const {
  return internal::DebugFormatter(name, options, indent)
      .StringField("tag_key", tag_key)
      .StringField("tag_value", tag_value)
      .Build();
}

std::string Dataset::DebugString(absl::string_view name,
                                 TracingOptions const& options,
                                 int indent) const {
  return internal::DebugFormatter(name, options, indent)
      .StringField("kind", kind)
      .StringField("etag", etag)
      .StringField("id", id)
      .StringField("self_link", self_link)
      .StringField("friendly_name", friendly_name)
      .StringField("description", description)
      .StringField("type", type)
      .StringField("location", location)
      .StringField("default_collation", default_collation)
      .Field("published", published)
      .Field("is_case_insensitive", is_case_insensitive)
      .Field("default_table_expiration", default_table_expiration)
      .Field("default_partition_expiration", default_partition_expiration)
      .Field("creation_time", creation_time)
      .Field("last_modified_time", last_modified_time)
      .Field("max_time_travel", max_time_travel)
      .Field("labels", labels)
      .Field("access", access)
      .Field("tags", tags)
      .SubMessage("dataset_reference", dataset_reference)
      .SubMessage("linked_dataset_source", linked_dataset_source)
      .SubMessage("default_rounding_mode", default_rounding_mode)
      .SubMessage("storage_billing_model", storage_billing_model)
      .Build();
}

std::string ListFormatDataset::DebugString(absl::string_view name,
                                           TracingOptions const& options,
                                           int indent) const {
  return internal::DebugFormatter(name, options, indent)
      .StringField("kind", kind)
      .StringField("id", id)
      .StringField("friendly_name", friendly_name)
      .StringField("location", location)
      .StringField("type", type)
      .SubMessage("dataset_reference", dataset_reference)
      .Field("labels", labels)
      .Build();
}

void to_json(nlohmann::json& j, ListFormatDataset const& d) {
  j = nlohmann::json{{"kind", d.kind},
                     {"id", d.id},
                     {"friendlyName", d.friendly_name},
                     {"location", d.location},
                     {"type", d.type},
                     {"datasetReference", d.dataset_reference},
                     {"labels", d.labels}};
}
void from_json(nlohmann::json const& j, ListFormatDataset& d) {
  // TODO(#12188): Implement SafeGetTo(...) for potentially better performance.
  if (j.contains("kind")) j.at("kind").get_to(d.kind);
  if (j.contains("id")) j.at("id").get_to(d.id);
  if (j.contains("friendlyName")) j.at("friendlyName").get_to(d.friendly_name);
  if (j.contains("location")) j.at("location").get_to(d.location);
  if (j.contains("type")) j.at("type").get_to(d.type);
  if (j.contains("datasetReference")) {
    j.at("datasetReference").get_to(d.dataset_reference);
  }
  if (j.contains("labels")) j.at("labels").get_to(d.labels);
}

void to_json(nlohmann::json& j, GcpTag const& t) {
  j = nlohmann::json{{"tagKey", t.tag_key}, {"tagValue", t.tag_value}};
}
void from_json(nlohmann::json const& j, GcpTag& t) {
  // TODO(#12188): Implement SafeGetTo(...) for potentially better performance.
  if (j.contains("tagKey")) j.at("tagKey").get_to(t.tag_key);
  if (j.contains("tagValue")) j.at("tagValue").get_to(t.tag_value);
}

void to_json(nlohmann::json& j, Access const& a) {
  j = nlohmann::json{{"role", a.role},
                     {"userByEmail", a.user_by_email},
                     {"groupByEmail", a.group_by_email},
                     {"domain", a.domain},
                     {"specialGroup", a.special_group},
                     {"iamMember", a.iam_member},
                     {"view", a.view},
                     {"routine", a.routine},
                     {"dataset", a.dataset}};
}
void from_json(nlohmann::json const& j, Access& a) {
  // TODO(#12188): Implement SafeGetTo(...) for potentially better performance.
  if (j.contains("role")) j.at("role").get_to(a.role);
  if (j.contains("userByEmail")) j.at("userByEmail").get_to(a.user_by_email);
  if (j.contains("groupByEmail")) j.at("groupByEmail").get_to(a.group_by_email);
  if (j.contains("domain")) j.at("domain").get_to(a.domain);
  if (j.contains("specialGroup")) j.at("specialGroup").get_to(a.special_group);
  if (j.contains("iamMember")) j.at("iamMember").get_to(a.iam_member);
  if (j.contains("view")) j.at("view").get_to(a.view);
  if (j.contains("routine")) j.at("routine").get_to(a.routine);
  if (j.contains("dataset")) j.at("dataset").get_to(a.dataset);
}

void to_json(nlohmann::json& j, DatasetAccessEntry const& d) {
  j = nlohmann::json{{"dataset", d.dataset}, {"targetTypes", d.target_types}};
}
void from_json(nlohmann::json const& j, DatasetAccessEntry& d) {
  // TODO(#12188): Implement SafeGetTo(...) for potentially better performance.
  if (j.contains("dataset")) j.at("dataset").get_to(d.dataset);
  if (j.contains("targetTypes")) j.at("targetTypes").get_to(d.target_types);
}

void to_json(nlohmann::json& j, LinkedDatasetSource const& d) {
  j = nlohmann::json{{"sourceDataset", d.source_dataset}};
}
void from_json(nlohmann::json const& j, LinkedDatasetSource& d) {
  // TODO(#12188): Implement SafeGetTo(...) for potentially better performance.
  if (j.contains("sourceDataset")) {
    j.at("sourceDataset").get_to(d.source_dataset);
  }
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigquery_v2_minimal_internal
}  // namespace cloud
}  // namespace google
