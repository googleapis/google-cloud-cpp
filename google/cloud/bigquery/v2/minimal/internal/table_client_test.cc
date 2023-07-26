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

#include "google/cloud/bigquery/v2/minimal/internal/table_client.h"
#include "google/cloud/bigquery/v2/minimal/mocks/mock_table_connection.h"
#include "google/cloud/bigquery/v2/minimal/testing/table_test_utils.h"
#include "google/cloud/mocks/mock_stream_range.h"
#include "google/cloud/internal/make_status.h"
#include "google/cloud/internal/pagination_range.h"
#include "google/cloud/internal/rest_response.h"
#include "google/cloud/status_or.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace bigquery_v2_minimal_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

using ::google::cloud::rest_internal::HttpStatusCode;
using ::google::cloud::testing_util::StatusIs;
using ::testing::HasSubstr;
using ::testing::IsEmpty;
using ::testing::Not;
using ::testing::Return;

TEST(TableClientTest, GetTableSuccess) {
  auto mock_table_connection = std::make_shared<MockTableConnection>();
  auto table = bigquery_v2_minimal_testing::MakeTable();
  EXPECT_CALL(*mock_table_connection, GetTable)
      .WillOnce([&](GetTableRequest const& request) {
        EXPECT_THAT(request.project_id(), Not(IsEmpty()));
        EXPECT_THAT(request.dataset_id(), Not(IsEmpty()));
        EXPECT_THAT(request.table_id(), Not(IsEmpty()));
        return make_status_or(table);
      });

  TableClient table_client(mock_table_connection);

  GetTableRequest request = bigquery_v2_minimal_testing::MakeGetTableRequest();
  auto expected = bigquery_v2_minimal_testing::MakeTable();

  auto actual = table_client.GetTable(request);

  ASSERT_STATUS_OK(actual);
  bigquery_v2_minimal_testing::AssertEquals(expected, *actual);
}

TEST(TableClientTest, GetTableFailure) {
  auto mock_table_connection = std::make_shared<MockTableConnection>();

  EXPECT_CALL(*mock_table_connection, GetTable)
      .WillOnce(Return(rest_internal::AsStatus(HttpStatusCode::kBadRequest,
                                               "bad-request-error")));

  TableClient table_client(mock_table_connection);

  GetTableRequest request;

  auto result = table_client.GetTable(request);
  EXPECT_THAT(result, StatusIs(StatusCode::kInvalidArgument,
                               HasSubstr("bad-request-error")));
}

TEST(TableClientTest, ListTablesSuccess) {
  auto mock = std::make_shared<MockTableConnection>();
  EXPECT_CALL(*mock, options);

  ListFormatTable list_format_table =
      bigquery_v2_minimal_testing::MakeListFormatTable();

  EXPECT_CALL(*mock, ListTables)
      .WillOnce([list_format_table](ListTablesRequest const& request) {
        EXPECT_EQ(request.project_id(), "test-project-id");
        EXPECT_EQ(request.dataset_id(), "test-dataset-id");
        return mocks::MakeStreamRange<ListFormatTable>({list_format_table});
        ;
      });

  TableClient client(std::move(mock));
  ListTablesRequest request;
  request.set_project_id("test-project-id");
  request.set_dataset_id("test-dataset-id");

  auto range = client.ListTables(request);
  auto begin = range.begin();
  ASSERT_NE(begin, range.end());
  EXPECT_THAT(*begin, StatusIs(StatusCode::kOk));
}

TEST(TableClientTest, ListTablesFailure) {
  auto mock = std::make_shared<MockTableConnection>();
  EXPECT_CALL(*mock, options);

  EXPECT_CALL(*mock, ListTables).WillOnce([](ListTablesRequest const& request) {
    EXPECT_EQ(request.project_id(), "test-project-id");
    EXPECT_EQ(request.dataset_id(), "test-dataset-id");
    return mocks::MakeStreamRange<ListFormatTable>(
        {}, internal::PermissionDeniedError("denied"));
    ;
  });

  TableClient client(std::move(mock));
  ListTablesRequest request;
  request.set_project_id("test-project-id");
  request.set_dataset_id("test-dataset-id");

  auto range = client.ListTables(request);
  auto begin = range.begin();
  ASSERT_NE(begin, range.end());
  EXPECT_THAT(*begin, StatusIs(StatusCode::kPermissionDenied));
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigquery_v2_minimal_internal
}  // namespace cloud
}  // namespace google
