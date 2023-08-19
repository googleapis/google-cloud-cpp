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

#include "google/cloud/bigquery/v2/minimal/internal/table_connection.h"
#include "google/cloud/bigquery/v2/minimal/internal/table_client.h"
#include "google/cloud/bigquery/v2/minimal/internal/table_rest_connection_impl.h"
#include "google/cloud/bigquery/v2/minimal/internal/table_rest_stub.h"
#include "google/cloud/bigquery/v2/minimal/testing/mock_table_rest_stub.h"
#include "google/cloud/bigquery/v2/minimal/testing/table_test_utils.h"
#include "google/cloud/common_options.h"
#include "google/cloud/internal/make_status.h"
#include "google/cloud/internal/rest_response.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace bigquery_v2_minimal_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

using ::google::cloud::bigquery_v2_minimal_testing::MockTableRestStub;
using ::google::cloud::testing_util::StatusIs;
using ::testing::AtLeast;
using ::testing::ElementsAre;
using ::testing::HasSubstr;
using ::testing::IsEmpty;
using ::testing::Not;
using ::testing::Return;

std::shared_ptr<TableConnection> CreateTestingConnection(
    std::shared_ptr<TableRestStub> mock) {
  TableLimitedErrorCountRetryPolicy retry(
      /*maximum_failures=*/2);
  ExponentialBackoffPolicy backoff(
      /*initial_delay=*/std::chrono::microseconds(1),
      /*maximum_delay=*/std::chrono::microseconds(1),
      /*scaling=*/2.0);
  auto options =
      TableDefaultOptions(Options{}
                              .set<TableRetryPolicyOption>(retry.clone())
                              .set<TableBackoffPolicyOption>(backoff.clone()));
  return std::make_shared<TableRestConnectionImpl>(std::move(mock),
                                                   std::move(options));
}

// Verify that the successful case works.
TEST(TableConnectionTest, GetTableSuccess) {
  auto mock = std::make_shared<MockTableRestStub>();

  EXPECT_CALL(*mock, GetTable)
      .WillOnce(
          [&](rest_internal::RestContext&,
              GetTableRequest const& request) -> StatusOr<GetTableResponse> {
            EXPECT_THAT(request.project_id(), Not(IsEmpty()));
            EXPECT_THAT(request.dataset_id(), Not(IsEmpty()));
            EXPECT_THAT(request.table_id(), Not(IsEmpty()));
            BigQueryHttpResponse http_response;
            http_response.payload =
                bigquery_v2_minimal_testing::MakeTableJsonText();
            ;
            return GetTableResponse::BuildFromHttpResponse(
                std::move(http_response));
          });

  auto conn = CreateTestingConnection(std::move(mock));

  GetTableRequest request = bigquery_v2_minimal_testing::MakeGetTableRequest();
  auto expected = bigquery_v2_minimal_testing::MakeTable();

  google::cloud::internal::OptionsSpan span(conn->options());
  auto table_result = conn->GetTable(request);

  ASSERT_STATUS_OK(table_result);
  bigquery_v2_minimal_testing::AssertEquals(expected, *table_result);
}

