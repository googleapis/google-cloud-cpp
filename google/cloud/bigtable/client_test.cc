// Copyright 2025 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "google/cloud/bigtable/client.h"
#include "google/cloud/bigtable/internal/query_plan.h"
#include "google/cloud/bigtable/mocks/mock_data_connection.h"
#include "google/cloud/bigtable/mocks/mock_query_row.h"
#include "google/cloud/bigtable/mocks/mock_row_reader.h"
#include "google/cloud/bigtable/query_row.h"
#include "google/cloud/stream_range.h"
#include "google/cloud/testing_util/fake_completion_queue_impl.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <google/protobuf/text_format.h>

namespace google {
namespace cloud {
namespace bigtable {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::google::bigtable::v2::PrepareQueryResponse;
using ::google::cloud::testing_util::FakeCompletionQueueImpl;
using ::google::cloud::testing_util::IsOkAndHolds;
using ::google::cloud::testing_util::StatusIs;
using ::testing::Return;

TEST(Client, PrepareQuery) {
  auto fake_cq_impl = std::make_shared<testing_util::FakeCompletionQueueImpl>();
  auto conn_mock = std::make_shared<bigtable_mocks::MockDataConnection>();
  InstanceResource instance(Project("the-project"), "the-instance");
  SqlStatement sql("SELECT * FROM the-table");
  EXPECT_CALL(*conn_mock, PrepareQuery)
      .WillOnce([&](PrepareQueryParams const& params) {
        EXPECT_EQ("projects/the-project/instances/the-instance",
                  params.instance.FullName());
        EXPECT_EQ("SELECT * FROM the-table", params.sql_statement.sql());

        std::unordered_map<std::string, bigtable::Value> parameters;
        google::bigtable::v2::PrepareQueryResponse pq_response;
        auto refresh_fn = [&pq_response]() {
          return make_ready_future(
              StatusOr<google::bigtable::v2::PrepareQueryResponse>(
                  pq_response));
        };
        auto query_plan = bigtable_internal::QueryPlan::Create(
            CompletionQueue(fake_cq_impl), std::move(pq_response),
            std::move(refresh_fn));
        return bigtable::PreparedQuery(instance, sql, std::move(query_plan));
      });

  Client client(std::move(conn_mock));
  auto prepared_query = client.PrepareQuery(instance, sql);
  ASSERT_STATUS_OK(prepared_query);

  // Cancel all pending operations, satisfying any remaining futures.
  fake_cq_impl->SimulateCompletion(false);
}

TEST(Client, AsyncPrepareQuery) {
  auto fake_cq_impl = std::make_shared<testing_util::FakeCompletionQueueImpl>();
  auto conn_mock = std::make_shared<bigtable_mocks::MockDataConnection>();
  InstanceResource instance(Project("the-project"), "the-instance");
  SqlStatement sql("SELECT * FROM the-table");
  EXPECT_CALL(*conn_mock, AsyncPrepareQuery)
      .WillOnce([&](PrepareQueryParams const& params) {
        EXPECT_EQ("projects/the-project/instances/the-instance",
                  params.instance.FullName());
        EXPECT_EQ("SELECT * FROM the-table", params.sql_statement.sql());

        std::unordered_map<std::string, bigtable::Value> parameters;
        google::bigtable::v2::PrepareQueryResponse pq_response;
        auto refresh_fn = [&pq_response]() {
          return make_ready_future(
              StatusOr<google::bigtable::v2::PrepareQueryResponse>(
                  pq_response));
        };
        auto query_plan = bigtable_internal::QueryPlan::Create(
            CompletionQueue(fake_cq_impl), std::move(pq_response),
            std::move(refresh_fn));
        StatusOr<bigtable::PreparedQuery> result =
            bigtable::PreparedQuery(instance, sql, std::move(query_plan));
        return make_ready_future(result);
      });
  Client client(std::move(conn_mock));
  auto prepared_query = client.AsyncPrepareQuery(instance, sql);
  ASSERT_STATUS_OK(prepared_query.get());

  // Cancel all pending operations, satisfying any remaining futures.
  fake_cq_impl->SimulateCompletion(false);
}

class MockQueryRowSource : public ResultSourceInterface {
 public:
  MOCK_METHOD(StatusOr<QueryRow>, NextRow, (), (override));
  MOCK_METHOD(absl::optional<google::bigtable::v2::ResultSetMetadata>, Metadata,
              (), (override));
};

TEST(ClientTest, ExecuteQuery) {
  auto conn_mock = std::make_shared<bigtable_mocks::MockDataConnection>();
  auto fake_cq_impl = std::make_shared<FakeCompletionQueueImpl>();
  auto refresh_fn = []() {
    return make_ready_future(
        StatusOr<google::bigtable::v2::PrepareQueryResponse>(
            Status{StatusCode::kUnimplemented, "not implemented"}));
  };
  PrepareQueryResponse pq_response;
  pq_response.set_prepared_query("test-pq-id-54321");
  auto constexpr kResultMetadataText = R"pb(
    proto_schema {
      columns {
        name: "key"
        type { string_type {} }
      }
      columns {
        name: "val"
        type { string_type {} }
      }
    }
  )pb";
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(
      kResultMetadataText, pq_response.mutable_metadata()));
  EXPECT_CALL(*conn_mock, ExecuteQuery)
      .WillOnce(
          [&](bigtable::ExecuteQueryParams const&) -> StatusOr<RowStream> {
            auto mock_source = std::make_unique<MockQueryRowSource>();
            EXPECT_CALL(*mock_source, Metadata)
                .WillRepeatedly(Return(pq_response.metadata()));

            testing::InSequence s;
            EXPECT_CALL(*mock_source, NextRow)
                .WillOnce(Return(bigtable_mocks::MakeQueryRow(
                    {{"key", bigtable::Value("r1")},
                     {"val", bigtable::Value("v1")}})));
            EXPECT_CALL(*mock_source, NextRow)
                .WillOnce(Return(bigtable_mocks::MakeQueryRow(
                    {{"key", bigtable::Value("r2")},
                     {"val", bigtable::Value("v2")}})));
            EXPECT_CALL(*mock_source, NextRow)
                // Signal end of stream
                .WillOnce(
                    Return(Status(StatusCode::kOutOfRange, "End of stream")));

            // Create RowStream with the mock result source
            RowStream row_stream(std::move(mock_source));
            return StatusOr<RowStream>(std::move(row_stream));
          });

