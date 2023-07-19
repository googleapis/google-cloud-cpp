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
      .StringField("create_disposition", createDisposition)
      .StringField("write_disposition", writeDisposition)
      .StringField("priority", priority)
      .StringField("parameter_mode", parameterMode)
      .Field("preserve_nulls", preserveNulls)
      .Field("allow_large_results", allowLargeResults)
      .Field("use_query_cache", useQueryCache)
      .Field("flatten_results", flattenResults)
      .Field("use_legacy_sql", useLegacySql)
      .Field("create_session", createSession)
      .Field("maximum_bytes_billed", maximumBytesBilled)
      .Field("schema_update_options", schemaUpdateOptions)
      .Field("connection_properties", connectionProperties)
      .Field("query_parameters", queryParameters)
      .SubMessage("default_dataset", defaultDataset)
      .SubMessage("destination_table", destinationTable)
      .SubMessage("time_partitioning", timePartitioning)
      .SubMessage("range_partitioning", rangePartitioning)
      .SubMessage("clustering", clustering)
      .SubMessage("destination_encryption_configuration",
                  destinationEncryptionConfiguration)
      .SubMessage("script_options", scriptOptions)
      .SubMessage("system_variables", systemVariables)
      .Build();
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigquery_v2_minimal_internal
}  // namespace cloud
}  // namespace google
