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

#include "google/cloud/spanner/results.h"
#include "google/cloud/spanner/mocks/mock_spanner_connection.h"
#include "google/cloud/spanner/timestamp.h"
#include "google/cloud/testing_util/assert_ok.h"
#include "google/cloud/testing_util/is_proto_equal.h"
#include "google/cloud/testing_util/status_matchers.h"
#include "absl/memory/memory.h"
#include <google/protobuf/text_format.h>
#include <gmock/gmock.h>
#include <chrono>
#include <iostream>
#include <string>

namespace google {
namespace cloud {
namespace spanner {
inline namespace SPANNER_CLIENT_NS {
namespace {

namespace spanner_proto = ::google::spanner::v1;

using ::google::cloud::spanner_mocks::MockResultSetSource;
using ::google::cloud::testing_util::IsProtoEqual;
using ::google::cloud::testing_util::StatusIs;
using ::google::protobuf::TextFormat;
using ::testing::Eq;
using ::testing::Return;
using ::testing::UnorderedPointwise;

TEST(RowStream, IterateNoRows) {
  auto mock_source = absl::make_unique<MockResultSetSource>();
  EXPECT_CALL(*mock_source, NextRow()).WillOnce(Return(Row()));

  RowStream rows(std::move(mock_source));
  int num_rows = 0;
  for (auto const& row : rows) {
    static_cast<void>(row);
    ++num_rows;
  }
  EXPECT_EQ(num_rows, 0);
}

TEST(RowStream, IterateOverRows) {
  auto mock_source = absl::make_unique<MockResultSetSource>();
  EXPECT_CALL(*mock_source, NextRow())
      .WillOnce(Return(MakeTestRow(5, true, "foo")))
      .WillOnce(Return(MakeTestRow(10, false, "bar")))
      .WillOnce(Return(Row()));

  RowStream rows(std::move(mock_source));
  int num_rows = 0;
  for (auto const& row :
       StreamOf<std::tuple<std::int64_t, bool, std::string>>(rows)) {
    ASSERT_STATUS_OK(row);
    switch (num_rows++) {
      case 0:
        EXPECT_EQ(std::get<0>(*row), 5);
        EXPECT_EQ(std::get<1>(*row), true);
        EXPECT_EQ(std::get<2>(*row), "foo");
        break;

      case 1:
        EXPECT_EQ(std::get<0>(*row), 10);
        EXPECT_EQ(std::get<1>(*row), false);
        EXPECT_EQ(std::get<2>(*row), "bar");
        break;

      default:
        ADD_FAILURE() << "Unexpected row number " << num_rows;
        break;
    }
  }
  EXPECT_EQ(num_rows, 2);
}

TEST(RowStream, IterateError) {
  auto mock_source = absl::make_unique<MockResultSetSource>();
  EXPECT_CALL(*mock_source, NextRow())
      .WillOnce(Return(MakeTestRow(5, true, "foo")))
      .WillOnce(Return(Status(StatusCode::kUnknown, "oops")));

  RowStream rows(std::move(mock_source));

  int num_rows = 0;
  for (auto const& row :
       StreamOf<std::tuple<std::int64_t, bool, std::string>>(rows)) {
    switch (num_rows++) {
      case 0:
        ASSERT_STATUS_OK(row);
        EXPECT_EQ(std::get<0>(*row), 5);
        EXPECT_EQ(std::get<1>(*row), true);
        EXPECT_EQ(std::get<2>(*row), "foo");
        break;

      case 1:
        EXPECT_THAT(row, StatusIs(StatusCode::kUnknown, "oops"));
        break;

      default:
        ADD_FAILURE() << "Unexpected row number " << num_rows;
        break;
    }
  }
  EXPECT_EQ(num_rows, 2);
}

TEST(RowStream, TimestampNoTransaction) {
  auto mock_source = absl::make_unique<MockResultSetSource>();
  spanner_proto::ResultSetMetadata no_transaction;
  EXPECT_CALL(*mock_source, Metadata()).WillOnce(Return(no_transaction));

  RowStream rows(std::move(mock_source));
  EXPECT_FALSE(rows.ReadTimestamp().has_value());
}

TEST(RowStream, TimestampNotPresent) {
  auto mock_source = absl::make_unique<MockResultSetSource>();
  spanner_proto::ResultSetMetadata transaction_no_timestamp;
  transaction_no_timestamp.mutable_transaction()->set_id("dummy");
  EXPECT_CALL(*mock_source, Metadata())
      .WillOnce(Return(transaction_no_timestamp));

  RowStream rows(std::move(mock_source));
  EXPECT_FALSE(rows.ReadTimestamp().has_value());
}

TEST(RowStream, TimestampPresent) {
  auto mock_source = absl::make_unique<MockResultSetSource>();
  spanner_proto::ResultSetMetadata transaction_with_timestamp;
  transaction_with_timestamp.mutable_transaction()->set_id("dummy2");
  Timestamp timestamp = MakeTimestamp(std::chrono::system_clock::now()).value();
  *transaction_with_timestamp.mutable_transaction()->mutable_read_timestamp() =
      timestamp.get<protobuf::Timestamp>().value();
  EXPECT_CALL(*mock_source, Metadata())
      .WillOnce(Return(transaction_with_timestamp));

  RowStream rows(std::move(mock_source));
  EXPECT_EQ(*rows.ReadTimestamp(), timestamp);
}

TEST(ProfileQueryResult, TimestampPresent) {
  auto mock_source = absl::make_unique<MockResultSetSource>();
  spanner_proto::ResultSetMetadata transaction_with_timestamp;
  transaction_with_timestamp.mutable_transaction()->set_id("dummy2");
  Timestamp timestamp = MakeTimestamp(std::chrono::system_clock::now()).value();
  *transaction_with_timestamp.mutable_transaction()->mutable_read_timestamp() =
      timestamp.get<protobuf::Timestamp>().value();
  EXPECT_CALL(*mock_source, Metadata())
      .WillOnce(Return(transaction_with_timestamp));

  ProfileQueryResult rows(std::move(mock_source));
  EXPECT_EQ(*rows.ReadTimestamp(), timestamp);
}

TEST(ProfileQueryResult, ExecutionStats) {
  auto mock_source = absl::make_unique<MockResultSetSource>();
  auto constexpr kText = R"pb(
    query_stats {
      fields {
        key: "elapsed_time"
        value { string_value: "42 secs" }
      }
    }
  )pb";
  google::spanner::v1::ResultSetStats stats;
  ASSERT_TRUE(TextFormat::ParseFromString(kText, &stats));
  EXPECT_CALL(*mock_source, Stats()).WillOnce(Return(stats));

  std::vector<std::pair<const std::string, std::string>> expected;
  expected.emplace_back("elapsed_time", "42 secs");
  ProfileQueryResult query_result(std::move(mock_source));
  EXPECT_THAT(*query_result.ExecutionStats(),
              UnorderedPointwise(Eq(), expected));
}

TEST(ProfileQueryResult, ExecutionPlan) {
  auto mock_source = absl::make_unique<MockResultSetSource>();
  auto constexpr kText = R"pb(
    query_plan { plan_nodes: { index: 42 } }
  )pb";
  google::spanner::v1::ResultSetStats stats;
  ASSERT_TRUE(TextFormat::ParseFromString(kText, &stats));
  EXPECT_CALL(*mock_source, Stats()).WillRepeatedly(Return(stats));

  ProfileQueryResult query_result(std::move(mock_source));
  EXPECT_THAT(*query_result.ExecutionPlan(), IsProtoEqual(stats.query_plan()));
}

TEST(DmlResult, RowsModified) {
  auto mock_source = absl::make_unique<MockResultSetSource>();
  auto constexpr kText = R"pb(
    row_count_exact: 42
  )pb";
  google::spanner::v1::ResultSetStats stats;
  ASSERT_TRUE(TextFormat::ParseFromString(kText, &stats));
  EXPECT_CALL(*mock_source, Stats()).WillOnce(Return(stats));

