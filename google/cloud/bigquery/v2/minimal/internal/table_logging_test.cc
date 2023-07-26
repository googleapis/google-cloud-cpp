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

#include "google/cloud/bigquery/v2/minimal/internal/table_logging.h"
#include "google/cloud/bigquery/v2/minimal/internal/table_rest_stub.h"
#include "google/cloud/bigquery/v2/minimal/testing/mock_table_rest_stub.h"
#include "google/cloud/bigquery/v2/minimal/testing/table_test_utils.h"
#include "google/cloud/common_options.h"
#include "google/cloud/log.h"
#include "google/cloud/testing_util/scoped_log.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace bigquery_v2_minimal_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

using ::google::cloud::bigquery_v2_minimal_testing::MockTableRestStub;
using ::google::cloud::testing_util::ScopedLog;
using ::testing::Contains;
using ::testing::HasSubstr;
using ::testing::IsEmpty;
using ::testing::Not;

std::shared_ptr<TableLogging> CreateMockTableLogging(
    std::shared_ptr<TableRestStub> mock) {
  return std::make_shared<TableLogging>(std::move(mock), TracingOptions{},
                                        std::set<std::string>{});
}

TEST(TableLoggingClientTest, GetTable) {
  ScopedLog log;

  auto mock_stub = std::make_shared<MockTableRestStub>();

  EXPECT_CALL(*mock_stub, GetTable)
      .WillOnce(
          [&](rest_internal::RestContext&,
              GetTableRequest const& request) -> StatusOr<GetTableResponse> {
            EXPECT_THAT(request.project_id(), Not(IsEmpty()));
            EXPECT_THAT(request.table_id(), Not(IsEmpty()));
            BigQueryHttpResponse http_response;
            http_response.payload =
                bigquery_v2_minimal_testing::MakeTableJsonText();
            return GetTableResponse::BuildFromHttpResponse(
                std::move(http_response));
          });

  auto client = CreateMockTableLogging(std::move(mock_stub));
  GetTableRequest request = bigquery_v2_minimal_testing::MakeGetTableRequest();
  rest_internal::RestContext context;
  context.AddHeader("header-1", "value-1");
  context.AddHeader("header-2", "value-2");

  client->GetTable(context, request);

  auto actual_lines = log.ExtractLines();

  EXPECT_THAT(actual_lines, Contains(HasSubstr(R"(GetTableRequest)")));
  EXPECT_THAT(actual_lines,
              Contains(HasSubstr(R"(project_id: "t-123")")).Times(2));
  EXPECT_THAT(actual_lines,
              Contains(HasSubstr(R"(dataset_id: "t-123")")).Times(2));
  EXPECT_THAT(actual_lines,
              Contains(HasSubstr(R"(table_id: "t-123")")).Times(2));
  EXPECT_THAT(actual_lines, Contains(HasSubstr(R"(GetTableResponse)")));
  EXPECT_THAT(actual_lines, Contains(HasSubstr(R"(id: "t-id")")));
  EXPECT_THAT(actual_lines, Contains(HasSubstr(R"(kind: "t-kind")")));
  EXPECT_THAT(actual_lines, Contains(HasSubstr(R"(Context)")));
  EXPECT_THAT(actual_lines, Contains(HasSubstr(R"(name: "header-1")")));
  EXPECT_THAT(actual_lines, Contains(HasSubstr(R"(value: "value-1")")));
  EXPECT_THAT(actual_lines, Contains(HasSubstr(R"(name: "header-2")")));
  EXPECT_THAT(actual_lines, Contains(HasSubstr(R"(value: "value-2")")));
}

TEST(TableLoggingClientTest, ListTables) {
  ScopedLog log;

  auto mock_stub = std::make_shared<MockTableRestStub>();

  EXPECT_CALL(*mock_stub, ListTables)
      .WillOnce(
          [&](rest_internal::RestContext&, ListTablesRequest const& request)
              -> StatusOr<ListTablesResponse> {
            EXPECT_THAT(request.project_id(), Not(IsEmpty()));
            BigQueryHttpResponse http_response;
            http_response.payload =
                bigquery_v2_minimal_testing::MakeListTablesResponseJsonText();
            return ListTablesResponse::BuildFromHttpResponse(
                std::move(http_response));
          });

  auto client = CreateMockTableLogging(std::move(mock_stub));
  ListTablesRequest request =
      bigquery_v2_minimal_testing::MakeListTablesRequest();

  rest_internal::RestContext context;
  context.AddHeader("header-1", "value-1");
  context.AddHeader("header-2", "value-2");

  client->ListTables(context, request);

  auto actual_lines = log.ExtractLines();

  EXPECT_THAT(actual_lines, Contains(HasSubstr(R"(ListTablesRequest)")));
  EXPECT_THAT(actual_lines,
              Contains(HasSubstr(R"(project_id: "t-123")")).Times(2));
  EXPECT_THAT(actual_lines,
              Contains(HasSubstr(R"(dataset_id: "t-123")")).Times(2));
  EXPECT_THAT(actual_lines, Contains(HasSubstr(R"(max_results: 10)")));
  EXPECT_THAT(actual_lines, Contains(HasSubstr(R"(page_token: "123")")));
  EXPECT_THAT(actual_lines, Contains(HasSubstr(R"(ListTablesResponse)")));
  EXPECT_THAT(actual_lines, Contains(HasSubstr(R"(id: "t-id")")));
  EXPECT_THAT(actual_lines, Contains(HasSubstr(R"(kind: "t-kind")")));
  EXPECT_THAT(actual_lines, Contains(HasSubstr(R"(table_id: "t-123")")));
  EXPECT_THAT(actual_lines,
              Contains(HasSubstr(R"(next_page_token: "npt-123")")));
  EXPECT_THAT(actual_lines, Contains(HasSubstr(R"(Context)")));
  EXPECT_THAT(actual_lines, Contains(HasSubstr(R"(name: "header-1")")));
  EXPECT_THAT(actual_lines, Contains(HasSubstr(R"(value: "value-1")")));
  EXPECT_THAT(actual_lines, Contains(HasSubstr(R"(name: "header-2")")));
  EXPECT_THAT(actual_lines, Contains(HasSubstr(R"(value: "value-2")")));
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigquery_v2_minimal_internal
}  // namespace cloud
}  // namespace google
