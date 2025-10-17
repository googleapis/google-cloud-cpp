// Copyright 2025 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may
// obtain a copy of the License at
//
// https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "google/cloud/bigtable/query.h"
#include "google/cloud/bigtable/value.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <algorithm>

namespace google {
namespace cloud {
namespace bigtable {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {
using ::google::bigtable::v2::PrepareQueryResponse;

class BasicInputs {
 public:
  CompletionQueue cq;
  Project p = Project("dummy-project");
  InstanceResource instance = InstanceResource(p, "dummy-instance");
  std::string statement_contents =
      "SELECT * FROM my_table WHERE col1 = @val1 and col2 = @val2;";
  SqlStatement sql_statement = SqlStatement(statement_contents);
  PrepareQueryResponse response;
  std::unordered_map<std::string, Value> parameters = {{"val1", Value(true)},
                                                       {"val2", Value(2.0)}};
};

TEST(PreparedQuery, DefaultConstructor) {
  auto inputs = BasicInputs();
  PreparedQuery q(inputs.cq, inputs.instance, inputs.sql_statement,
                  inputs.response);
  EXPECT_EQ(inputs.instance.FullName(), q.instance().FullName());
  EXPECT_EQ(inputs.statement_contents, q.sql_statement().sql());
}

TEST(BoundQuery, FromPreparedQuery) {
  auto inputs = BasicInputs();
  auto response = PrepareQueryResponse();

  // The following variables are only meant to confirm the metadata is correctly
  // passed down to the BoundQuery.
  auto metadata = std::make_unique<google::bigtable::v2::ResultSetMetadata>();
  auto schema = std::make_unique<google::bigtable::v2::ProtoSchema>();
  auto column = google::bigtable::v2::ColumnMetadata();
  *column.mutable_name() = "col1";
  schema->mutable_columns()->Add(std::move(column));
  metadata->set_allocated_proto_schema(schema.release());
  response.set_allocated_metadata(metadata.release());

  PreparedQuery pq(inputs.cq, inputs.instance, inputs.sql_statement, response);
  auto bq = pq.BindParameters(inputs.parameters);
  EXPECT_EQ(inputs.instance.FullName(), bq.instance().FullName());
  EXPECT_EQ(inputs.statement_contents, bq.prepared_query());
  EXPECT_EQ(inputs.parameters, bq.parameters());
  EXPECT_TRUE(bq.metadata().has_proto_schema());
  EXPECT_EQ(1, bq.metadata().proto_schema().columns_size());
  EXPECT_EQ("col1", bq.metadata().proto_schema().columns()[0].name());
}

TEST(BoundQuery, ToRequestProto) {
  auto inputs = BasicInputs();
  PreparedQuery pq(inputs.cq, inputs.instance, inputs.sql_statement,
                   inputs.response);
  auto bq = pq.BindParameters(inputs.parameters);
  google::bigtable::v2::ExecuteQueryRequest proto = bq.ToRequestProto();
  EXPECT_EQ(inputs.instance.FullName(), proto.instance_name());
  EXPECT_EQ(inputs.statement_contents, proto.prepared_query());

  // Test param contents.
  EXPECT_EQ(inputs.parameters.size(), proto.mutable_params()->size());

  // The first parameter is a boolean.
  EXPECT_TRUE(proto.params().contains("val1"));
  auto val1 = proto.params().find("val1")->second;
  EXPECT_TRUE(val1.has_bool_value());
  EXPECT_EQ(true, val1.bool_value());

  // The second parameter is a double.
  EXPECT_TRUE(proto.params().contains("val2"));
  auto val2 = proto.params().find("val2")->second;
  EXPECT_TRUE(val2.has_float_value());
  EXPECT_EQ(2.0, val2.float_value());
}
}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable
}  // namespace cloud
}  // namespace google
