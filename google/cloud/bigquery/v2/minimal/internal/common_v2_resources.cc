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

#include "google/cloud/bigquery/v2/minimal/internal/common_v2_resources.h"

namespace google {
namespace cloud {
namespace bigquery_v2_minimal_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

// NOLINTBEGIN
void to_json(nlohmann::json& j, QueryParameterType const& q) {
  if (q.array_type != nullptr) {
    j = nlohmann::json{{"type", q.type},
                       {"array_type", *q.array_type},
                       {"struct_types", q.struct_types}};
  } else {
    j = nlohmann::json{{"type", q.type}, {"struct_types", q.struct_types}};
  }
}

void from_json(nlohmann::json const& j, QueryParameterType& q) {
  if (exists(j, "type")) j.at("type").get_to(q.type);
  if (exists(j, "array_type")) {
    if (q.array_type == nullptr) {
      q.array_type = std::make_shared<QueryParameterType>();
    }
    j.at("array_type").get_to(*q.array_type);
  }
  if (exists(j, "struct_types")) j.at("struct_types").get_to(q.struct_types);
}

void to_json(nlohmann::json& j, QueryParameterValue const& q) {
  j = nlohmann::json{{"value", q.value},
                     {"array_values", q.array_values},
                     {"struct_values", q.struct_values}};
}

void from_json(nlohmann::json const& j, QueryParameterValue& q) {
  if (exists(j, "value")) j.at("value").get_to(q.value);
  if (exists(j, "array_values")) j.at("array_values").get_to(q.array_values);
  if (exists(j, "struct_values")) j.at("struct_values").get_to(q.struct_values);
}
// NOLINTEND
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigquery_v2_minimal_internal
}  // namespace cloud
}  // namespace google
