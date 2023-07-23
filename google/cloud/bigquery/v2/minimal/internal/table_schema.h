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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGQUERY_V2_MINIMAL_INTERNAL_TABLE_SCHEMA_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGQUERY_V2_MINIMAL_INTERNAL_TABLE_SCHEMA_H

#include "google/cloud/bigquery/v2/minimal/internal/common_v2_resources.h"
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

struct CategoryList {
  std::vector<std::string> names;

  std::string DebugString(absl::string_view name,
                          TracingOptions const& options = {},
                          int indent = 0) const;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(CategoryList, names);

struct PolicyTagList {
  std::vector<std::string> names;

  std::string DebugString(absl::string_view name,
                          TracingOptions const& options = {},
                          int indent = 0) const;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(PolicyTagList, names);

struct FieldElementType {
  std::string type;

  std::string DebugString(absl::string_view name,
                          TracingOptions const& options = {},
                          int indent = 0) const;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(FieldElementType, type);

// NOLINTBEGIN
struct TableFieldSchema {
  std::string name;
  std::string type;
  std::string mode;
  std::string description;
  std::string collation;
  std::string default_value_expression;

  std::int64_t max_length = 0;
  std::int64_t precision = 0;
  std::int64_t scale = 0;

  std::vector<std::shared_ptr<TableFieldSchema>> fields;

  CategoryList categories;
  PolicyTagList policy_tags;
  RoundingMode rounding_mode;
  FieldElementType range_element_type;

  std::string DebugString(absl::string_view fname,
                          TracingOptions const& options = {},
                          int indent = 0) const;
};
void to_json(nlohmann::json& j, TableFieldSchema const& t);
void from_json(nlohmann::json const& j, TableFieldSchema& t);

void to_json(nlohmann::json& j,
             std::vector<std::shared_ptr<TableFieldSchema>> const& t);
void from_json(nlohmann::json const& j,
               std::vector<std::shared_ptr<TableFieldSchema>>& t);

struct TableSchema {
  std::vector<TableFieldSchema> fields;

  std::string DebugString(absl::string_view name,
                          TracingOptions const& options = {},
                          int indent = 0) const;
};
void to_json(nlohmann::json& j, TableSchema const& t);
void from_json(nlohmann::json const& j, TableSchema& t);
// NOLINTEND

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigquery_v2_minimal_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGQUERY_V2_MINIMAL_INTERNAL_TABLE_SCHEMA_H
