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
      .Field("continuous", continuous)
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

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigquery_v2_minimal_internal
}  // namespace cloud
}  // namespace google
