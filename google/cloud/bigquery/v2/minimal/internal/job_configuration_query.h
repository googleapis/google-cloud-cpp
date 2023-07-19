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
  std::string createDisposition;
  std::string writeDisposition;
  std::string priority;
  std::string parameterMode;
  bool preserveNulls = false;
  bool allowLargeResults = false;
  bool useQueryCache = false;
  bool flattenResults = false;
  bool useLegacySql = false;
  bool createSession = false;
  std::int64_t maximumBytesBilled = 0;

  std::vector<QueryParameter> queryParameters;
  std::vector<std::string> schemaUpdateOptions;
  std::vector<ConnectionProperty> connectionProperties;

  DatasetReference defaultDataset;
  TableReference destinationTable;
  TimePartitioning timePartitioning;
  RangePartitioning rangePartitioning;
  Clustering clustering;
  EncryptionConfiguration destinationEncryptionConfiguration;
  ScriptOptions scriptOptions;
  SystemVariables systemVariables;

  std::string DebugString(absl::string_view name,
                          TracingOptions const& options = {},
                          int indent = 0) const;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(
    JobConfigurationQuery, query, createDisposition, writeDisposition, priority,
    parameterMode, preserveNulls, allowLargeResults, useQueryCache,
    flattenResults, useLegacySql, createSession, maximumBytesBilled,
    queryParameters, schemaUpdateOptions, connectionProperties, defaultDataset,
    destinationTable, timePartitioning, rangePartitioning, clustering,
    destinationEncryptionConfiguration, scriptOptions, systemVariables);

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END

}  // namespace bigquery_v2_minimal_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGQUERY_V2_MINIMAL_INTERNAL_JOB_CONFIGURATION_QUERY_H
