// Copyright 2017 Google Inc.
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

#include "bigtable/client/row_reader.h"

#include "bigtable/client/table.h"
#include "bigtable/client/testing/table_test_fixture.h"

#include <gmock/gmock.h>
#include <grpc++/test/mock_stream.h>

using testing::_;
using testing::DoAll;
using testing::Return;
using testing::SetArgPointee;

using MockResponseStream =
    grpc::testing::MockClientReader<google::bigtable::v2::ReadRowsResponse>;

namespace {
class ReadRowsParserMock : public bigtable::internal::ReadRowsParser {};
}  // anonymous namespace

class RowReaderTest : public bigtable::testing::TableTestFixture {
 public:
  RowReaderTest()
      : stream_(new MockResponseStream()),
        no_retry_policy_(0),
        backoff_policy_(std::chrono::seconds(0), std::chrono::seconds(0)) {
    EXPECT_CALL(*bigtable_stub_, ReadRowsRaw(_, _)).WillOnce(Return(stream_));
  }

  // must be a new pointer, it is wrapped in unique_ptr by ReadRows
  MockResponseStream *stream_;

  bigtable::LimitedErrorCountRetryPolicy no_retry_policy_;
  bigtable::ExponentialBackoffPolicy backoff_policy_;
};

TEST_F(RowReaderTest, EmptyReaderHasNoRows) {
  EXPECT_CALL(*stream_, Read(_)).WillOnce(Return(false));

  bigtable::RowReader reader(
      client_, "", bigtable::RowSet(), bigtable::RowReader::NO_ROWS_LIMIT,
      bigtable::Filter::PassAllFilter(), no_retry_policy_.clone(),
      backoff_policy_.clone(), absl::make_unique<ReadRowsParserMock>());

  EXPECT_EQ(reader.begin(), reader.end());
}

TEST_F(RowReaderTest, ReadOneRow) {
  auto response = bigtable::testing::ReadRowsResponseFromString(R"(
      chunks {
        row_key: "r1"
        family_name { value: "fam" }
        qualifier { value: "qual" }
        timestamp_micros: 42000
        value: "value"
        commit_row: true
      }
      )");
  EXPECT_CALL(*stream_, Read(_))
      .WillOnce(DoAll(SetArgPointee<0>(response), Return(true)))
      .WillOnce(Return(false));

  bigtable::RowReader reader(
      client_, "", bigtable::RowSet(), bigtable::RowReader::NO_ROWS_LIMIT,
      bigtable::Filter::PassAllFilter(), no_retry_policy_.clone(),
      backoff_policy_.clone(), absl::make_unique<ReadRowsParserMock>());

  auto it = reader.begin();

  EXPECT_NE(it, reader.end());

  EXPECT_EQ(it->row_key(), "r1");

  EXPECT_EQ(std::next(it), reader.end());
}
