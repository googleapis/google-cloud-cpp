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
                     {"defaultRoundingMode", d.default_rounding_mode.value},
                     {"storageBillingModel", d.storage_billing_model.value}};

  ToJson(d.default_table_expiration, j, "defaultTableExpirationMs");
  ToJson(d.default_partition_expiration, j, "defaultPartitionExpirationMs");
  ToJson(d.creation_time, j, "creationTime");
  ToJson(d.last_modified_time, j, "lastModifiedTime");
  ToJson(d.max_time_travel, j, "maxTimeTravelHours");
}

void from_json(nlohmann::json const& j, Dataset& d) {
  SafeGetTo(d.kind, j, "kind");
  SafeGetTo(d.etag, j, "etag");
  SafeGetTo(d.id, j, "id");
  SafeGetTo(d.self_link, j, "selfLink");
  SafeGetTo(d.friendly_name, j, "friendlyName");
  SafeGetTo(d.description, j, "description");
  SafeGetTo(d.type, j, "type");
  SafeGetTo(d.location, j, "location");
  SafeGetTo(d.default_collation, j, "defaultCollation");
  SafeGetTo(d.published, j, "published");
  SafeGetTo(d.is_case_insensitive, j, "isCaseInsensitive");
  SafeGetTo(d.labels, j, "labels");
  SafeGetTo(d.access, j, "access");
  SafeGetTo(d.tags, j, "tags");
  SafeGetTo(d.dataset_reference, j, "datasetReference");
  SafeGetTo(d.linked_dataset_source, j, "linkedDatasetSource");
  SafeGetTo(d.default_rounding_mode.value, j, "defaultRoundingMode");
  SafeGetTo(d.storage_billing_model.value, j, "storageBillingModel");

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
  SafeGetTo(d.kind, j, "kind");
  SafeGetTo(d.id, j, "id");
  SafeGetTo(d.friendly_name, j, "friendlyName");
  SafeGetTo(d.location, j, "location");
  SafeGetTo(d.type, j, "type");
  SafeGetTo(d.dataset_reference, j, "datasetReference");
  SafeGetTo(d.labels, j, "labels");
}

void to_json(nlohmann::json& j, GcpTag const& t) {
  j = nlohmann::json{{"tagKey", t.tag_key}, {"tagValue", t.tag_value}};
}
void from_json(nlohmann::json const& j, GcpTag& t) {
  SafeGetTo(t.tag_key, j, "tagKey");
  SafeGetTo(t.tag_value, j, "tagValue");
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
  SafeGetTo(a.role, j, "role");
  SafeGetTo(a.user_by_email, j, "userByEmail");
  SafeGetTo(a.group_by_email, j, "groupByEmail");
  SafeGetTo(a.domain, j, "domain");
  SafeGetTo(a.special_group, j, "specialGroup");
  SafeGetTo(a.iam_member, j, "iamMember");
  SafeGetTo(a.view, j, "view");
  SafeGetTo(a.routine, j, "routine");
  SafeGetTo(a.dataset, j, "dataset");
}

void to_json(nlohmann::json& j, DatasetAccessEntry const& d) {
  j = nlohmann::json{{"dataset", d.dataset}, {"targetTypes", d.target_types}};
}
void from_json(nlohmann::json const& j, DatasetAccessEntry& d) {
  SafeGetTo(d.dataset, j, "dataset");
  SafeGetTo(d.target_types, j, "targetTypes");
}

void to_json(nlohmann::json& j, LinkedDatasetSource const& d) {
  j = nlohmann::json{{"sourceDataset", d.source_dataset}};
}
void from_json(nlohmann::json const& j, LinkedDatasetSource& d) {
  SafeGetTo(d.source_dataset, j, "sourceDataset");
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigquery_v2_minimal_internal
}  // namespace cloud
}  // namespace google
