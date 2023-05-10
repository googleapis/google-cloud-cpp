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
#include "google/cloud/bigquery/v2/minimal/testing/mock_log_backend.h"
#include "google/cloud/bigquery/v2/minimal/testing/mock_table_rest_stub.h"
#include "google/cloud/bigquery/v2/minimal/testing/table_test_utils.h"
#include "google/cloud/common_options.h"
#include "google/cloud/log.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace bigquery_v2_minimal_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

using ::google::cloud::bigquery_v2_minimal_testing::MockTableRestStub;
using ::testing::HasSubstr;
using ::testing::IsEmpty;

std::shared_ptr<TableLogging> CreateMockTableLogging(
    std::shared_ptr<TableRestStub> mock) {
  return std::make_shared<TableLogging>(std::move(mock), TracingOptions{},
                                        std::set<std::string>{});
}

class TableLoggingClientTest : public ::testing::Test {
 protected:
  void SetUp() override {
    log_backend_ =
        std::make_shared<bigquery_v2_minimal_testing::MockLogBackend>();
    log_backend_id_ =
        google::cloud::LogSink::Instance().AddBackend(log_backend_);
  }
  void TearDown() override {
    google::cloud::LogSink::Instance().RemoveBackend(log_backend_id_);
    log_backend_id_ = 0;
    log_backend_.reset();
  }

  std::shared_ptr<bigquery_v2_minimal_testing::MockLogBackend> log_backend_ =
      nullptr;
  long log_backend_id_ = 0;  // NOLINT(google-runtime-int)
};

TEST_F(TableLoggingClientTest, GetTable) {
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

  // Not checking all fields exhaustively since this has already been tested.
  // Just ensuring here that the request and response are being logged.
  EXPECT_CALL(*log_backend_, ProcessWithOwnership)
      .WillOnce([](LogRecord const& lr) {
        EXPECT_THAT(lr.message, HasSubstr(" << "));
        EXPECT_THAT(lr.message, HasSubstr(R"(GetTableRequest)"));
        EXPECT_THAT(lr.message, HasSubstr(R"(project_id: "t-123")"));
        EXPECT_THAT(lr.message, HasSubstr(R"(dataset_id: "t-123")"));
        EXPECT_THAT(lr.message, HasSubstr(R"(table_id: "t-123")"));
      })
      .WillOnce([](LogRecord const& lr) {
        EXPECT_THAT(lr.message, HasSubstr(R"(GetTableResponse)"));
        EXPECT_THAT(lr.message, HasSubstr(R"(project_id: "t-123")"));
        EXPECT_THAT(lr.message, HasSubstr(R"(dataset_id: "t-123")"));
        EXPECT_THAT(lr.message, HasSubstr(R"(table_id: "t-123")"));
      });

  auto client = CreateMockTableLogging(std::move(mock_stub));
  GetTableRequest request = bigquery_v2_minimal_testing::MakeGetTableRequest();
  rest_internal::RestContext context;

  client->GetTable(context, request);
}

TEST_F(TableLoggingClientTest, ListTables) {
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

  EXPECT_CALL(*log_backend_, ProcessWithOwnership)
      .WillOnce([](LogRecord const& lr) {
        EXPECT_THAT(lr.message, HasSubstr(" << "));
        EXPECT_THAT(lr.message, HasSubstr(R"(ListTablesRequest)"));
        EXPECT_THAT(lr.message, HasSubstr(R"(project_id: "t-123")"));
        EXPECT_THAT(lr.message, HasSubstr(R"(dataset_id: "t-123")"));
        EXPECT_THAT(lr.message, HasSubstr(R"(max_results: 10)"));
        EXPECT_THAT(lr.message, HasSubstr(R"(page_token: "123")"));
      })
      .WillOnce([](LogRecord const& lr) {
        EXPECT_THAT(lr.message, HasSubstr(R"(ListTablesResponse)"));
        EXPECT_THAT(lr.message, HasSubstr(R"(project_id: "t-123")"));
        EXPECT_THAT(lr.message, HasSubstr(R"(dataset_id: "t-123")"));
        EXPECT_THAT(lr.message, HasSubstr(R"(table_id: "t-123")"));
        EXPECT_THAT(lr.message, HasSubstr(R"(next_page_token: "npt-123")"));
      });

  auto client = CreateMockTableLogging(std::move(mock_stub));
  ListTablesRequest request =
      bigquery_v2_minimal_testing::MakeListTablesRequest();
  rest_internal::RestContext context;

  client->ListTables(context, request);
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigquery_v2_minimal_internal
}  // namespace cloud
}  // namespace google
