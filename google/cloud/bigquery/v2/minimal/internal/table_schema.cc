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
#include "google/cloud/internal/debug_string.h"
#include "google/cloud/internal/format_time_point.h"

namespace google {
namespace cloud {
namespace bigquery_v2_minimal_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

std::string CategoryList::DebugString(absl::string_view name,
                                      TracingOptions const& options,
                                      int indent) const {
  return internal::DebugFormatter(name, options, indent)
      .Field("names", names)
      .Build();
}

std::string PolicyTagList::DebugString(absl::string_view name,
                                       TracingOptions const& options,
                                       int indent) const {
  return internal::DebugFormatter(name, options, indent)
      .Field("names", names)
      .Build();
}

std::string FieldElementType::DebugString(absl::string_view name,
                                          TracingOptions const& options,
                                          int indent) const {
  return internal::DebugFormatter(name, options, indent)
      .StringField("type", type)
      .Build();
}

std::string TableFieldSchema::DebugString(absl::string_view fname,
                                          TracingOptions const& options,
                                          int indent) const {
  return internal::DebugFormatter(fname, options, indent)
      .StringField("name", name)
      .StringField("type", type)
      .StringField("mode", mode)
      .StringField("description", description)
      .StringField("collation", collation)
      .StringField("default_value_expression", default_value_expression)
      .Field("max_length", max_length)
      .Field("precision", precision)
      .Field("scale", scale)
      .SubMessage("categories", categories)
      .SubMessage("policy_tags", policy_tags)
      .SubMessage("rounding_mode", rounding_mode)
      .SubMessage("range_element_type", range_element_type)
      .Build();
}

std::string TableSchema::DebugString(absl::string_view name,
                                     TracingOptions const& options,
                                     int indent) const {
  return internal::DebugFormatter(name, options, indent)
      .Field("fields", fields)
      .Build();
}

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
      t.push_back(std::make_shared<TableFieldSchema>(f));
    }
  }
}

void to_json(nlohmann::json& j, TableFieldSchema const& t) {
  j = nlohmann::json{{"name", t.name},
                     {"type", t.type},
                     {"mode", t.mode},
                     {"description", t.description},
                     {"collation", t.collation},
                     {"defaultValueExpression", t.default_value_expression},
                     {"maxLength", t.max_length},
                     {"precision", t.precision},
                     {"scale", t.scale},
                     {"fields", t.fields},
                     {"categories", t.categories},
                     {"policyTags", t.policy_tags},
                     {"roundingMode", t.rounding_mode},
                     {"rangeElementType", t.range_element_type}};
}

void from_json(nlohmann::json const& j, TableFieldSchema& t) {
  if (j.contains("name")) j.at("name").get_to(t.name);
  if (j.contains("type")) j.at("type").get_to(t.type);
  if (j.contains("mode")) j.at("mode").get_to(t.mode);
  if (j.contains("description")) j.at("description").get_to(t.description);
  if (j.contains("collation")) j.at("collation").get_to(t.collation);
  if (j.contains("defaultValueExpression"))
    j.at("defaultValueExpression").get_to(t.default_value_expression);
  if (j.contains("maxLength")) j.at("maxLength").get_to(t.max_length);
  if (j.contains("precision")) j.at("precision").get_to(t.precision);
  if (j.contains("scale")) j.at("scale").get_to(t.scale);
  if (j.contains("fields")) j.at("fields").get_to(t.fields);
  if (j.contains("categories")) j.at("categories").get_to(t.categories);
  if (j.contains("policyTags")) j.at("policyTags").get_to(t.policy_tags);
  if (j.contains("roundingMode")) j.at("roundingMode").get_to(t.rounding_mode);
  if (j.contains("rangeElementType"))
    j.at("rangeElementType").get_to(t.range_element_type);
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
