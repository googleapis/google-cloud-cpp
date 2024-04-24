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
  auto f = internal::DebugFormatter(name, options, indent);
  if (query) f.StringField("query", *query);
  if (create_disposition) f.StringField("create_disposition", *create_disposition);
  if (write_disposition) f.StringField("write_disposition", *write_disposition);
  if (priority) f.StringField("priority", *priority);
  if (parameter_mode) f.StringField("parameter_mode", *parameter_mode);
  if (preserve_nulls) f.Field("preserve_nulls", *preserve_nulls);
  if (allow_large_results) f.Field("allow_large_results", *allow_large_results);
  if (use_query_cache) f.Field("use_query_cache", *use_query_cache);
  if (flatten_results) f.Field("flatten_results", *flatten_results);
  if (use_legacy_sql) f.Field("use_legacy_sql", *use_legacy_sql);
  if (create_session) f.Field("create_session", *create_session);
  if (maximum_bytes_billed) f.Field("maximum_bytes_billed", *maximum_bytes_billed);
  if (schema_update_options) f.Field("schema_update_options", *schema_update_options);
  if (connection_properties) f.Field("connection_properties", *connection_properties);
  if (query_parameters) f.Field("query_parameters", *query_parameters);
  if (default_dataset) f.SubMessage("default_dataset", *default_dataset);
  if (destination_table) f.SubMessage("destination_table", *destination_table);
  if (time_partitioning) f.SubMessage("time_partitioning", *time_partitioning);
  if (range_partitioning) f.SubMessage("range_partitioning", *range_partitioning);
  if (clustering) f.SubMessage("clustering", *clustering);
  if (destination_encryption_configuration) f.SubMessage("destination_encryption_configuration",
                                                        *destination_encryption_configuration);
  if (script_options) f.SubMessage("script_options", *script_options);
  if (system_variables) f.SubMessage("system_variables", *system_variables);
  return f.Build();
}

void to_json(nlohmann::json& j, JobConfigurationQuery const& c) {
  if (c.query) j["query"] = *c.query;
  if (c.create_disposition) j["createDisposition"] = *c.create_disposition;
  if (c.write_disposition) j["writeDisposition"] = *c.write_disposition;
  if (c.priority) j["priority"] = *c.priority;
  if (c.parameter_mode) j["parameterMode"] = *c.parameter_mode;
  if (c.preserve_nulls) j["preserveNulls"] = *c.preserve_nulls;
  if (c.allow_large_results) j["allowLargeResults"] = *c.allow_large_results;
  if (c.use_query_cache) j["useQueryCache"] = *c.use_query_cache;
  if (c.flatten_results) j["flattenResults"] = *c.flatten_results;
  if (c.use_legacy_sql) j["useLegacySql"] = *c.use_legacy_sql;
  if (c.create_session) j["createSession"] = *c.create_session;
  if (c.maximum_bytes_billed) j["maximumBytesBilled"] = std::to_string(*c.maximum_bytes_billed);
  if (c.query_parameters) j["queryParameters"] = *c.query_parameters;
  if (c.schema_update_options) j["schemaUpdateOptions"] = *c.schema_update_options;
  if (c.connection_properties) j["connectionProperties"] = *c.connection_properties;
  if (c.default_dataset) j["defaultDataset"] = *c.default_dataset;
  if (c.destination_table) j["destinationTable"] = *c.destination_table;
  if (c.time_partitioning) j["timePartitioning"] = *c.time_partitioning;
  if (c.range_partitioning) j["rangePartitioning"] = *c.range_partitioning;
  if (c.clustering) j["clustering"] = *c.clustering;
  if (c.destination_encryption_configuration) j["destinationEncryptionConfiguration"] = *c.destination_encryption_configuration;
  if (c.script_options) j["scriptOptions"] = *c.script_options;
  if (c.system_variables) j["systemVariables"] = *c.system_variables;
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
