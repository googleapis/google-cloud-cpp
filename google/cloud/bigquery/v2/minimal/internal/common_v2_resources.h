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

struct ErrorProto {
  std::string reason;
  std::string location;
  std::string message;
};

struct TableReference {
  std::string project_id;
  std::string dataset_id;
  std::string table_id;

  std::string DebugString(absl::string_view name,
                          TracingOptions const& options = {},
                          int indent = 0) const;
};

struct DatasetReference {
  std::string dataset_id;
  std::string project_id;

  std::string DebugString(absl::string_view name,
                          TracingOptions const& options = {},
                          int indent = 0) const;
};

struct RoundingMode {
  static RoundingMode UnSpecified();
  static RoundingMode RoundHalfAwayFromZero();
  static RoundingMode RoundHalfEven();

  std::string value;

  std::string DebugString(absl::string_view name,
                          TracingOptions const& options = {},
                          int indent = 0) const;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(RoundingMode, value);

// Self referential and circular structures below is based on the bigquery v2
// QueryParameterType and QueryParameterValue proto definitions. Disabling
// clang-tidy warnings for these two types as we want to be close to the
// protobuf definition as possible.

// NOLINTBEGIN
struct QueryParameterType;

struct QueryParameterStructType {
  std::string name;
  std::shared_ptr<QueryParameterType> type;
  std::string description;
};

struct QueryParameterType {
  std::string type;

  std::shared_ptr<QueryParameterType> array_type;
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
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(ConnectionProperty, key, value);

void to_json(nlohmann::json& j, QueryParameterType const& q);

void from_json(nlohmann::json const& j, QueryParameterType& q);

void to_json(nlohmann::json& j, QueryParameterValue const& q);

void from_json(nlohmann::json const& j, QueryParameterValue& q);

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(QueryParameter, name,
                                                parameter_type,
                                                parameter_value);

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigquery_v2_minimal_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGQUERY_V2_MINIMAL_INTERNAL_COMMON_V2_RESOURCES_H
