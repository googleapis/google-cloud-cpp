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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGQUERY_V2_MINIMAL_INTERNAL_TABLE_CONSTRAINTS_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGQUERY_V2_MINIMAL_INTERNAL_TABLE_CONSTRAINTS_H

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

struct PrimaryKey {
  std::vector<std::string> columns;

  std::string DebugString(absl::string_view name,
                          TracingOptions const& options = {},
                          int indent = 0) const;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(PrimaryKey, columns);

struct ColumnReference {
  std::string referencing_column;
  std::string referenced_column;

  std::string DebugString(absl::string_view name,
                          TracingOptions const& options = {},
                          int indent = 0) const;
};
void to_json(nlohmann::json& j, ColumnReference const& c);
void from_json(nlohmann::json const& j, ColumnReference& c);

struct ForeignKey {
  std::string key_name;
  TableReference referenced_table;
  std::vector<ColumnReference> column_references;

  std::string DebugString(absl::string_view name,
                          TracingOptions const& options = {},
                          int indent = 0) const;
};
void to_json(nlohmann::json& j, ForeignKey const& f);
void from_json(nlohmann::json const& j, ForeignKey& f);

struct TableConstraints {
  PrimaryKey primary_key;
  std::vector<ForeignKey> foreign_keys;

  std::string DebugString(absl::string_view name,
                          TracingOptions const& options = {},
                          int indent = 0) const;
};
void to_json(nlohmann::json& j, TableConstraints const& t);
void from_json(nlohmann::json const& j, TableConstraints& t);

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigquery_v2_minimal_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGQUERY_V2_MINIMAL_INTERNAL_TABLE_CONSTRAINTS_H
