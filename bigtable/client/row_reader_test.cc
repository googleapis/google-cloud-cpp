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

class RowReaderTest : public bigtable::testing::TableTestFixture {
 public:
  RowReaderTest()
      : stream_(new MockResponseStream()),
        response_(),
        no_retry_policy_(0),
        backoff_policy_(std::chrono::seconds(0), std::chrono::seconds(0)) {
    EXPECT_CALL(*bigtable_stub_, ReadRowsRaw(_, _)).WillOnce(Return(stream_));

    auto chunk = response_.add_chunks();
    chunk->set_row_key("r1");
    chunk->mutable_family_name()->set_value("fam");
    chunk->mutable_qualifier()->set_value("qual");
    chunk->set_timestamp_micros(42000);
    chunk->set_value("value");
    chunk->set_commit_row(true);
  }

  // must be a new pointer, it is wrapped in unique_ptr by ReadRows
  MockResponseStream *stream_;

  // a simple fake response
  google::bigtable::v2::ReadRowsResponse response_;

  bigtable::LimitedErrorCountRetryPolicy no_retry_policy_;
  bigtable::ExponentialBackoffPolicy backoff_policy_;
};

TEST_F(RowReaderTest, EmptyReaderHasNoRows) {
  EXPECT_CALL(*stream_, Read(_)).WillOnce(Return(false));

  bigtable::RowReader reader(client_, "", bigtable::RowSet(),
                             bigtable::Table::NO_ROWS_LIMIT,
                             bigtable::Filter::PassAllFilter(),
                             no_retry_policy_.clone(), backoff_policy_.clone());

  EXPECT_EQ(reader.begin(), reader.end());
}

TEST_F(RowReaderTest, ReadOneRow) {
  EXPECT_CALL(*stream_, Read(_))
      .WillOnce(DoAll(SetArgPointee<0>(response_), Return(true)))
      .WillOnce(Return(false));

  bigtable::RowReader reader(client_, "", bigtable::RowSet(),
                             bigtable::Table::NO_ROWS_LIMIT,
                             bigtable::Filter::PassAllFilter(),
                             no_retry_policy_.clone(), backoff_policy_.clone());

  EXPECT_NE(reader.begin(), reader.end());

  EXPECT_EQ(reader.begin()->row_key(), response_.chunks(0).row_key());

  EXPECT_EQ(std::next(reader.begin()), reader.end());
}
