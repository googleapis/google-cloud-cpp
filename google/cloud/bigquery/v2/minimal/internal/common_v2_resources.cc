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
#include "google/cloud/internal/debug_string.h"
#include "google/cloud/internal/format_time_point.h"

namespace google {
namespace cloud {
namespace bigquery_v2_minimal_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

// Bypass clang-tidy warnings for self-referential and recursive structures.

// NOLINTBEGIN
struct QueryParameterType;

void to_json(nlohmann::json& j, QueryParameterStructType const& q) {
  if (q.type != nullptr) {
    j = nlohmann::json{
        {"name", q.name}, {"type", *q.type}, {"description", q.description}};
  } else {
    j = nlohmann::json{{"name", q.name}, {"description", q.description}};
  }
}

void from_json(nlohmann::json const& j, QueryParameterStructType& q) {
  if (j.contains("name")) j.at("name").get_to(q.name);
  if (j.contains("type")) {
    if (q.type == nullptr) {
      q.type = std::make_shared<QueryParameterType>();
    }
    j.at("type").get_to(*q.type);
  }
  if (j.contains("description")) j.at("description").get_to(q.description);
}

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
  if (j.contains("type")) j.at("type").get_to(q.type);
  if (j.contains("array_type")) {
    if (q.array_type == nullptr) {
      q.array_type = std::make_shared<QueryParameterType>();
    }
    j.at("array_type").get_to(*q.array_type);
  }
  if (j.contains("struct_types")) j.at("struct_types").get_to(q.struct_types);
}

void to_json(nlohmann::json& j, QueryParameterValue const& q) {
  j = nlohmann::json{{"value", q.value},
                     {"array_values", q.array_values},
                     {"struct_values", q.struct_values}};
}

void from_json(nlohmann::json const& j, QueryParameterValue& q) {
  if (j.contains("value")) j.at("value").get_to(q.value);
  if (j.contains("array_values")) j.at("array_values").get_to(q.array_values);
  if (j.contains("struct_values"))
    j.at("struct_values").get_to(q.struct_values);
}

void to_json(nlohmann::json& j, StandardSqlField const& f) {
  if (f.type != nullptr) {
    j = nlohmann::json{{"name", f.name}, {"type", *f.type}};
  } else {
    j = nlohmann::json{{"name", f.name}};
  }
}

void from_json(nlohmann::json const& j, StandardSqlField& f) {
  if (j.contains("name")) j.at("name").get_to(f.name);
  if (f.type == nullptr) {
    f.type = std::make_shared<StandardSqlDataType>();
  }
  if (j.contains("type")) j.at("type").get_to(*f.type);
}

void to_json(nlohmann::json& j, StandardSqlStructType const& t) {
  j = nlohmann::json{{"fields", t.fields}};
}

void from_json(nlohmann::json const& j, StandardSqlStructType& t) {
  if (j.contains("fields")) j.at("fields").get_to(t.fields);
}

void to_json(nlohmann::json& j, StandardSqlDataType const& t) {
  if (t.sub_type.valueless_by_exception()) {
    j = nlohmann::json{{"type_kind", t.type_kind}};
    return;
  }

  struct Visitor {
    TypeKind type_kind;
    nlohmann::json& j;

    void operator()(absl::monostate const&) {
      j = nlohmann::json{{"type_kind", std::move(type_kind)}};
    }
    void operator()(std::shared_ptr<StandardSqlDataType> const& type) {
      j = nlohmann::json{{"type_kind", std::move(type_kind)},
                         {"sub_type", *type},
                         {"sub_type_index", 1}};
    }
    void operator()(StandardSqlStructType const& type) {
      j = nlohmann::json{{"type_kind", std::move(type_kind)},
                         {"sub_type", type},
                         {"sub_type_index", 2}};
    }
  };

  absl::visit(Visitor{t.type_kind, j}, t.sub_type);
}

void from_json(nlohmann::json const& j, StandardSqlDataType& t) {
  if (j.contains("type_kind")) j.at("type_kind").get_to(t.type_kind);
  if (j.contains("sub_type_index") && j.contains("sub_type")) {
    auto const index = j.at("sub_type_index").get<int>();
    switch (index) {
      case 1:
        t.sub_type = std::make_shared<StandardSqlDataType>(
            j.at("sub_type").get<StandardSqlDataType>());
        break;
      case 2:
        t.sub_type = j.at("sub_type").get<StandardSqlStructType>();
        break;
      default:
        break;
    }
  }
}

void to_json(nlohmann::json& j, Value const& v) {
  if (v.value_kind.valueless_by_exception()) {
    return;
  }

  struct Visitor {
    nlohmann::json& j;

    void operator()(absl::monostate const&) {
      // Nothing to do.
    }
    void operator()(double const& val) {
      j = nlohmann::json{{"value_kind", val}, {"kind_index", 1}};
    }
    void operator()(std::string const& val) {
      j = nlohmann::json{{"value_kind", val}, {"kind_index", 2}};
    }
    void operator()(bool const& val) {
      j = nlohmann::json{{"value_kind", val}, {"kind_index", 3}};
    }
    void operator()(std::shared_ptr<Struct> const& val) {
      j = nlohmann::json{{"value_kind", *val}, {"kind_index", 4}};
    }
    void operator()(std::vector<Value> const& val) {
      j = nlohmann::json{{"value_kind", val}, {"kind_index", 5}};
    }
  };

  absl::visit(Visitor{j}, v.value_kind);
}

void from_json(nlohmann::json const& j, Value& v) {
  if (j.contains("kind_index") && j.contains("value_kind")) {
    auto const index = j.at("kind_index").get<int>();
    switch (index) {
      case 0:
        // Do not set any value
        break;
      case 1:
        v.value_kind = j.at("value_kind").get<double>();
        break;
      case 2:
        v.value_kind = j.at("value_kind").get<std::string>();
        break;
      case 3:
        v.value_kind = j.at("value_kind").get<bool>();
        break;
      case 4:
        v.value_kind =
            std::make_shared<Struct>(j.at("value_kind").get<Struct>());
        break;
      case 5:
        v.value_kind = j.at("value_kind").get<std::vector<Value>>();
        break;
      default:
        break;
    }
  }
}

void to_json(nlohmann::json& j, Struct const& s) {
  j = nlohmann::json{{"fields", s.fields}};
}

void from_json(nlohmann::json const& j, Struct& s) {
  if (j.contains("fields")) j.at("fields").get_to(s.fields);
}

void to_json(nlohmann::json& j, SystemVariables const& s) {
  j = nlohmann::json{{"types", s.types}, {"values", s.values}};
}

void from_json(nlohmann::json const& j, SystemVariables& s) {
  if (j.contains("types")) j.at("types").get_to(s.types);
  if (j.contains("values")) j.at("values").get_to(s.values);
}

bool operator==(StandardSqlStructType const& lhs,
                StandardSqlStructType const& rhs) {
  return std::equal(lhs.fields.begin(), lhs.fields.end(), rhs.fields.begin());
}

bool operator==(StandardSqlField const& lhs, StandardSqlField const& rhs) {
  auto eq_name = (lhs.name == rhs.name);
  auto eq_type = ((lhs.type == nullptr) && (rhs.type == nullptr));
  if (eq_type) return eq_name;

  if (lhs.type != nullptr && rhs.type != nullptr) {
    return (*lhs.type == *rhs.type);
  }

  return false;
}

bool operator==(StandardSqlDataType const& lhs,
                StandardSqlDataType const& rhs) {
  return (lhs.type_kind.value == rhs.type_kind.value);
}

bool operator==(Value const& lhs, Value const& rhs) {
  return (lhs.value_kind == rhs.value_kind);
}
// NOLINTEND

RoundingMode RoundingMode::UnSpecified() {
  return RoundingMode{"ROUNDING_MODE_UNSPECIFIED"};
}

RoundingMode RoundingMode::RoundHalfAwayFromZero() {
  return RoundingMode{"ROUND_HALF_AWAY_FROM_ZERO"};
}

RoundingMode RoundingMode::RoundHalfEven() {
  return RoundingMode{"ROUND_HALF_EVEN"};
}

KeyResultStatementKind KeyResultStatementKind::UnSpecified() {
  return KeyResultStatementKind{"KEY_RESULT_STATEMENT_KIND_UNSPECIFIED"};
}

KeyResultStatementKind KeyResultStatementKind::Last() {
  return KeyResultStatementKind{"LAST"};
}

KeyResultStatementKind KeyResultStatementKind::FirstSelect() {
  return KeyResultStatementKind{"FIRST_SELECT"};
}

TypeKind TypeKind::UnSpecified() { return TypeKind{"TYPE_KIND_UNSPECIFIED"}; }

TypeKind TypeKind::Int64() { return TypeKind{"INT64"}; }

TypeKind TypeKind::Bool() { return TypeKind{"BOOL"}; }

TypeKind TypeKind::Float64() { return TypeKind{"FLOAT64"}; }

TypeKind TypeKind::String() { return TypeKind{"STRING"}; }

TypeKind TypeKind::Bytes() { return TypeKind{"BYTES"}; }

TypeKind TypeKind::Timestamp() { return TypeKind{"TIMESTAMP"}; }

TypeKind TypeKind::Date() { return TypeKind{"DATE"}; }

TypeKind TypeKind::Time() { return TypeKind{"TIME"}; }

TypeKind TypeKind::DateTime() { return TypeKind{"DATETIME"}; }

TypeKind TypeKind::Interval() { return TypeKind{"INTERVAL"}; }

TypeKind TypeKind::Geography() { return TypeKind{"GEOGRAPHY"}; }

TypeKind TypeKind::Numeric() { return TypeKind{"NUMERIC"}; }

TypeKind TypeKind::BigNumeric() { return TypeKind{"BIGNUMERIC"}; }

TypeKind TypeKind::Json() { return TypeKind{"JSON"}; }

TypeKind TypeKind::Array() { return TypeKind{"ARRAY"}; }

TypeKind TypeKind::Struct() { return TypeKind{"STRUCT"}; }

std::string ErrorProto::DebugString(absl::string_view name,
                                    TracingOptions const& options,
                                    int indent) const {
  return internal::DebugFormatter(name, options, indent)
      .StringField("reason", reason)
      .StringField("location", location)
      .StringField("message", message)
      .Build();
}

std::string RoundingMode::DebugString(absl::string_view name,
                                      TracingOptions const& options,
                                      int indent) const {
  return internal::DebugFormatter(name, options, indent)
      .StringField("value", value)
      .Build();
}

std::string DatasetReference::DebugString(absl::string_view name,
                                          TracingOptions const& options,
                                          int indent) const {
  return internal::DebugFormatter(name, options, indent)
      .StringField("project_id", project_id)
      .StringField("dataset_id", dataset_id)
      .Build();
}

std::string TableReference::DebugString(absl::string_view name,
                                        TracingOptions const& options,
                                        int indent) const {
  return internal::DebugFormatter(name, options, indent)
      .StringField("project_id", project_id)
      .StringField("dataset_id", dataset_id)
      .StringField("table_id", table_id)
      .Build();
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigquery_v2_minimal_internal
}  // namespace cloud
}  // namespace google
