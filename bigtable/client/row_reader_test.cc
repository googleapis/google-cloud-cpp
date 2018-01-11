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

#include <deque>
#include <initializer_list>

using testing::_;
using testing::DoAll;
using testing::Eq;
using testing::Matcher;
using testing::Property;
using testing::Return;
using testing::SetArgPointee;

using MockResponseStream =
    grpc::testing::MockClientReader<google::bigtable::v2::ReadRowsResponse>;

using bigtable::Row;

namespace {
class ReadRowsParserMock : public bigtable::internal::ReadRowsParser {
 public:
  MOCK_METHOD1(HandleChunkHook,
               void(google::bigtable::v2::ReadRowsResponse_CellChunk chunk));
  void HandleChunk(
      google::bigtable::v2::ReadRowsResponse_CellChunk chunk) override {
    HandleChunkHook(chunk);
  }

  MOCK_METHOD0(HandleEndOfStreamHook, void());
  void HandleEndOfStream() override { HandleEndOfStreamHook(); }

  bool HasNext() const override { return not rows_.empty(); }

  Row Next() override {
    Row row = rows_.front();
    rows_.pop_front();
    return row;
  }

  void SetRows(std::initializer_list<std::string> l) {
    std::transform(l.begin(), l.end(), std::back_inserter(rows_),
                   [](std::string const& s) -> Row {
                     return Row(s, std::vector<bigtable::Cell>());
                   });
  }

 private:
  std::deque<Row> rows_;
};

class RetryPolicyMock : public bigtable::RPCRetryPolicy {
 public:
  RetryPolicyMock() {}
  std::unique_ptr<RPCRetryPolicy> clone() const override {
    throw std::runtime_error("Mocks cannot be copied.");
  }
  void setup(grpc::ClientContext& context) const override {}
  MOCK_METHOD1(on_failure_impl, bool(grpc::Status const& status));
  bool on_failure(grpc::Status const& status) override {
    return on_failure_impl(status);
  }
  bool can_retry(grpc::StatusCode code) const override { return true; }
};

class BackoffPolicyMock : public bigtable::RPCBackoffPolicy {
 public:
  BackoffPolicyMock() {}
  std::unique_ptr<RPCBackoffPolicy> clone() const override {
    throw std::runtime_error("Mocks cannot be copied.");
  }
  void setup(grpc::ClientContext& context) const override {}
  MOCK_METHOD1(on_completion_impl,
               std::chrono::milliseconds(grpc::Status const& s));
  std::chrono::milliseconds on_completion(grpc::Status const& s) override {
    return on_completion_impl(s);
  }
};

// Match the number of expected row keys in a request in EXPECT_CALL
Matcher<const google::bigtable::v2::ReadRowsRequest&> RequestRowKeysCount(
    int n) {
  return Property(
      &google::bigtable::v2::ReadRowsRequest::rows,
      Property(&google::bigtable::v2::RowSet::row_keys_size, Eq(n)));
}

}  // anonymous namespace

class RowReaderTest : public bigtable::testing::TableTestFixture {
 public:
  RowReaderTest()
      : stream_(new MockResponseStream()),
        retry_policy_(absl::make_unique<RetryPolicyMock>()),
        backoff_policy_(absl::make_unique<BackoffPolicyMock>()),
        parser_(absl::make_unique<ReadRowsParserMock>()) {}

  // must be a new pointer, it is wrapped in unique_ptr by ReadRows
  MockResponseStream* stream_;

  std::unique_ptr<RetryPolicyMock> retry_policy_;
  std::unique_ptr<BackoffPolicyMock> backoff_policy_;

  std::unique_ptr<ReadRowsParserMock> parser_;
};

TEST_F(RowReaderTest, EmptyReaderHasNoRows) {
  EXPECT_CALL(*bigtable_stub_, ReadRowsRaw(_, _)).WillOnce(Return(stream_));
  EXPECT_CALL(*stream_, Read(_)).WillOnce(Return(false));

  bigtable::RowReader reader(
      client_, "", bigtable::RowSet(), bigtable::RowReader::NO_ROWS_LIMIT,
      bigtable::Filter::PassAllFilter(), std::move(retry_policy_),
      std::move(backoff_policy_), std::move(parser_));

  EXPECT_EQ(reader.begin(), reader.end());
}