  DmlResult dml_result(std::move(mock_source));
  EXPECT_EQ(dml_result.RowsModified(), 42);
}

TEST(ProfileDmlResult, RowsModified) {
  auto mock_source = absl::make_unique<MockResultSetSource>();
  auto constexpr kText = R"pb(
    row_count_exact: 42
  )pb";
  google::spanner::v1::ResultSetStats stats;
  ASSERT_TRUE(TextFormat::ParseFromString(kText, &stats));
  EXPECT_CALL(*mock_source, Stats()).WillOnce(Return(stats));

  ProfileDmlResult dml_result(std::move(mock_source));
  EXPECT_EQ(dml_result.RowsModified(), 42);
}

TEST(ProfileDmlResult, ExecutionStats) {
  auto mock_source = absl::make_unique<MockResultSetSource>();
  auto constexpr kText =
      R"pb(
    query_stats {
      fields {
        key: "elapsed_time"
        value { string_value: "42 secs" }
      }
    })pb";
  google::spanner::v1::ResultSetStats stats;
  ASSERT_TRUE(TextFormat::ParseFromString(kText, &stats));
  EXPECT_CALL(*mock_source, Stats()).WillOnce(Return(stats));

  std::vector<std::pair<const std::string, std::string>> expected;
  expected.emplace_back("elapsed_time", "42 secs");
  ProfileDmlResult dml_result(std::move(mock_source));
  EXPECT_THAT(*dml_result.ExecutionStats(), UnorderedPointwise(Eq(), expected));
}

}  // namespace
}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner
}  // namespace cloud
}  // namespace google
