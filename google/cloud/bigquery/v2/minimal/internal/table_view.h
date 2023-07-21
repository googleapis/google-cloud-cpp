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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGQUERY_V2_MINIMAL_INTERNAL_TABLE_VIEW_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGQUERY_V2_MINIMAL_INTERNAL_TABLE_VIEW_H

#include "google/cloud/bigquery/v2/minimal/internal/common_v2_resources.h"
#include "google/cloud/bigquery/v2/minimal/internal/table_partition.h"
#include "google/cloud/tracing_options.h"
#include "google/cloud/version.h"
#include "absl/strings/string_view.h"
#include <nlohmann/json.hpp>
#include <chrono>
#include <string>

namespace google {
namespace cloud {
namespace bigquery_v2_minimal_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

// Disabling clang-tidy here as the namespace is needed for using the
// NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT.
using namespace nlohmann::literals;  // NOLINT

struct UserDefinedFunctionResource {
  std::string resource_uri;
  std::string inline_code;

  std::string DebugString(absl::string_view name,
                          TracingOptions const& options = {},
                          int indent = 0) const;
};
void to_json(nlohmann::json& j, UserDefinedFunctionResource const& u);
void from_json(nlohmann::json const& j, UserDefinedFunctionResource& u);

struct ViewDefinition {
  std::string query;

  bool use_legacy_sql = false;

  std::vector<UserDefinedFunctionResource> user_defined_function_resources;

  std::string DebugString(absl::string_view name,
                          TracingOptions const& options = {},
                          int indent = 0) const;
};
void to_json(nlohmann::json& j, ViewDefinition const& v);
void from_json(nlohmann::json const& j, ViewDefinition& v);

struct MaterializedViewDefinition {
  std::string query;
  bool enable_refresh = false;

  std::chrono::milliseconds refresh_interval_time =
      std::chrono::milliseconds(0);
  std::chrono::system_clock::time_point last_refresh_time;

  std::string DebugString(absl::string_view name,
                          TracingOptions const& options = {},
                          int indent = 0) const;
};
void to_json(nlohmann::json& j, MaterializedViewDefinition const& m);
void from_json(nlohmann::json const& j, MaterializedViewDefinition& m);

struct MaterializedViewStatus {
  std::chrono::system_clock::time_point refresh_watermark;
  ErrorProto last_refresh_status;

  std::string DebugString(absl::string_view name,
                          TracingOptions const& options = {},
                          int indent = 0) const;
};
void to_json(nlohmann::json& j, MaterializedViewStatus const& m);
void from_json(nlohmann::json const& j, MaterializedViewStatus& m);

struct TableMetadataView {
  static TableMetadataView UnSpecified();
  static TableMetadataView Basic();
  static TableMetadataView StorageStats();
  static TableMetadataView Full();

  std::string value;

  std::string DebugString(absl::string_view name,
                          TracingOptions const& options = {},
                          int indent = 0) const;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(TableMetadataView, value);

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigquery_v2_minimal_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGQUERY_V2_MINIMAL_INTERNAL_TABLE_VIEW_H
