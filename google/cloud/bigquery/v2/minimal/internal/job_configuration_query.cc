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
#include "google/cloud/bigquery/v2/minimal/internal/json_utils.h"
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
  j = nlohmann::json{
      {"query", c.query},
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
      {"maximumBytesBilled", std::to_string(c.maximum_bytes_billed)},
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
  SafeGetTo(c.query, j, "query");
  SafeGetTo(c.create_disposition, j, "createDisposition");
  SafeGetTo(c.write_disposition, j, "writeDisposition");
  SafeGetTo(c.priority, j, "priority");
  SafeGetTo(c.parameter_mode, j, "parameterMode");
  SafeGetTo(c.preserve_nulls, j, "preserveNulls");
  SafeGetTo(c.allow_large_results, j, "allowLargeResults");
  SafeGetTo(c.use_query_cache, j, "useQueryCache");
  SafeGetTo(c.flatten_results, j, "flattenResults");
  SafeGetTo(c.use_legacy_sql, j, "useLegacySql");
  SafeGetTo(c.create_session, j, "createSession");
  c.maximum_bytes_billed = GetNumberFromJson(j, "maximumBytesBilled");
  SafeGetTo(c.query_parameters, j, "queryParameters");
  SafeGetTo(c.schema_update_options, j, "schemaUpdateOptions");
  SafeGetTo(c.connection_properties, j, "connectionProperties");
  SafeGetTo(c.default_dataset, j, "defaultDataset");
  SafeGetTo(c.destination_table, j, "destinationTable");
  SafeGetTo(c.time_partitioning, j, "timePartitioning");
  SafeGetTo(c.range_partitioning, j, "rangePartitioning");
  SafeGetTo(c.clustering, j, "clustering");
  SafeGetTo(c.destination_encryption_configuration, j,
            "destinationEncryptionConfiguration");
  SafeGetTo(c.script_options, j, "scriptOptions");
  SafeGetTo(c.system_variables, j, "systemVariables");
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigquery_v2_minimal_internal
}  // namespace cloud
}  // namespace google
