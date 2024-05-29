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

#include "google/cloud/bigquery/v2/minimal/testing/common_v2_test_utils.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace bigquery_v2_minimal_testing {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

using ::google::cloud::bigquery_v2_minimal_internal::ColumnData;
using ::google::cloud::bigquery_v2_minimal_internal::ConnectionProperty;
using ::google::cloud::bigquery_v2_minimal_internal::DatasetReference;
using ::google::cloud::bigquery_v2_minimal_internal::QueryParameter;
using ::google::cloud::bigquery_v2_minimal_internal::QueryParameterStructType;
using ::google::cloud::bigquery_v2_minimal_internal::QueryParameterType;
using ::google::cloud::bigquery_v2_minimal_internal::QueryParameterValue;
using ::google::cloud::bigquery_v2_minimal_internal::RowData;
using ::google::cloud::bigquery_v2_minimal_internal::StandardSqlDataType;
using ::google::cloud::bigquery_v2_minimal_internal::StandardSqlField;
using ::google::cloud::bigquery_v2_minimal_internal::StandardSqlStructType;
using ::google::cloud::bigquery_v2_minimal_internal::SystemVariables;
using ::google::cloud::bigquery_v2_minimal_internal::TypeKind;
using ::google::cloud::bigquery_v2_minimal_internal::Value;

using ::testing::IsEmpty;
using ::testing::Not;

DatasetReference MakeDatasetReference() {
  DatasetReference d;
  d.dataset_id = "1";
  d.project_id = "2";

  return d;
}

ConnectionProperty MakeConnectionProperty() {
  ConnectionProperty cp;

  cp.key = "conn-prop-key";
  cp.value = "conn-prop-val";

  return cp;
}

QueryParameterType MakeQueryParameterType() {
  QueryParameterType expected_qp_type;
  QueryParameterStructType array_st;
  QueryParameterStructType qp_st;

  array_st.name = "array-struct-name";
  array_st.type = std::make_shared<QueryParameterType>();
  array_st.type->type = "array-struct-type";

  array_st.description = "array-struct-description";
  qp_st.name = "qp-struct-name";
  qp_st.type = std::make_shared<QueryParameterType>();
  qp_st.type->type = "qp-struct-type";
  qp_st.description = "qp-struct-description";

  expected_qp_type.type = "query-parameter-type";
  expected_qp_type.array_type = std::make_shared<QueryParameterType>();
  expected_qp_type.array_type->type = "array-type";
  expected_qp_type.array_type->struct_types.push_back(array_st);
  expected_qp_type.struct_types.push_back(qp_st);

  return expected_qp_type;
}

QueryParameterValue MakeQueryParameterValue() {
  QueryParameterValue expected_qp_val;
  QueryParameterValue qp_struct_val;
  QueryParameterValue array_val;
  QueryParameterValue nested_array_val;
  QueryParameterValue array_struct_val;

  expected_qp_val.value = "query-parameter-value";
  qp_struct_val.value = "qp-map-value";

  nested_array_val.value = "array-val-2";
  array_struct_val.value = "array-map-value";
  nested_array_val.struct_values.insert({"array-map-key", array_struct_val});

  array_val.value = "array-val-1";
  array_val.array_values.push_back(nested_array_val);

  expected_qp_val.array_values.push_back(array_val);
  expected_qp_val.struct_values.insert({"qp-map-key", qp_struct_val});

  return expected_qp_val;
}

QueryParameter MakeQueryParameter() {
  QueryParameter expected;
  expected.name = "query-parameter-name";
  expected.parameter_type = MakeQueryParameterType();
  expected.parameter_value = MakeQueryParameterValue();

  return expected;
}

