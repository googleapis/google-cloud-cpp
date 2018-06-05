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

namespace bigtable = google::cloud::bigtable;
using testing::_;
using testing::DoAll;
using testing::Invoke;
using testing::Return;
using testing::SetArgPointee;

/// Define helper types and functions for this test.
namespace {
class TableReadRowsTest : public bigtable::testing::TableTestFixture {};
using bigtable::testing::MockReadRowsReader;
}  // anonymous namespace

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
  auto stream = new MockReadRowsReader;
  EXPECT_CALL(*stream, Read(_))
      .WillOnce(DoAll(SetArgPointee<0>(response), Return(true)))
      .WillOnce(Return(false));
  EXPECT_CALL(*stream, Finish()).WillOnce(Return(grpc::Status::OK));
  EXPECT_CALL(*client_, ReadRows(_, _))
      .WillOnce(Invoke(stream->MakeMockReturner()));

  auto reader =
      table_.ReadRows(bigtable::RowSet(), bigtable::Filter::PassAllFilter());

  auto it = reader.begin();
  EXPECT_NE(it, reader.end());
  EXPECT_EQ(it->row_key(), "r1");
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
  auto stream = new MockReadRowsReader;
  auto stream_retry = new MockReadRowsReader;

  EXPECT_CALL(*client_, ReadRows(_, _))
      .WillOnce(Invoke(stream->MakeMockReturner()))
      .WillOnce(Invoke(stream_retry->MakeMockReturner()));

  EXPECT_CALL(*stream, Read(_))
      .WillOnce(DoAll(SetArgPointee<0>(response), Return(true)))
      .WillOnce(Return(false));

  EXPECT_CALL(*stream, Finish())
      .WillOnce(
          Return(grpc::Status(grpc::StatusCode::UNAVAILABLE, "try-again")));

  EXPECT_CALL(*stream_retry, Read(_))
      .WillOnce(DoAll(SetArgPointee<0>(response_retry), Return(true)))
      .WillOnce(Return(false));

  EXPECT_CALL(*stream_retry, Finish()).WillOnce(Return(grpc::Status::OK));

  auto reader =
      table_.ReadRows(bigtable::RowSet(), bigtable::Filter::PassAllFilter());

  auto it = reader.begin();
  EXPECT_NE(it, reader.end());
  EXPECT_EQ(it->row_key(), "r1");
  ++it;
  EXPECT_NE(it, reader.end());
  EXPECT_EQ(it->row_key(), "r2");
  EXPECT_EQ(++it, reader.end());
}

#if GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
TEST_F(TableReadRowsTest, ReadRowsThrowsWhenTooManyErrors) {
  EXPECT_CALL(*client_, ReadRows(_, _))
      .WillRepeatedly(testing::WithoutArgs(testing::Invoke([] {
        auto stream = new MockReadRowsReader;
        EXPECT_CALL(*stream, Read(_)).WillOnce(Return(false));
        EXPECT_CALL(*stream, Finish())
            .WillOnce(
                Return(grpc::Status(grpc::StatusCode::UNAVAILABLE, "broken")));
        return stream->AsUniqueMocked();
      })));

  auto table = bigtable::Table(
      client_, "table_id", bigtable::LimitedErrorCountRetryPolicy(3),
      bigtable::ExponentialBackoffPolicy(std::chrono::seconds(0),
                                         std::chrono::seconds(0)),
      bigtable::SafeIdempotentMutationPolicy());
  auto reader =
      table.ReadRows(bigtable::RowSet(), bigtable::Filter::PassAllFilter());

  EXPECT_THROW(reader.begin(), std::exception);
}
#endif  // GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
