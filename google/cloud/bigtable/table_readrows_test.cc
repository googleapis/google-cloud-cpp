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

#include "google/cloud/bigtable/table.h"
#include "google/cloud/bigtable/testing/mock_read_rows_reader.h"
#include "google/cloud/bigtable/testing/table_test_fixture.h"
#include "google/cloud/testing_util/status_matchers.h"
#include "absl/memory/memory.h"

namespace google {
namespace cloud {
namespace bigtable {
inline namespace BIGTABLE_CLIENT_NS {
namespace {

using ::google::cloud::bigtable::testing::MockReadRowsReader;
using ::testing::DoAll;
using ::testing::Return;
using ::testing::SetArgPointee;
using ::testing::WithoutArgs;

class TableReadRowsTest : public bigtable::testing::TableTestFixture {
 public:
  TableReadRowsTest() : TableTestFixture(CompletionQueue{}) {}
};

TEST_F(TableReadRowsTest, ReadRowsCanReadOneRow) {
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

  // must be a new pointer, it is wrapped in unique_ptr by ReadRows
  auto* stream = new MockReadRowsReader("google.bigtable.v2.Bigtable.ReadRows");
  EXPECT_CALL(*stream, Read)
      .WillOnce(DoAll(SetArgPointee<0>(response), Return(true)))
      .WillOnce(Return(false));
  EXPECT_CALL(*stream, Finish()).WillOnce(Return(grpc::Status::OK));
  EXPECT_CALL(*client_, ReadRows).WillOnce(stream->MakeMockReturner());

  auto reader =
      table_.ReadRows(bigtable::RowSet(), bigtable::Filter::PassAllFilter());

  auto it = reader.begin();
  EXPECT_NE(it, reader.end());
  ASSERT_STATUS_OK(*it);
  EXPECT_EQ((*it)->row_key(), "r1");
  EXPECT_EQ(++it, reader.end());
}

TEST_F(TableReadRowsTest, ReadRowsCanReadWithRetries) {
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

  auto response_retry = bigtable::testing::ReadRowsResponseFromString(R"(
      chunks {
        row_key: "r2"
        family_name { value: "fam" }
        qualifier { value: "qual" }
        timestamp_micros: 42000
        value: "value"
        commit_row: true
      }
      )");

  // must be a new pointer, it is wrapped in unique_ptr by ReadRows
  auto* stream = new MockReadRowsReader("google.bigtable.v2.Bigtable.ReadRows");
  auto* stream_retry =
      new MockReadRowsReader("google.bigtable.v2.Bigtable.ReadRows");

  EXPECT_CALL(*client_, ReadRows)
      .WillOnce(stream->MakeMockReturner())
      .WillOnce(stream_retry->MakeMockReturner());

  EXPECT_CALL(*stream, Read)
      .WillOnce(DoAll(SetArgPointee<0>(response), Return(true)))
      .WillOnce(Return(false));

  EXPECT_CALL(*stream, Finish())
      .WillOnce(
          Return(grpc::Status(grpc::StatusCode::UNAVAILABLE, "try-again")));

  EXPECT_CALL(*stream_retry, Read)
      .WillOnce(DoAll(SetArgPointee<0>(response_retry), Return(true)))
      .WillOnce(Return(false));

  EXPECT_CALL(*stream_retry, Finish()).WillOnce(Return(grpc::Status::OK));

  auto reader =
      table_.ReadRows(bigtable::RowSet(), bigtable::Filter::PassAllFilter());

  auto it = reader.begin();
  EXPECT_NE(it, reader.end());
  ASSERT_STATUS_OK(*it);
  EXPECT_EQ((*it)->row_key(), "r1");
  ++it;
  EXPECT_NE(it, reader.end());
  ASSERT_STATUS_OK(*it);
  EXPECT_EQ((*it)->row_key(), "r2");
  EXPECT_EQ(++it, reader.end());
}

TEST_F(TableReadRowsTest, ReadRowsThrowsWhenTooManyErrors) {
  EXPECT_CALL(*client_, ReadRows).WillRepeatedly(WithoutArgs([] {
    auto stream = absl::make_unique<MockReadRowsReader>(
        "google.bigtable.v2.Bigtable.ReadRows");
    EXPECT_CALL(*stream, Read).WillOnce(Return(false));
    EXPECT_CALL(*stream, Finish())
        .WillOnce(
            Return(grpc::Status(grpc::StatusCode::UNAVAILABLE, "broken")));
    return stream;
  }));

  auto table = bigtable::Table(
      client_, "table_id", bigtable::LimitedErrorCountRetryPolicy(3),
      bigtable::ExponentialBackoffPolicy(std::chrono::seconds(0),
                                         std::chrono::seconds(0)),
      bigtable::SafeIdempotentMutationPolicy());
  auto reader =
      table.ReadRows(bigtable::RowSet(), bigtable::Filter::PassAllFilter());

  auto it = reader.begin();
  ASSERT_NE(reader.end(), it);
  ASSERT_FALSE(*it);
  ++it;
  ASSERT_EQ(reader.end(), it);
}

}  // anonymous namespace
}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable
}  // namespace cloud
}  // namespace google
