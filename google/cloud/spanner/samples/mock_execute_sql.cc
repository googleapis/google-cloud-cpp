// Copyright 2019 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

//! [all]

//! [required-includes]
#include "google/cloud/spanner/client.h"
#include "google/cloud/spanner/testing/mock_spanner_connection.h"
#include <google/protobuf/text_format.h>
#include <gmock/gmock.h>
//! [required-includes]

namespace {

//! [helper-aliases]
using ::testing::_;
using ::testing::Return;
namespace spanner = google::cloud::spanner;
//! [helper-aliases]

TEST(MockSpannerClient, SuccessfulExecuteSql) {
  // Create a mock object to stream the results of a ExecuteSql query.
  //! [create-streaming-source]
  auto source =
      std::unique_ptr<google::cloud::spanner_testing::MockResultSetSource>(
          new google::cloud::spanner_testing::MockResultSetSource);
  //! [create-streaming-source]

  // Setup the return type of the ExecuteSql results:
  //! [return-metadata]
  google::spanner::v1::ResultSetMetadata metadata;
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(
      R"pb(
        row_type: {
          fields: {
            name: "Id",
            type: { code: INT64 }
          }
          fields: {
            name: "Greeting",
            type: { code: STRING }
          }
        }
      )pb",
      &metadata));
  EXPECT_CALL(*source, Metadata()).WillRepeatedly(Return(metadata));
  //! [return-metadata]

  // Setup the mock source to return some values:
  //! [simulate-streaming-results]
  EXPECT_CALL(*source, NextValue())
      .WillOnce(Return(google::cloud::optional<spanner::Value>(1)))
      .WillOnce(Return(google::cloud::optional<spanner::Value>("Hello World")))
      .WillOnce(Return(google::cloud::optional<spanner::Value>(2)))
      .WillOnce(Return(google::cloud::optional<spanner::Value>("Hello World")))
      //! [simulate-streaming-results]
      //! [simulate-streaming-end]
      .WillOnce(Return(google::cloud::optional<spanner::Value>()));
  //! [simulate-streaming-end]

  // Create a mock for `spanner::Connection`:
  //! [create-mock]
  auto conn =
      std::make_shared<google::cloud::spanner_testing::MockConnection>();
  //! [create-mock]

  // Setup the connection mock to return the results previously setup:
  //! [mock-execute-sql]
  EXPECT_CALL(*conn, ExecuteSql(_))
      .WillOnce([&source](spanner::Connection::ExecuteSqlParams const&)
                    -> google::cloud::StatusOr<spanner::ResultSet> {
        return spanner::ResultSet(std::move(source));
      });
  //! [mock-execute-sql]

  // Create a client with the mocked connection:
  //! [create-client]
  spanner::Client client(conn);
  //! [create-client]

  // Make the request and verify the expected results:
  //! [client-call]
  auto reader = client.ExecuteSql(
      spanner::SqlStatement("SELECT Id, Greeting FROM Greetings"));
  ASSERT_TRUE(reader);
  //! [client-call]

  //! [expected-results]
  int count = 0;
  for (auto row : reader->Rows<std::int64_t, std::string>()) {
    ASSERT_TRUE(row);
    auto expected_id = ++count;
    EXPECT_EQ(expected_id, row->get<0>());
    EXPECT_EQ("Hello World", row->get<1>());
  }
  //! [expected-results]
}

}  // namespace

//! [all]
