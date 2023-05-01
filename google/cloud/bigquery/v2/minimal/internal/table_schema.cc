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

#include "google/cloud/bigquery/v2/minimal/internal/table_schema.h"
#include "google/cloud/internal/format_time_point.h"

namespace google {
namespace cloud {
namespace bigquery_v2_minimal_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

// NOLINTBEGIN
void to_json(nlohmann::json& j,
             std::vector<std::shared_ptr<TableFieldSchema>> const& t) {
  std::vector<TableFieldSchema> fields;
  for (auto const& pt : t) {
    if (pt != nullptr) {
      fields.push_back(*pt);
    }
  }
  j = nlohmann::json{{"fields", fields}};
}

void from_json(nlohmann::json const& j,
               std::vector<std::shared_ptr<TableFieldSchema>>& t) {
  if (j.contains("fields")) {
    std::vector<TableFieldSchema> fields;
    j.at("fields").get_to(fields);
    for (auto const& f : fields) {
      auto fpt = std::make_shared<TableFieldSchema>(f);
      t.push_back(fpt);
    }
  }
}

void to_json(nlohmann::json& j, TableFieldSchema const& t) {
  j = nlohmann::json{{"name", t.name},
                     {"type", t.type},
                     {"mode", t.mode},
                     {"description", t.description},
                     {"collation", t.collation},
                     {"default_value_expression", t.default_value_expression},
                     {"max_length", t.max_length},
                     {"precision", t.precision},
                     {"scale", t.scale},
                     {"is_measure", t.is_measure},
                     {"fields", t.fields},
                     {"categories", t.categories},
                     {"policy_tags", t.policy_tags},
                     {"data_classification_tags", t.data_classification_tags},
                     {"rounding_mode", t.rounding_mode},
                     {"range_element_type", t.range_element_type}};
}

void from_json(nlohmann::json const& j, TableFieldSchema& t) {
  if (j.contains("name")) j.at("name").get_to(t.name);
  if (j.contains("type")) j.at("type").get_to(t.type);
  if (j.contains("mode")) j.at("mode").get_to(t.mode);
  if (j.contains("description")) j.at("description").get_to(t.description);
  if (j.contains("collation")) j.at("collation").get_to(t.collation);
  if (j.contains("default_value_expression"))
    j.at("default_value_expression").get_to(t.default_value_expression);
  if (j.contains("max_length")) j.at("max_length").get_to(t.max_length);
  if (j.contains("precision")) j.at("precision").get_to(t.precision);
  if (j.contains("scale")) j.at("scale").get_to(t.scale);
  if (j.contains("is_measure")) j.at("is_measure").get_to(t.is_measure);
  if (j.contains("fields")) j.at("fields").get_to(t.fields);
  if (j.contains("categories")) j.at("categories").get_to(t.categories);
  if (j.contains("policy_tags")) j.at("policy_tags").get_to(t.policy_tags);
  if (j.contains("data_classification_tags"))
    j.at("data_classification_tags").get_to(t.data_classification_tags);
  if (j.contains("rounding_mode"))
    j.at("rounding_mode").get_to(t.rounding_mode);
  if (j.contains("range_element_type"))
    j.at("range_element_type").get_to(t.range_element_type);
}

void to_json(nlohmann::json& j, TableSchema const& t) {
  j = nlohmann::json{{"fields", t.fields}};
}

void from_json(nlohmann::json const& j, TableSchema& t) {
  if (j.contains("fields")) j.at("fields").get_to(t.fields);
}

// NOLINTEND

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigquery_v2_minimal_internal
}  // namespace cloud
}  // namespace google
