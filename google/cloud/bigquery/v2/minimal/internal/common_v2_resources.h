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
#include <chrono>
#include <string>

namespace google {
namespace cloud {
namespace bigquery_v2_minimal_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

// Disabling clang-tidy here as the namespace is needed for using the
// NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT.
using namespace nlohmann::literals;  // NOLINT

// NOLINTBEGIN(misc-no-recursion)
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
bool operator==(ErrorProto const& lhs, ErrorProto const& rhs);

struct TableReference {
  std::string project_id;
  std::string dataset_id;
  std::string table_id;

  std::string DebugString(absl::string_view name,
                          TracingOptions const& options = {},
                          int indent = 0) const;
};
void to_json(nlohmann::json& j, TableReference const& t);
void from_json(nlohmann::json const& j, TableReference& t);
bool operator==(TableReference const& lhs, TableReference const& rhs);

struct DatasetReference {
  std::string dataset_id;
  std::string project_id;

  std::string DebugString(absl::string_view name,
                          TracingOptions const& options = {},
                          int indent = 0) const;
};
void to_json(nlohmann::json& j, DatasetReference const& d);
void from_json(nlohmann::json const& j, DatasetReference& d);
bool operator==(DatasetReference const& lhs, DatasetReference const& rhs);

struct RoutineReference {
  std::string project_id;
  std::string dataset_id;
  std::string routine_id;

  std::string DebugString(absl::string_view name,
                          TracingOptions const& options = {},
                          int indent = 0) const;
};
void to_json(nlohmann::json& j, RoutineReference const& r);
void from_json(nlohmann::json const& j, RoutineReference& r);
bool operator==(RoutineReference const& lhs, RoutineReference const& rhs);

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

// Customizes QUERY behavior.
// For ODBC, corresponds to properties in a connection string.
//
// For more details, see:
// https://cloud.google.com/bigquery/docs/reference/rest/v2/ConnectionProperty
struct ConnectionProperty {
  std::string key;
  std::string value;

  std::string DebugString(absl::string_view name,
                          TracingOptions const& options = {},
                          int indent = 0) const;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(ConnectionProperty, key, value);
bool operator==(ConnectionProperty const& lhs, ConnectionProperty const& rhs);

// Describes the encryption key used to protect the BigQuery destination table.
//
// For more details, see:
// https://cloud.google.com/bigquery/docs/reference/rest/v2/EncryptionConfiguration
struct EncryptionConfiguration {
  std::string kms_key_name;

  std::string DebugString(absl::string_view name,
                          TracingOptions const& options = {},
                          int indent = 0) const;
};
void to_json(nlohmann::json& j, EncryptionConfiguration const& ec);
void from_json(nlohmann::json const& j, EncryptionConfiguration& ec);

// Used in ScriptOptions to control the execution of script.
// Determines which statement in the script represents the "key result",
// used to populate the schema and query results of the script job.
//
// For more details, please see:
// https://cloud.google.com/bigquery/docs/reference/rest/v2/Job#KeyResultStatementKind
struct KeyResultStatementKind {
  static KeyResultStatementKind UnSpecified();
  static KeyResultStatementKind Last();
  static KeyResultStatementKind FirstSelect();

  std::string DebugString(absl::string_view name,
                          TracingOptions const& options = {},
                          int indent = 0) const;

  std::string value;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(KeyResultStatementKind, value);

// Controls the execution of a script job using timeout, bytes billed
// and result statements.
//
// For more details, please see:
// https://cloud.google.com/bigquery/docs/reference/rest/v2/Job#ScriptOptions
struct ScriptOptions {
  std::chrono::milliseconds statement_timeout = std::chrono::milliseconds(0);
  std::int64_t statement_byte_budget = 0;

  KeyResultStatementKind key_result_statement;

  std::string DebugString(absl::string_view name,
                          TracingOptions const& options = {},
                          int indent = 0) const;
};
void to_json(nlohmann::json& j, ScriptOptions const& s);
void from_json(nlohmann::json const& j, ScriptOptions& s);

// Represents a GoogleSQL Data Type.
//
// This is used to define a top level type or a sub type
// for a sql field. The latter is applicable if the top level field is an
// ARRAY or STRUCT.
//
// For more details, please see
// https://cloud.google.com/bigquery/docs/reference/rest/v2/StandardSqlDataType#typekind
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

  std::string DebugString(absl::string_view name,
                          TracingOptions const& options = {},
                          int indent = 0) const;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(TypeKind, value);

// Self referential and circular structures below is based on the bigquery v2
// QueryParameterType and QueryParameterValue proto definitions. Disabling
// clang-tidy warnings for these two types as we want to be close to the
// protobuf definition as possible.

// Represents the data type of a variable.
// Please see the full definition for more details.
struct StandardSqlDataType;

// Represents a Google SQL field or column.
// Used to define field members for a STRUCT type fields.
// For more details see
// https://cloud.google.com/bigquery/docs/reference/rest/v2/StandardSqlField
struct StandardSqlField {
  std::string name;
  std::shared_ptr<StandardSqlDataType> type;

  std::string DebugString(absl::string_view field_name,
                          TracingOptions const& options = {},
                          int indent = 0) const;
};
void to_json(nlohmann::json& j, StandardSqlField const& f);
void from_json(nlohmann::json const& j, StandardSqlField& f);
bool operator==(StandardSqlField const& lhs, StandardSqlField const& rhs);

// Represents a STRUCT type field.
// Used to define struct members for a top level TypeKind of STRUCT
// in a StandardSqlDataType definition.
//
// For more details, see:
// https://cloud.google.com/bigquery/docs/reference/rest/v2/StandardSqlDataType
struct StandardSqlStructType {
  std::vector<StandardSqlField> fields;

