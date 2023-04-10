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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGQUERY_V2_MINIMAL_INTERNAL_JOB_CONFIGURATION_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGQUERY_V2_MINIMAL_INTERNAL_JOB_CONFIGURATION_H

#include "google/cloud/bigquery/v2/minimal/internal/common_v2_resources.h"
#include "google/cloud/tracing_options.h"
#include "google/cloud/version.h"
#include "absl/strings/string_view.h"
#include <nlohmann/json.hpp>
#include <string>

namespace google {
namespace cloud {
namespace bigquery_v2_minimal_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

// Disabling clang-tidy here as the namespace is needed for using the
// NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT.
using namespace nlohmann::literals;  // NOLINT

struct JobConfigurationQuery {
  std::string query;
  std::string create_disposition;
  std::string write_disposition;
  std::string priority;
  std::string parameter_mode;
  bool preserve_nulls = false;
  bool allow_large_results = false;
  bool use_query_cache = false;
  bool flatten_results = false;
  bool use_legacy_sql = false;
  bool create_session = false;
  std::int64_t maximum_bytes_billed = 0;

  DatasetReference default_dataset;
  TableReference destination_table;
  std::vector<QueryParameter> query_parameters;
  std::vector<std::string> schema_update_options;
  std::vector<ConnectionProperty> connection_properties;
};

struct JobConfiguration {
  std::string job_type;
  bool dry_run = false;
  std::int64_t job_timeout_ms = 0;
  std::map<std::string, std::string> labels;

  JobConfigurationQuery query_config;

  std::string DebugString(absl::string_view name,
                          TracingOptions const& options = {},
                          int indent = 0) const;
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(
    JobConfigurationQuery, query, create_disposition, write_disposition,
    priority, parameter_mode, preserve_nulls, allow_large_results,
    use_query_cache, flatten_results, use_legacy_sql, create_session,
    maximum_bytes_billed, default_dataset, destination_table, query_parameters,
    schema_update_options, connection_properties);
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(JobConfiguration, job_type,
                                                query_config, dry_run,
                                                job_timeout_ms, labels);

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END

}  // namespace bigquery_v2_minimal_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGQUERY_V2_MINIMAL_INTERNAL_JOB_CONFIGURATION_H
