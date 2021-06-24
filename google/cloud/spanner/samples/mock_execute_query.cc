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
#include "google/cloud/spanner/mocks/mock_spanner_connection.h"
#include "absl/memory/memory.h"
#include <google/protobuf/text_format.h>
#include <gmock/gmock.h>
//! [required-includes]

namespace {

//! [helper-aliases]
using ::testing::Return;
namespace spanner = ::google::cloud::spanner;
//! [helper-aliases]

TEST(MockSpannerClient, SuccessfulExecuteQuery) {
  // Create a mock object to stream the results of a ExecuteQuery.
  //! [create-streaming-source]
  auto source =
      absl::make_unique<google::cloud::spanner_mocks::MockResultSetSource>();
  //! [create-streaming-source]

  // Setup the return type of the ExecuteQuery results:
  //! [return-metadata]
  auto constexpr kText = R"pb(
    row_type: {
      fields: {
        name: "Id",
        type: { code: INT64 }
      }
      fields: {
        name: "Greeting",
        type: { code: STRING }
      }
    })pb";
  google::spanner::v1::ResultSetMetadata metadata;
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(kText, &metadata));
  EXPECT_CALL(*source, Metadata()).WillRepeatedly(Return(metadata));
  //! [return-metadata]

  // Setup the mock source to return some values:
  //! [simulate-streaming-results]
  EXPECT_CALL(*source, NextRow())
      .WillOnce(Return(
          spanner::MakeTestRow({{"Id", spanner::Value(1)},
                                {"Greeting", spanner::Value("Hello World")}})))
      .WillOnce(Return(
          spanner::MakeTestRow({{"Id", spanner::Value(2)},
                                {"Greeting", spanner::Value("Hello World")}})))
      //! [simulate-streaming-results]
      //! [simulate-streaming-end]
      .WillOnce(Return(spanner::Row()));
  //! [simulate-streaming-end]

  // Create a mock for `spanner::Connection`:
  //! [create-mock]
  auto conn = std::make_shared<google::cloud::spanner_mocks::MockConnection>();
  //! [create-mock]

  // Setup the connection mock to return the results previously setup:
  //! [mock-execute-query]
  EXPECT_CALL(*conn, ExecuteQuery)
      .WillOnce([&source](spanner::Connection::SqlParams const&)
                    -> spanner::RowStream {
        return spanner::RowStream(std::move(source));
      });
  //! [mock-execute-query]

  // Create a client with the mocked connection:
  //! [create-client]
  spanner::Client client(conn);
  //! [create-client]

  // Make the request and verify the expected results:
  //! [client-call]
  auto rows = client.ExecuteQuery(
      spanner::SqlStatement("SELECT Id, Greeting FROM Greetings"));
  //! [client-call]

  //! [expected-results]
  int count = 0;
  using RowType = std::tuple<std::int64_t, std::string>;
  for (auto const& row : spanner::StreamOf<RowType>(rows)) {
    ASSERT_TRUE(row);
    auto expected_id = ++count;
    EXPECT_EQ(expected_id, std::get<0>(*row));
    EXPECT_EQ("Hello World", std::get<1>(*row));
  }
  //! [expected-results]
}

}  // namespace

//! [all]