  Client client(conn_mock);
  InstanceResource instance(Project("test-project"), "test-instance");
  SqlStatement sql("SELECT * FROM `test-table`");
  auto query_plan = bigtable_internal::QueryPlan::Create(
      CompletionQueue(fake_cq_impl), std::move(pq_response),
      std::move(refresh_fn));
  auto prepared_query = PreparedQuery(instance, sql, std::move(query_plan));
  auto bound_query = prepared_query.BindParameters({});
  RowStream row_stream = client.ExecuteQuery(std::move(bound_query));
  std::vector<StatusOr<bigtable::QueryRow>> rows;
  for (auto const& row : std::move(row_stream)) {
    rows.push_back(row);
  }

  ASSERT_EQ(rows.size(), 3);
  ASSERT_STATUS_OK(rows[0]);
  auto const& row1 = *rows[0];
  EXPECT_EQ(row1.columns().size(), 2);
  EXPECT_THAT(row1.values()[0].get<std::string>(), IsOkAndHolds("r1"));
  EXPECT_THAT(row1.values()[1].get<std::string>(), IsOkAndHolds("v1"));

  ASSERT_STATUS_OK(rows[1]);
  auto const& row2 = *rows[1];
  EXPECT_EQ(row2.columns().size(), 2);
  EXPECT_THAT(row2.values()[0].get<std::string>(), IsOkAndHolds("r2"));
  EXPECT_THAT(row2.values()[1].get<std::string>(), IsOkAndHolds("v2"));

  EXPECT_THAT(rows[2], StatusIs(StatusCode::kOutOfRange, "End of stream"));

  // Cancel all pending operations, satisfying any remaining futures.
  fake_cq_impl->SimulateCompletion(false);
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable
}  // namespace cloud
}  // namespace google
