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
#include "absl/types/variant.h"
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

  std::string DebugString(absl::string_view name,
                          TracingOptions const& options = {},
                          int indent = 0) const;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(ErrorProto, reason, location,
                                                message);

struct TableReference {
  std::string project_id;
  std::string dataset_id;
  std::string table_id;

  std::string DebugString(absl::string_view name,
                          TracingOptions const& options = {},
                          int indent = 0) const;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(TableReference, project_id,
                                                dataset_id, table_id);

struct DatasetReference {
  std::string dataset_id;
  std::string project_id;

  std::string DebugString(absl::string_view name,
                          TracingOptions const& options = {},
                          int indent = 0) const;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(DatasetReference, project_id,
                                                dataset_id);

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

struct ConnectionProperty {
  std::string key;
  std::string value;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(ConnectionProperty, key, value);

struct EncryptionConfiguration {
  std::string kms_key_name;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(EncryptionConfiguration,
                                                kms_key_name);

struct KeyResultStatementKind {
  static KeyResultStatementKind UnSpecified();
  static KeyResultStatementKind Last();
  static KeyResultStatementKind FirstSelect();

  std::string value;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(KeyResultStatementKind, value);

struct ScriptOptions {
  std::int64_t statement_timeout_ms;
  std::int64_t statement_byte_budget;

  KeyResultStatementKind key_result_statement;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(ScriptOptions,
                                                statement_timeout_ms,
                                                statement_byte_budget,
                                                key_result_statement);
struct TypeKind {
  static TypeKind UnSpecified();
  static TypeKind Int64();
  static TypeKind Bool();
  static TypeKind Float64();
  static TypeKind String();
  static TypeKind Bytes();
  static TypeKind Timestamp();
  static TypeKind Date();
  static TypeKind Time();
  static TypeKind DateTime();
  static TypeKind Interval();
  static TypeKind Geography();
  static TypeKind Numeric();
  static TypeKind BigNumeric();
  static TypeKind Json();
  static TypeKind Array();
  static TypeKind Struct();

  std::string value;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(TypeKind, value);

// Self referential and circular structures below is based on the bigquery v2
// QueryParameterType and QueryParameterValue proto definitions. Disabling
// clang-tidy warnings for these two types as we want to be close to the
// protobuf definition as possible.

// NOLINTBEGIN
struct StandardSqlDataType;

struct StandardSqlField {
  std::string name;
  std::shared_ptr<StandardSqlDataType> type;
};
void to_json(nlohmann::json& j, StandardSqlField const& f);
void from_json(nlohmann::json const& j, StandardSqlField& f);

struct StandardSqlStructType {
  std::vector<StandardSqlField> fields;
};
void to_json(nlohmann::json& j, StandardSqlStructType const& t);
void from_json(nlohmann::json const& j, StandardSqlStructType& t);

struct StandardSqlDataType {
  TypeKind type_kind;

  absl::variant<absl::monostate, std::shared_ptr<StandardSqlDataType>,
                StandardSqlStructType>
      sub_type;
};
void to_json(nlohmann::json& j, StandardSqlDataType const& t);
void from_json(nlohmann::json const& j, StandardSqlDataType& t);
bool operator==(StandardSqlDataType const& lhs, StandardSqlDataType const& rhs);

struct Struct;

struct Value {
  absl::variant<absl::monostate, double, std::string, bool,
                std::shared_ptr<Struct>, std::vector<Value>>
      kind;
};
void to_json(nlohmann::json& j, Value const& v);
void from_json(nlohmann::json const& j, Value& v);
bool operator==(Value const& lhs, Value const& rhs);

struct Struct {
  std::map<std::string, Value> fields;
};
void to_json(nlohmann::json& j, Struct const& s);
void from_json(nlohmann::json const& j, Struct& s);

struct SystemVariables {
  std::map<std::string, StandardSqlDataType> types;
  Struct values;
};
void to_json(nlohmann::json& j, SystemVariables const& s);
void from_json(nlohmann::json const& j, SystemVariables& s);

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

void to_json(nlohmann::json& j, QueryParameterType const& q);
void from_json(nlohmann::json const& j, QueryParameterType& q);

struct QueryParameterValue {
  std::string value;
  std::vector<QueryParameterValue> array_values;
  std::map<std::string, QueryParameterValue> struct_values;
};

void to_json(nlohmann::json& j, QueryParameterValue const& q);
void from_json(nlohmann::json const& j, QueryParameterValue& q);

struct QueryParameter {
  std::string name;
  QueryParameterType parameter_type;
  QueryParameterValue parameter_value;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(QueryParameter, name,
                                                parameter_type,
                                                parameter_value);
// NOLINTEND

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigquery_v2_minimal_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGQUERY_V2_MINIMAL_INTERNAL_COMMON_V2_RESOURCES_H
