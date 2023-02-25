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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGQUERY_V2_MINIMAL_INTERNAL_COMMON_V2_RESOURCES_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGQUERY_V2_MINIMAL_INTERNAL_COMMON_V2_RESOURCES_H

#include "google/cloud/version.h"
#include <nlohmann/json.hpp>
#include <string>

namespace google {
namespace cloud {
namespace bigquery_v2_minimal_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

using json = nlohmann::json;
// Disabling clang-tidy here as the namespace is needed for using the
// NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT.
using namespace nlohmann::literals;  // NOLINT

struct ErrorProto {
  std::string reason;
  std::string location;
  std::string message;
};

struct TableReference {
  std::string project_id;
  std::string dataset_id;
  std::string table_id;
};

struct DatasetReference {
  std::string dataset_id;
  std::string project_id;
};

struct QueryParameterType;

struct QueryParameterStructType {
  std::string name;
  // Figure out how to change type field to QueryParameterType to match the
  // proto.
  std::string type;
  std::string description;
};

// Self refrential structures below is reflecting the protobuf message structure
// for bigquery v2 QueryParameterType and QueryParameterValue proto definitions.
// Disabling clang-tidy for these two types as we want to be close to the
// protobuf definition as possible.

// NOLINTBEGIN
struct QueryParameterType {
  std::string type;
  // Have to use a raw pointer here. Smart pointer causes issues with
  // from_json() and to_json() due to only-move restrictions.
  QueryParameterType* array_type = nullptr;
  std::vector<QueryParameterStructType> struct_types;
};

struct QueryParameterValue {
  std::string value;
  std::vector<QueryParameterValue> array_values;
  std::map<std::string, QueryParameterValue> struct_values;
};
// NOLINTEND

struct QueryParameter {
  std::string name;
  QueryParameterType parameter_type;
  QueryParameterValue parameter_value;
};

struct ConnectionProperty {
  std::string key;
  std::string value;
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(ErrorProto, reason, location,
                                                message);
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(TableReference, project_id,
                                                dataset_id, table_id);
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(DatasetReference, project_id,
                                                dataset_id);
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(QueryParameterStructType, name,
                                                type, description);
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(ConnectionProperty, key, value);

// NOLINTBEGIN
inline void to_json(json& j, QueryParameterType const& q) {
  if (q.array_type != nullptr) {
    j = json{{"type", q.type},
             {"array_type", *q.array_type},
             {"struct_types", q.struct_types}};
  } else {
    j = json{{"type", q.type}, {"struct_types", q.struct_types}};
  }
}

inline void from_json(json const& j, QueryParameterType& q) {
  j.at("type").get_to(q.type);
  if (q.array_type != nullptr) {
    j.at("array_type").get_to(*q.array_type);
  }
  j.at("struct_types").get_to(q.struct_types);
}

inline void to_json(json& j, QueryParameterValue const& q) {
  j = json{{"value", q.value},
           {"array_values", q.array_values},
           {"struct_values", q.struct_values}};
}

inline void from_json(json const& j, QueryParameterValue& q) {
  j.at("value").get_to(q.value);
  j.at("array_values").get_to(q.array_values);
  j.at("struct_values").get_to(q.struct_values);
}
// NOLINTEND

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(QueryParameter, name,
                                                parameter_type,
                                                parameter_value);

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigquery_v2_minimal_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGQUERY_V2_MINIMAL_INTERNAL_COMMON_V2_RESOURCES_H
