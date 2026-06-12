// Copyright 2025 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "google/cloud/bigtable/bound_query.h"
#include "google/cloud/bigtable/internal/query_plan.h"
#include "google/cloud/bigtable/prepared_query.h"
#include "google/cloud/bigtable/sql_statement.h"
#include "google/cloud/bigtable/value.h"
#include "google/cloud/testing_util/fake_completion_queue_impl.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <algorithm>

namespace google {
namespace cloud {
namespace bigtable {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {
using ::google::bigtable::v2::PrepareQueryResponse;

TEST(BoundQueryTest, FromPreparedQuery) {
  auto fake_cq_impl = std::make_shared<testing_util::FakeCompletionQueueImpl>();
  Project p("dummy-project");
  InstanceResource instance(p, "dummy-instance");
  std::string statement_contents(
      "SELECT * FROM my_table WHERE col1 = @val1 and col2 = @val2;");
  SqlStatement sql_statement(statement_contents);
  PrepareQueryResponse response;
  // The following variables are only meant to confirm the metadata is correctly
  // passed down to the BoundQuery.
  auto metadata = std::make_unique<google::bigtable::v2::ResultSetMetadata>();
  auto schema = std::make_unique<google::bigtable::v2::ProtoSchema>();
  auto column = google::bigtable::v2::ColumnMetadata();
  *column.mutable_name() = "col1";
  schema->mutable_columns()->Add(std::move(column));
  metadata->set_allocated_proto_schema(schema.release());
  response.set_allocated_metadata(metadata.release());
  auto refresh_fn = [&response]() {
    return make_ready_future(StatusOr<PrepareQueryResponse>(response));
  };
  auto query_plan = bigtable_internal::QueryPlan::Create(
      CompletionQueue(fake_cq_impl), std::move(response),
      std::move(refresh_fn));
  std::unordered_map<std::string, Value> parameters = {{"val1", Value(true)},
                                                       {"val2", Value(2.0)}};

  PreparedQuery pq(instance, sql_statement, query_plan);
  auto bq = pq.BindParameters(parameters);
  EXPECT_EQ(instance.FullName(), bq.instance().FullName());
  EXPECT_EQ(parameters, bq.parameters());
  EXPECT_STATUS_OK(bq.response());
  EXPECT_TRUE(bq.response()->metadata().has_proto_schema());
  EXPECT_EQ(1, bq.response()->metadata().proto_schema().columns_size());
  EXPECT_EQ("col1",
            bq.response()->metadata().proto_schema().columns()[0].name());

  // Cancel all pending operations, satisfying any remaining futures.
  fake_cq_impl->SimulateCompletion(false);
}

TEST(BoundQueryTest, ToRequestProto) {
  auto fake_cq_impl = std::make_shared<testing_util::FakeCompletionQueueImpl>();
  Project p("dummy-project");
  InstanceResource instance(p, "dummy-instance");
  std::string statement_contents(
      "SELECT * FROM my_table WHERE col1 = @val1 and col2 = @val2;");
  SqlStatement sql_statement(statement_contents);
  PrepareQueryResponse response;
  auto refresh_fn = []() {
    return make_ready_future(StatusOr<PrepareQueryResponse>{});
  };
  auto query_plan = bigtable_internal::QueryPlan::Create(
      CompletionQueue(fake_cq_impl), response, std::move(refresh_fn));
  std::unordered_map<std::string, Value> parameters = {{"val1", Value(true)},
                                                       {"val2", Value(2.0)}};

  PreparedQuery pq(instance, sql_statement, query_plan);
  auto bq = pq.BindParameters(parameters);
  google::bigtable::v2::ExecuteQueryRequest proto = bq.ToRequestProto();
  EXPECT_EQ(instance.FullName(), proto.instance_name());
  EXPECT_EQ("", proto.prepared_query());

  // Test param contents.
  EXPECT_EQ(parameters.size(), proto.mutable_params()->size());

  // The first parameter is a boolean.
  EXPECT_TRUE(proto.params().contains("val1"));
  auto val1 = proto.params().find("val1")->second;
  EXPECT_TRUE(val1.has_bool_value());
  EXPECT_EQ(true, val1.bool_value());
  EXPECT_TRUE(val1.type().has_bool_type());

  // The second parameter is a double.
  EXPECT_TRUE(proto.params().contains("val2"));
  auto val2 = proto.params().find("val2")->second;
  EXPECT_TRUE(val2.has_float_value());
  EXPECT_EQ(2.0, val2.float_value());
  EXPECT_TRUE(val2.type().has_float64_type());

  // Cancel all pending operations, satisfying any remaining futures.
  fake_cq_impl->SimulateCompletion(false);
}
}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable
}  // namespace cloud
}  // namespace google
