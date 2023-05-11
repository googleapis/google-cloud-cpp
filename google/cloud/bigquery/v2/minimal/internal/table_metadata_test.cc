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

#include "google/cloud/bigquery/v2/minimal/internal/table_metadata.h"
#include "google/cloud/bigquery/v2/minimal/internal/table_rest_stub.h"
#include "google/cloud/bigquery/v2/minimal/testing/mock_table_rest_stub.h"
#include "google/cloud/bigquery/v2/minimal/testing/table_test_utils.h"
#include "google/cloud/common_options.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>
#include <chrono>

namespace google {
namespace cloud {
namespace bigquery_v2_minimal_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

using ::google::cloud::bigquery_v2_minimal_testing::MockTableRestStub;
using ::testing::Contains;
using ::testing::ElementsAre;
using ::testing::HasSubstr;
using ::testing::IsEmpty;

static auto const kUserProject = "test-only-project";
static auto const kQuotaUser = "test-quota-user";

std::shared_ptr<TableMetadata> CreateMockTableMetadata(
    std::shared_ptr<TableRestStub> mock) {
  return std::make_shared<TableMetadata>(std::move(mock));
}

void VerifyMetadataContext(rest_internal::RestContext& context) {
  EXPECT_THAT(context.GetHeader("x-goog-api-client"),
              Contains(HasSubstr("bigquery_v2_table")));
  EXPECT_THAT(context.GetHeader("x-goog-request-params"), IsEmpty());
  EXPECT_THAT(context.GetHeader("x-goog-user-project"),
              ElementsAre(kUserProject));
  EXPECT_THAT(context.GetHeader("x-goog-quota-user"), ElementsAre(kQuotaUser));
  EXPECT_THAT(context.GetHeader("x-server-timeout"), ElementsAre("3.141"));
}

Options GetMetadataOptions() {
  return Options{}
      .set<UserProjectOption>(kUserProject)
      .set<QuotaUserOption>(kQuotaUser)
      .set<ServerTimeoutOption>(std::chrono::milliseconds(3141));
}

TEST(TableMetadataTest, GetTable) {
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

  auto metadata = CreateMockTableMetadata(std::move(mock_stub));
  GetTableRequest request = bigquery_v2_minimal_testing::MakeGetTableRequest();
  rest_internal::RestContext context;

  internal::OptionsSpan span(GetMetadataOptions());

  auto result = metadata->GetTable(context, request);
  ASSERT_STATUS_OK(result);
  VerifyMetadataContext(context);
}

TEST(TableMetadataTest, ListTables) {
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

  auto metadata = CreateMockTableMetadata(std::move(mock_stub));
  ListTablesRequest request =
      bigquery_v2_minimal_testing::MakeListTablesRequest();
  rest_internal::RestContext context;

  internal::OptionsSpan span(GetMetadataOptions());

  auto result = metadata->ListTables(context, request);
  ASSERT_STATUS_OK(result);
  VerifyMetadataContext(context);
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigquery_v2_minimal_internal
}  // namespace cloud
}  // namespace google