TEST_F(RowReaderTest, ReadOneRow) {
  parser_->SetRows({"r1"});
  {
    testing::InSequence s;
    EXPECT_CALL(*bigtable_stub_, ReadRowsRaw(_, _)).WillOnce(Return(stream_));
    EXPECT_CALL(*stream_, Read(_)).WillOnce(Return(true));
    EXPECT_CALL(*stream_, Read(_)).WillOnce(Return(false));
  }

  bigtable::RowReader reader(
      client_, "", bigtable::RowSet(), bigtable::RowReader::NO_ROWS_LIMIT,
      bigtable::Filter::PassAllFilter(), std::move(retry_policy_),
      std::move(backoff_policy_), std::move(parser_));

  auto it = reader.begin();
  EXPECT_NE(it, reader.end());
  EXPECT_EQ(it->row_key(), "r1");
  EXPECT_EQ(++it, reader.end());
}

TEST_F(RowReaderTest, FailedStreamIsRetried) {
  parser_->SetRows({"r1"});
  {
    testing::InSequence s;
    EXPECT_CALL(*bigtable_stub_, ReadRowsRaw(_, _)).WillOnce(Return(stream_));
    EXPECT_CALL(*stream_, Read(_)).WillOnce(Return(false));
    EXPECT_CALL(*stream_, Finish())
        .WillOnce(Return(grpc::Status(grpc::INTERNAL, "retry")));

    EXPECT_CALL(*retry_policy_, on_failure_impl(_)).WillOnce(Return(true));

    auto stream_retry = new MockResponseStream();  // the stub will free it
    EXPECT_CALL(*bigtable_stub_, ReadRowsRaw(_, _))
        .WillOnce(Return(stream_retry));
    EXPECT_CALL(*stream_retry, Read(_)).WillOnce(Return(true));
    EXPECT_CALL(*stream_retry, Read(_)).WillOnce(Return(false));
  }

  bigtable::RowReader reader(
      client_, "", bigtable::RowSet(), bigtable::RowReader::NO_ROWS_LIMIT,
      bigtable::Filter::PassAllFilter(), std::move(retry_policy_),
      std::move(backoff_policy_), std::move(parser_));

  auto it = reader.begin();
  EXPECT_NE(it, reader.end());
  EXPECT_EQ(it->row_key(), "r1");
  EXPECT_EQ(++it, reader.end());
}

TEST_F(RowReaderTest, FailedStreamWithNoRetryThrows) {
  {
    testing::InSequence s;
    EXPECT_CALL(*bigtable_stub_, ReadRowsRaw(_, _)).WillOnce(Return(stream_));
    EXPECT_CALL(*stream_, Read(_)).WillOnce(Return(false));
    EXPECT_CALL(*stream_, Finish())
        .WillOnce(Return(grpc::Status(grpc::INTERNAL, "retry")));

    EXPECT_CALL(*retry_policy_, on_failure_impl(_)).WillOnce(Return(false));
  }

  bigtable::RowReader reader(
      client_, "", bigtable::RowSet(), bigtable::RowReader::NO_ROWS_LIMIT,
      bigtable::Filter::PassAllFilter(), std::move(retry_policy_),
      std::move(backoff_policy_), std::move(parser_));

  EXPECT_THROW(reader.begin(), std::exception);
}

TEST_F(RowReaderTest, FailedStreamRetriesSkipAlreadyReadRows) {
  parser_->SetRows({"r1"});
  {
    testing::InSequence s;
    // For sanity, check we have two rows in the initial request
    EXPECT_CALL(*bigtable_stub_, ReadRowsRaw(_, RequestRowKeysCount(2)))
        .WillOnce(Return(stream_));

    EXPECT_CALL(*stream_, Read(_)).WillOnce(Return(true));
    EXPECT_CALL(*stream_, Read(_)).WillOnce(Return(false));
    EXPECT_CALL(*stream_, Finish())
        .WillOnce(Return(grpc::Status(grpc::INTERNAL, "retry")));

    EXPECT_CALL(*retry_policy_, on_failure_impl(_)).WillOnce(Return(true));

    auto stream_retry = new MockResponseStream();  // the stub will free it
    // First row should be removed from the retried request, leaving one row
    EXPECT_CALL(*bigtable_stub_, ReadRowsRaw(_, RequestRowKeysCount(1)))
        .WillOnce(Return(stream_retry));
    EXPECT_CALL(*stream_retry, Read(_)).WillOnce(Return(false));
  }

  bigtable::RowReader reader(
      client_, "", bigtable::RowSet("r1", "r2"),
      bigtable::RowReader::NO_ROWS_LIMIT, bigtable::Filter::PassAllFilter(),
      std::move(retry_policy_), std::move(backoff_policy_), std::move(parser_));

  auto it = reader.begin();
  EXPECT_NE(it, reader.end());
  EXPECT_EQ(it->row_key(), "r1");
  EXPECT_EQ(++it, reader.end());
}
