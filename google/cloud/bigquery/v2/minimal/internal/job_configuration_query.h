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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGQUERY_V2_MINIMAL_INTERNAL_JOB_CONFIGURATION_QUERY_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGQUERY_V2_MINIMAL_INTERNAL_JOB_CONFIGURATION_QUERY_H

#include "google/cloud/bigquery/v2/minimal/internal/common_v2_resources.h"
#include "google/cloud/bigquery/v2/minimal/internal/table_partition.h"
#include "google/cloud/tracing_options.h"
#include "google/cloud/version.h"
#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include <nlohmann/json.hpp>
#include <string>

namespace google {
namespace cloud {
namespace bigquery_v2_minimal_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

struct JobConfigurationQuery {
  absl::optional<std::string> query;
  absl::optional<std::string> create_disposition;
  absl::optional<std::string> write_disposition;
  absl::optional<std::string> priority;
  absl::optional<std::string> parameter_mode;
  absl::optional<bool> preserve_nulls;
  absl::optional<bool> allow_large_results;
  absl::optional<bool> use_query_cache;
  absl::optional<bool> flatten_results;
  absl::optional<bool> use_legacy_sql;
  absl::optional<bool> create_session;
  absl::optional<std::int64_t> maximum_bytes_billed;

  absl::optional<std::vector<QueryParameter>> query_parameters;
  absl::optional<std::vector<std::string>> schema_update_options;
  absl::optional<std::vector<ConnectionProperty>> connection_properties;

  absl::optional<DatasetReference> default_dataset;
  absl::optional<TableReference> destination_table;
  absl::optional<TimePartitioning> time_partitioning;
  absl::optional<RangePartitioning> range_partitioning;
  absl::optional<Clustering> clustering;
  absl::optional<EncryptionConfiguration> destination_encryption_configuration;
  absl::optional<ScriptOptions> script_options;
  absl::optional<SystemVariables> system_variables;

  std::string DebugString(absl::string_view name,
                          TracingOptions const& options = {},
                          int indent = 0) const;
};
void to_json(nlohmann::json& j, JobConfigurationQuery const& c);
void from_json(nlohmann::json const& j, JobConfigurationQuery& c);

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END

}  // namespace bigquery_v2_minimal_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGQUERY_V2_MINIMAL_INTERNAL_JOB_CONFIGURATION_QUERY_H