SystemVariables MakeSystemVariables() {
  SystemVariables expected;

  StandardSqlField sql_field1;
  sql_field1.name = "f1-sql-struct-type-int64";

  StandardSqlField sql_field2;
  sql_field2.name = "f2-sql-struct-type-string";

  StandardSqlStructType sql_struct_type1;
  sql_struct_type1.fields.push_back(sql_field1);

  StandardSqlStructType sql_struct_type2;
  sql_struct_type2.fields.push_back(sql_field2);

  StandardSqlDataType sql_data_type1;
  StandardSqlDataType sql_data_type2;
  StandardSqlDataType sql_data_type3;

  sql_data_type1.type_kind = TypeKind::Int64();
  sql_data_type1.sub_type = sql_struct_type1;

  sql_data_type2.type_kind = TypeKind::String();
  sql_data_type2.sub_type = sql_struct_type2;

  sql_data_type3.type_kind = TypeKind::String();
  sql_data_type3.sub_type =
      std::make_shared<StandardSqlDataType>(sql_data_type2);

  Value val1;
  val1.value_kind = 3.4;

  Value val2;
  val2.value_kind = true;

  Value val3;
  val3.value_kind = std::string("val3");

  expected.types.insert({"sql-struct-type-key-1", sql_data_type1});
  expected.types.insert({"sql-struct-type-key-2", sql_data_type2});
  expected.types.insert({"sql-struct-type-key-3", sql_data_type3});

  expected.values.fields.insert({"double-key", val1});
  expected.values.fields.insert({"bool-key", val2});
  expected.values.fields.insert({"string-key", val3});

  return expected;
}

RowData MakeRowData() {
  RowData result;
  result.columns.push_back(ColumnData{"col1"});
  result.columns.push_back(ColumnData{"col2"});
  result.columns.push_back(ColumnData{"col3"});
  result.columns.push_back(ColumnData{"col4"});
  result.columns.push_back(ColumnData{"col5"});
  result.columns.push_back(ColumnData{"col6"});

  return result;
}

void AssertParamValueEquals(QueryParameterValue& expected,
                            QueryParameterValue& actual) {
  EXPECT_EQ(expected.value, actual.value);

  ASSERT_THAT(expected.array_values, Not(IsEmpty()));
  ASSERT_THAT(actual.array_values, Not(IsEmpty()));
  EXPECT_EQ(expected.array_values.size(), actual.array_values.size());
  EXPECT_EQ(expected.array_values[0].value, actual.array_values[0].value);

  ASSERT_THAT(expected.array_values[0].array_values, Not(IsEmpty()));
  ASSERT_THAT(actual.array_values[0].array_values, Not(IsEmpty()));
  EXPECT_EQ(expected.array_values[0].array_values.size(),
            actual.array_values[0].array_values.size());
  EXPECT_EQ(expected.array_values[0].array_values[0].value,
            actual.array_values[0].array_values[0].value);

  EXPECT_EQ(expected.array_values[0].struct_values["array-map-key"].value,
            actual.array_values[0].struct_values["array-map-key"].value);
  EXPECT_EQ(expected.struct_values["qp-map-key"].value,
            actual.struct_values["qp-map-key"].value);
}

void AssertParamTypeEquals(QueryParameterType& expected,
                           QueryParameterType& actual) {
  EXPECT_EQ(expected.type, actual.type);

  EXPECT_EQ(expected.array_type->type, actual.array_type->type);

  ASSERT_THAT(expected.array_type->struct_types, Not(IsEmpty()));
  ASSERT_THAT(actual.array_type->struct_types, Not(IsEmpty()));
  EXPECT_EQ(expected.array_type->struct_types.size(),
            actual.array_type->struct_types.size());
  EXPECT_EQ(expected.array_type->struct_types[0].name,
            actual.array_type->struct_types[0].name);
  EXPECT_EQ(expected.array_type->struct_types[0].type->type,
            actual.array_type->struct_types[0].type->type);
  EXPECT_EQ(expected.array_type->struct_types[0].description,
            actual.array_type->struct_types[0].description);

  ASSERT_THAT(expected.struct_types, Not(IsEmpty()));
  ASSERT_THAT(actual.struct_types, Not(IsEmpty()));
  EXPECT_EQ(expected.struct_types.size(), actual.struct_types.size());
  EXPECT_EQ(expected.struct_types[0].name, actual.struct_types[0].name);
  EXPECT_EQ(expected.struct_types[0].type->type,
            actual.struct_types[0].type->type);
  EXPECT_EQ(expected.struct_types[0].description,
            actual.struct_types[0].description);
}

void AssertEquals(SystemVariables& expected, SystemVariables& actual) {
  EXPECT_EQ(expected.types.size(), 3);
  EXPECT_EQ(expected.types.size(), actual.types.size());
  EXPECT_EQ(expected.types, actual.types);

  EXPECT_EQ(expected.values.fields.size(), 3);
  EXPECT_EQ(expected.values.fields.size(), actual.values.fields.size());
  EXPECT_EQ(expected.values.fields, actual.values.fields);
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigquery_v2_minimal_testing
}  // namespace cloud
}  // namespace google