  std::string DebugString(absl::string_view name,
                          TracingOptions const& options = {},
                          int indent = 0) const;
};
void to_json(nlohmann::json& j, StandardSqlStructType const& t);
void from_json(nlohmann::json const& j, StandardSqlStructType& t);
bool operator==(StandardSqlStructType const& lhs,
                StandardSqlStructType const& rhs);

// Represents the data type of a variable such as function argument.
//
// TypeKind defines the top level type for the field and can be any
// GoogleSQL Data type.
//
// An additional SubType is applicable if the top level type is either a
// STRUCT or an ARRAY. This is a recursive self-referential field which
// defines the sub types for an array or record elements.
//
// For more details, see:
// https://cloud.google.com/bigquery/docs/reference/rest/v2/StandardSqlDataType
struct StandardSqlDataType {
  using ValueType =
      absl::variant<absl::monostate, std::shared_ptr<StandardSqlDataType>,
                    StandardSqlStructType>;
  TypeKind type_kind;

  ValueType sub_type;

  std::string DebugString(absl::string_view name,
                          TracingOptions const& options = {},
                          int indent = 0) const;
};
void to_json(nlohmann::json& j, StandardSqlDataType const& t);
void from_json(nlohmann::json const& j, StandardSqlDataType& t);
bool operator==(StandardSqlDataType const& lhs, StandardSqlDataType const& rhs);

// Represents a structured data value, consisting of fields
// which map to dynamically typed values.
//
// For more details see:
// https://protobuf.dev/reference/protobuf/google.protobuf/#struct
//
// Please see the full definition for more details on the field members.
struct Struct;

// Value represents a dynamically typed value which can be either null,
// a number, a string, a boolean, a recursive struct value,
// or a list of values. A producer of value is expected to set
// one of the variants, and absence of any variant indicates an error.
//
// For more details, please see:
// https://protobuf.dev/reference/protobuf/google.protobuf/#value
struct Value {
  using KindType = absl::variant<absl::monostate, double, std::string, bool,
                                 std::shared_ptr<Struct>, std::vector<Value>>;
  KindType value_kind;

  std::string DebugString(absl::string_view name,
                          TracingOptions const& options = {},
                          int indent = 0) const;
};
void to_json(nlohmann::json& j, Value const& v);
void from_json(nlohmann::json const& j, Value& v);
bool operator==(Value const& lhs, Value const& rhs);

// Represents a structured data value, consisting of fields
// which map to dynamically typed values.
//
// For more details see:
// https://protobuf.dev/reference/protobuf/google.protobuf/#struct
struct Struct {
  std::map<std::string, Value> fields;

  std::string DebugString(absl::string_view name,
                          TracingOptions const& options = {},
                          int indent = 0) const;
};
void to_json(nlohmann::json& j, Struct const& s);
void from_json(nlohmann::json const& j, Struct& s);
bool operator==(Struct const& lhs, Struct const& rhs);

// Represents the system variables that can be given to a query job.
// System variables can be used to check information during query execution.
//
// For more details on SystemVariable structure definition, please see:
// https://cloud.google.com/bigquery/docs/reference/rest/v2/Job#systemvariables
//
// For system variables supported by bigquery , please see:
// https://cloud.google.com/bigquery/docs/reference/system-variables
struct SystemVariables {
  // Represents the data type for each system variable.
  std::map<std::string, StandardSqlDataType> types;
  //  Value for each system variable.
  Struct values;

  std::string DebugString(absl::string_view name,
                          TracingOptions const& options = {},
                          int indent = 0) const;
};
void to_json(nlohmann::json& j, SystemVariables const& s);
void from_json(nlohmann::json const& j, SystemVariables& s);

struct QueryParameterType;

struct QueryParameterStructType {
  std::string name;
  std::shared_ptr<QueryParameterType> type;
  std::string description;

  std::string DebugString(absl::string_view qp_name,
                          TracingOptions const& options = {},
                          int indent = 0) const;
};

struct QueryParameterType {
  std::string type;

  std::shared_ptr<QueryParameterType> array_type;
  std::vector<QueryParameterStructType> struct_types;

  std::string DebugString(absl::string_view name,
                          TracingOptions const& options = {},
                          int indent = 0) const;
};
void to_json(nlohmann::json& j, QueryParameterType const& q);
void from_json(nlohmann::json const& j, QueryParameterType& q);

struct QueryParameterValue {
  std::string value;
  std::vector<QueryParameterValue> array_values;
  std::map<std::string, QueryParameterValue> struct_values;

  std::string DebugString(absl::string_view name,
                          TracingOptions const& options = {},
                          int indent = 0) const;
};
void to_json(nlohmann::json& j, QueryParameterValue const& q);
void from_json(nlohmann::json const& j, QueryParameterValue& q);

struct QueryParameter {
  std::string name;
  QueryParameterType parameter_type;
  QueryParameterValue parameter_value;

  std::string DebugString(absl::string_view name,
                          TracingOptions const& options = {},
                          int indent = 0) const;
};
void to_json(nlohmann::json& j, QueryParameter const& q);
void from_json(nlohmann::json const& j, QueryParameter& q);
bool operator==(QueryParameter const& lhs, QueryParameter const& rhs);
// NOLINTEND(misc-no-recursion)

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigquery_v2_minimal_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGQUERY_V2_MINIMAL_INTERNAL_COMMON_V2_RESOURCES_H