TEST(TableConnectionTest, ListTablesSuccess) {
  auto mock = std::make_shared<MockTableRestStub>();

  EXPECT_CALL(*mock, ListTables)
      .WillOnce(
          [&](rest_internal::RestContext&, ListTablesRequest const& request) {
            EXPECT_THAT(request.project_id(), Not(IsEmpty()));
            EXPECT_THAT(request.dataset_id(), Not(IsEmpty()));
            EXPECT_TRUE(request.page_token().empty());
            ListTablesResponse page;
            page.next_page_token = "page-1";
            ListFormatTable table;
            table.id = "table1";
            page.tables.push_back(table);
            return make_status_or(page);
          })
      .WillOnce(
          [&](rest_internal::RestContext&, ListTablesRequest const& request) {
            EXPECT_EQ("test-project-id", request.project_id());
            EXPECT_EQ("test-dataset-id", request.dataset_id());
            EXPECT_EQ("page-1", request.page_token());
            ListTablesResponse page;
            page.next_page_token = "page-2";
            ListFormatTable table;
            table.id = "table2";
            page.tables.push_back(table);
            return make_status_or(page);
          })
      .WillOnce(
          [&](rest_internal::RestContext&, ListTablesRequest const& request) {
            EXPECT_EQ("test-project-id", request.project_id());
            EXPECT_EQ("test-dataset-id", request.dataset_id());
            EXPECT_EQ("page-2", request.page_token());
            ListTablesResponse page;
            page.next_page_token = "";
            ListFormatTable table;
            table.id = "table3";
            page.tables.push_back(table);
            return make_status_or(page);
          });

  auto conn = CreateTestingConnection(std::move(mock));

  std::vector<std::string> actual_table_ids;
  ListTablesRequest request;
  request.set_project_id("test-project-id");
  request.set_dataset_id("test-dataset-id");

  google::cloud::internal::OptionsSpan span(conn->options());
  for (auto const& table : conn->ListTables(request)) {
    ASSERT_STATUS_OK(table);
    actual_table_ids.push_back(table->id);
  }
  EXPECT_THAT(actual_table_ids, ElementsAre("table1", "table2", "table3"));
}

// Verify that permanent errors are reported immediately.
TEST(TableConnectionTest, GetTablePermanentError) {
  auto mock = std::make_shared<MockTableRestStub>();
  EXPECT_CALL(*mock, GetTable)
      .WillOnce(
          Return(Status(StatusCode::kPermissionDenied, "permission-denied")));
  auto conn = CreateTestingConnection(std::move(mock));

  GetTableRequest request;
  google::cloud::internal::OptionsSpan span(conn->options());
  auto result = conn->GetTable(request);
  EXPECT_THAT(result, StatusIs(StatusCode::kPermissionDenied,
                               HasSubstr("permission-denied")));
}

TEST(TableConnectionTest, ListTablesPermanentError) {
  auto mock = std::make_shared<MockTableRestStub>();
  EXPECT_CALL(*mock, ListTables)
      .WillOnce(
          Return(Status(StatusCode::kPermissionDenied, "permission-denied")));
  auto conn = CreateTestingConnection(std::move(mock));

  ListTablesRequest request;
  google::cloud::internal::OptionsSpan span(conn->options());
  auto range = conn->ListTables(request);
  auto begin = range.begin();
  ASSERT_NE(begin, range.end());
  EXPECT_THAT(*begin, StatusIs(StatusCode::kPermissionDenied));
}

// Verify that too many transients errors are reported correctly.
TEST(TableConnectionTest, GetTableTooManyTransients) {
  auto mock = std::make_shared<MockTableRestStub>();
  EXPECT_CALL(*mock, GetTable)
      .Times(AtLeast(2))
      .WillRepeatedly(
          Return(Status(StatusCode::kResourceExhausted, "try-again")));
  auto conn = CreateTestingConnection(std::move(mock));

  GetTableRequest request;
  google::cloud::internal::OptionsSpan span(conn->options());
  auto result = conn->GetTable(request);
  EXPECT_THAT(result,
              StatusIs(StatusCode::kResourceExhausted, HasSubstr("try-again")));
}

TEST(TableConnectionTest, ListTablesTooManyTransients) {
  auto mock = std::make_shared<MockTableRestStub>();
  EXPECT_CALL(*mock, ListTables)
      .Times(AtLeast(2))
      .WillRepeatedly(
          Return(Status(StatusCode::kResourceExhausted, "try-again")));
  auto conn = CreateTestingConnection(std::move(mock));

  ListTablesRequest request;
  google::cloud::internal::OptionsSpan span(conn->options());
  auto range = conn->ListTables(request);
  auto begin = range.begin();
  ASSERT_NE(begin, range.end());
  EXPECT_THAT(*begin, StatusIs(StatusCode::kResourceExhausted));
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigquery_v2_minimal_internal
}  // namespace cloud
}  // namespace google
