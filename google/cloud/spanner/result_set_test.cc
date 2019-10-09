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

#include "google/cloud/spanner/result_set.h"
#include "google/cloud/spanner/internal/time.h"
#include "google/cloud/spanner/mocks/mock_spanner_connection.h"
#include "google/cloud/spanner/timestamp.h"
#include "google/cloud/internal/make_unique.h"
#include <gmock/gmock.h>
#include <chrono>
#include <string>

namespace google {
namespace cloud {
namespace spanner {
inline namespace SPANNER_CLIENT_NS {
namespace {

namespace spanner_proto = ::google::spanner::v1;

using ::google::cloud::internal::make_unique;
using ::google::cloud::spanner_mocks::MockResultSetSource;
using ::testing::Return;

TEST(ReadResult, IterateNoRows) {
  auto mock_source = make_unique<MockResultSetSource>();
  EXPECT_CALL(*mock_source, NextValue()).WillOnce(Return(optional<Value>()));

  ReadResult result_set(std::move(mock_source));
  int num_rows = 0;
  for (auto const& row : result_set.Rows<Row<bool>>()) {
    static_cast<void>(row);
    ++num_rows;
  }
  EXPECT_EQ(num_rows, 0);
}

TEST(ReadResult, IterateOverRows) {
  auto mock_source = make_unique<MockResultSetSource>();
  EXPECT_CALL(*mock_source, NextValue())
      .WillOnce(Return(optional<Value>(5)))
      .WillOnce(Return(optional<Value>(true)))
      .WillOnce(Return(optional<Value>("foo")))
      .WillOnce(Return(optional<Value>(10)))
      .WillOnce(Return(optional<Value>(false)))
      .WillOnce(Return(optional<Value>("bar")))
      .WillOnce(Return(optional<Value>()));

  ReadResult result_set(std::move(mock_source));
  int num_rows = 0;
  for (auto const& row :
       result_set.Rows<Row<std::int64_t, bool, std::string>>()) {
    EXPECT_TRUE(row.ok());
    switch (num_rows++) {
      case 0:
        EXPECT_EQ(row->get<0>(), 5);
        EXPECT_EQ(row->get<1>(), true);
        EXPECT_EQ(row->get<2>(), "foo");
        break;

      case 1:
        EXPECT_EQ(row->get<0>(), 10);
        EXPECT_EQ(row->get<1>(), false);
        EXPECT_EQ(row->get<2>(), "bar");
        break;

      default:
        ADD_FAILURE() << "Unexpected row number " << num_rows;
        break;
    }
  }
  EXPECT_EQ(num_rows, 2);
}

TEST(ReadResult, IterateError) {
  auto mock_source = make_unique<MockResultSetSource>();
  EXPECT_CALL(*mock_source, NextValue())
      .WillOnce(Return(optional<Value>(5)))
      .WillOnce(Return(optional<Value>(true)))
      .WillOnce(Return(optional<Value>("foo")))
      .WillOnce(Return(optional<Value>(10)))
      .WillOnce(Return(Status(StatusCode::kUnknown, "oops")));

  ReadResult result_set(std::move(mock_source));
  int num_rows = 0;
  for (auto const& row :
       result_set.Rows<Row<std::int64_t, bool, std::string>>()) {
    switch (num_rows++) {
      case 0:
        EXPECT_TRUE(row.ok());
        EXPECT_EQ(row->get<0>(), 5);
        EXPECT_EQ(row->get<1>(), true);
        EXPECT_EQ(row->get<2>(), "foo");
        break;

      case 1:
        EXPECT_FALSE(row.ok());
        EXPECT_EQ(row.status().code(), StatusCode::kUnknown);
        EXPECT_EQ(row.status().message(), "oops");
        break;

      default:
        ADD_FAILURE() << "Unexpected row number " << num_rows;
        break;
    }
  }
  EXPECT_EQ(num_rows, 2);
}

TEST(ReadResult, TimestampNoTransaction) {
  auto mock_source = make_unique<MockResultSetSource>();
  spanner_proto::ResultSetMetadata no_transaction;
  EXPECT_CALL(*mock_source, Metadata()).WillOnce(Return(no_transaction));

  ReadResult result_set(std::move(mock_source));
  EXPECT_FALSE(result_set.ReadTimestamp().has_value());
}

TEST(ReadResult, TimestampNotPresent) {
  auto mock_source = make_unique<MockResultSetSource>();
  spanner_proto::ResultSetMetadata transaction_no_timestamp;
  transaction_no_timestamp.mutable_transaction()->set_id("dummy");
  EXPECT_CALL(*mock_source, Metadata())
      .WillOnce(Return(transaction_no_timestamp));

  ReadResult result_set(std::move(mock_source));
  EXPECT_FALSE(result_set.ReadTimestamp().has_value());
}

TEST(ReadResult, TimestampPresent) {
  auto mock_source = make_unique<MockResultSetSource>();
  spanner_proto::ResultSetMetadata transaction_with_timestamp;
  transaction_with_timestamp.mutable_transaction()->set_id("dummy2");
  Timestamp timestamp = std::chrono::system_clock::now();
  *transaction_with_timestamp.mutable_transaction()->mutable_read_timestamp() =
      internal::ToProto(timestamp);
  EXPECT_CALL(*mock_source, Metadata())
      .WillOnce(Return(transaction_with_timestamp));

  ReadResult result_set(std::move(mock_source));
  EXPECT_EQ(*result_set.ReadTimestamp(), timestamp);
}

}  // namespace
}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner
}  // namespace cloud
}  // namespace google
