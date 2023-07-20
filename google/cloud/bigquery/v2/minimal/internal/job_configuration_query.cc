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

#include "google/cloud/bigquery/v2/minimal/internal/job_configuration_query.h"
#include "google/cloud/internal/absl_str_join_quiet.h"
#include "google/cloud/internal/debug_string.h"

namespace google {
namespace cloud {
namespace bigquery_v2_minimal_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

std::string JobConfigurationQuery::DebugString(absl::string_view name,
                                               TracingOptions const& options,
                                               int indent) const {
  return internal::DebugFormatter(name, options, indent)
      .StringField("query", query)
      .StringField("create_disposition", create_disposition)
      .StringField("write_disposition", write_disposition)
      .StringField("priority", priority)
      .StringField("parameter_mode", parameter_mode)
      .Field("preserve_nulls", preserve_nulls)
      .Field("allow_large_results", allow_large_results)
      .Field("use_query_cache", use_query_cache)
      .Field("flatten_results", flatten_results)
      .Field("use_legacy_sql", use_legacy_sql)
      .Field("create_session", create_session)
      .Field("maximum_bytes_billed", maximum_bytes_billed)
      .Field("schema_update_options", schema_update_options)
      .Field("connection_properties", connection_properties)
      .Field("query_parameters", query_parameters)
      .SubMessage("default_dataset", default_dataset)
      .SubMessage("destination_table", destination_table)
      .SubMessage("time_partitioning", time_partitioning)
      .SubMessage("range_partitioning", range_partitioning)
      .SubMessage("clustering", clustering)
      .SubMessage("destination_encryption_configuration",
                  destination_encryption_configuration)
      .SubMessage("script_options", script_options)
      .SubMessage("system_variables", system_variables)
      .Build();
}

void to_json(nlohmann::json& j, JobConfigurationQuery const& c) {
  j = nlohmann::json{{"query", c.query},
                     {"createDisposition", c.create_disposition},
                     {"writeDisposition", c.write_disposition},
                     {"priority", c.priority},
                     {"parameterMode", c.parameter_mode},
                     {"preserveNulls", c.preserve_nulls},
                     {"allowLargeResults", c.allow_large_results},
                     {"useQueryCache", c.use_query_cache},
                     {"flattenResults", c.flatten_results},
                     {"useLegacySql", c.use_legacy_sql},
                     {"createSession", c.create_session},
                     {"maximumBytesBilled", c.maximum_bytes_billed},
                     {"queryParameters", c.query_parameters},
                     {"schemaUpdateOptions", c.schema_update_options},
                     {"connectionProperties", c.connection_properties},
                     {"defaultDataset", c.default_dataset},
                     {"destinationTable", c.destination_table},
                     {"timePartitioning", c.time_partitioning},
                     {"rangePartitioning", c.range_partitioning},
                     {"clustering", c.clustering},
                     {"destinationEncryptionConfiguration",
                      c.destination_encryption_configuration},
                     {"scriptOptions", c.script_options},
                     {"systemVariables", c.system_variables}};
}
void from_json(nlohmann::json const& j, JobConfigurationQuery& c) {
  if (j.contains("query")) j.at("query").get_to(c.query);
  if (j.contains("createDisposition")) {
    j.at("createDisposition").get_to(c.create_disposition);
  }
  if (j.contains("writeDisposition")) {
    j.at("writeDisposition").get_to(c.write_disposition);
  }
  if (j.contains("priority")) j.at("priority").get_to(c.priority);
  if (j.contains("parameterMode")) {
    j.at("parameterMode").get_to(c.parameter_mode);
  }
  if (j.contains("preserveNulls")) {
    j.at("preserveNulls").get_to(c.preserve_nulls);
  }
  if (j.contains("allowLargeResults")) {
    j.at("allowLargeResults").get_to(c.allow_large_results);
  }
  if (j.contains("useQueryCache")) {
    j.at("useQueryCache").get_to(c.use_query_cache);
  }
  if (j.contains("flattenResults")) {
    j.at("flattenResults").get_to(c.flatten_results);
  }
  if (j.contains("useLegacySql")) {
    j.at("useLegacySql").get_to(c.use_legacy_sql);
  }
  if (j.contains("createSession")) {
    j.at("createSession").get_to(c.create_session);
  }
  if (j.contains("maximumBytesBilled")) {
    j.at("maximumBytesBilled").get_to(c.maximum_bytes_billed);
  }
  if (j.contains("queryParameters")) {
    j.at("queryParameters").get_to(c.query_parameters);
  }
  if (j.contains("schemaUpdateOptions")) {
    j.at("schemaUpdateOptions").get_to(c.schema_update_options);
  }
  if (j.contains("connectionProperties")) {
    j.at("connectionProperties").get_to(c.connection_properties);
  }
  if (j.contains("defaultDataset")) {
    j.at("defaultDataset").get_to(c.default_dataset);
  }
  if (j.contains("destinationTable")) {
    j.at("destinationTable").get_to(c.destination_table);
  }
  if (j.contains("timePartitioning")) {
    j.at("timePartitioning").get_to(c.time_partitioning);
  }
  if (j.contains("rangePartitioning")) {
    j.at("rangePartitioning").get_to(c.range_partitioning);
  }
  if (j.contains("clustering")) j.at("clustering").get_to(c.clustering);
  if (j.contains("destinationEncryptionConfiguration")) {
    j.at("destinationEncryptionConfiguration")
        .get_to(c.destination_encryption_configuration);
  }
  if (j.contains("scriptOptions")) {
    j.at("scriptOptions").get_to(c.script_options);
  }
  if (j.contains("systemVariables")) {
    j.at("systemVariables").get_to(c.system_variables);
  }
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigquery_v2_minimal_internal
}  // namespace cloud
}  // namespace google
