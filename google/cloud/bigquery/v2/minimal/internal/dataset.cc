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
  j = nlohmann::json{
      {"kind", d.kind},
      {"etag", d.etag},
      {"id", d.id},
      {"self_link", d.self_link},
      {"friendly_name", d.friendly_name},
      {"description", d.description},
      {"type", d.type},
      {"location", d.location},
      {"default_collation", d.default_collation},
      {"published", d.published},
      {"is_case_insensitive", d.is_case_insensitive},
      {"labels", d.labels},
      {"access", d.access},
      {"tags", d.tags},
      {"dataset_reference", d.dataset_reference},

      {"linked_dataset_source", d.linked_dataset_source},
      {"external_dataset_reference", d.external_dataset_reference},
      {"default_rounding_mode", d.default_rounding_mode},
      {"storage_billing_model", d.storage_billing_model}};

  ToJson(d.default_table_expiration, j, "default_table_expiration");
  ToJson(d.default_partition_expiration, j, "default_partition_expiration");
  ToJson(d.creation_time, j, "creation_time");
  ToJson(d.last_modified_time, j, "last_modified_time");
  ToJson(d.max_time_travel, j, "max_time_travel");
}

void from_json(nlohmann::json const& j, Dataset& d) {
  if (j.contains("kind")) j.at("kind").get_to(d.kind);
  if (j.contains("etag")) j.at("etag").get_to(d.etag);
  if (j.contains("id")) j.at("id").get_to(d.id);
  if (j.contains("self_link")) j.at("self_link").get_to(d.self_link);
  if (j.contains("friendly_name"))
    j.at("friendly_name").get_to(d.friendly_name);
  if (j.contains("description")) j.at("description").get_to(d.description);
  if (j.contains("type")) j.at("type").get_to(d.type);
  if (j.contains("location")) j.at("location").get_to(d.location);
  if (j.contains("default_collation"))
    j.at("default_collation").get_to(d.default_collation);
  if (j.contains("published")) j.at("published").get_to(d.published);
  if (j.contains("is_case_insensitive"))
    j.at("is_case_insensitive").get_to(d.is_case_insensitive);
  if (j.contains("labels")) j.at("labels").get_to(d.labels);
  if (j.contains("access")) j.at("access").get_to(d.access);
  if (j.contains("tags")) j.at("tags").get_to(d.tags);
  if (j.contains("dataset_reference"))
    j.at("dataset_reference").get_to(d.dataset_reference);
  if (j.contains("linked_dataset_source"))
    j.at("linked_dataset_source").get_to(d.linked_dataset_source);
  if (j.contains("external_dataset_reference"))
    j.at("external_dataset_reference").get_to(d.external_dataset_reference);
  if (j.contains("default_rounding_mode"))
    j.at("default_rounding_mode").get_to(d.default_rounding_mode);
  if (j.contains("storage_billing_model"))
    j.at("storage_billing_model").get_to(d.storage_billing_model);

  FromJson(d.default_table_expiration, j, "default_table_expiration");
  FromJson(d.default_partition_expiration, j, "default_partition_expiration");
  FromJson(d.creation_time, j, "creation_time");
  FromJson(d.last_modified_time, j, "last_modified_time");
  FromJson(d.max_time_travel, j, "max_time_travel");
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

std::string HiveMetastoreConnectivity::DebugString(
    absl::string_view name, TracingOptions const& options, int indent) const {
  return internal::DebugFormatter(name, options, indent)
      .StringField("access_uri_type", access_uri_type)
      .StringField("access_uri", access_uri)
      .StringField("metadata_connection", metadata_connection)
      .StringField("storage_connection", storage_connection)
      .Build();
}

std::string HiveDatabaseReference::DebugString(absl::string_view name,
                                               TracingOptions const& options,
                                               int indent) const {
  return internal::DebugFormatter(name, options, indent)
      .StringField("catalog_id", catalog_id)
      .StringField("database", database)
      .SubMessage("metadata_connectivity", metadata_connectivity)
      .Build();
}

std::string ExternalDatasetReference::DebugString(absl::string_view name,
                                                  TracingOptions const& options,
                                                  int indent) const {
  return internal::DebugFormatter(name, options, indent)
      .SubMessage("hive_database", hive_database)
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
      .SubMessage("external_dataset_reference", external_dataset_reference)
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

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigquery_v2_minimal_internal
}  // namespace cloud
}  // namespace google
