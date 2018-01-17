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

#include <deque>
#include <initializer_list>

using testing::DoAll;
using testing::Eq;
using testing::Matcher;
using testing::Property;
using testing::Return;
using testing::SetArgPointee;
using testing::Throw;
using testing::_;

using google::bigtable::v2::ReadRowsRequest;
using google::bigtable::v2::ReadRowsResponse;
using google::bigtable::v2::ReadRowsResponse_CellChunk;

class MockResponseStream
    : public grpc::ClientReaderInterface<ReadRowsResponse> {
 public:
  MOCK_METHOD0(WaitForInitialMetadata, void());
  MOCK_METHOD0(Finish, grpc::Status());
  MOCK_METHOD1(NextMessageSize, bool(std::uint32_t*));
  MOCK_METHOD1(Read, bool(ReadRowsResponse*));
};

using bigtable::Row;

namespace {
class ReadRowsParserMock : public bigtable::internal::ReadRowsParser {
 public:
  MOCK_METHOD1(HandleChunkHook, void(ReadRowsResponse_CellChunk chunk));
  void HandleChunk(ReadRowsResponse_CellChunk chunk) override {
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

// Returns a preconfigured set of parsers, so expectations can be set on each.
class ReadRowsParserMockFactory
    : public bigtable::internal::ReadRowsParserFactory {
  using ParserPtr = std::unique_ptr<bigtable::internal::ReadRowsParser>;

 public:
  void AddParser(ParserPtr parser) { parsers_.emplace_back(std::move(parser)); }

  ParserPtr Create() override {
    if (parsers_.empty()) {
      return absl::make_unique<bigtable::internal::ReadRowsParser>();
    }
    ParserPtr parser = std::move(parsers_.front());
    parsers_.pop_front();
    return parser;
  }

 private:
  std::deque<ParserPtr> parsers_;
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
Matcher<const ReadRowsRequest&> RequestWithRowKeysCount(int n) {
  return Property(
      &ReadRowsRequest::rows,
      Property(&google::bigtable::v2::RowSet::row_keys_size, Eq(n)));
}

// Match the row limit in a request
Matcher<const ReadRowsRequest&> RequestWithRowsLimit(std::int64_t n) {
  return Property(&ReadRowsRequest::rows_limit, Eq(n));
}

}  // anonymous namespace

class RowReaderTest : public bigtable::testing::TableTestFixture {
 public:
  RowReaderTest()
      : stream_(new MockResponseStream()),
        retry_policy_(absl::make_unique<RetryPolicyMock>()),
        backoff_policy_(absl::make_unique<BackoffPolicyMock>()),
        parser_factory_(absl::make_unique<ReadRowsParserMockFactory>()) {}

  // must be a new pointer, it is wrapped in unique_ptr by ReadRows
  MockResponseStream* stream_;

  std::unique_ptr<RetryPolicyMock> retry_policy_;
  std::unique_ptr<BackoffPolicyMock> backoff_policy_;

  std::unique_ptr<ReadRowsParserMockFactory> parser_factory_;
};

TEST_F(RowReaderTest, EmptyReaderHasNoRows) {
  EXPECT_CALL(*bigtable_stub_, ReadRowsRaw(_, _)).WillOnce(Return(stream_));
  EXPECT_CALL(*stream_, Read(_)).WillOnce(Return(false));
  EXPECT_CALL(*stream_, Finish()).WillOnce(Return(grpc::Status::OK));

  bigtable::RowReader reader(
      client_, "", bigtable::RowSet(), bigtable::RowReader::NO_ROWS_LIMIT,
      bigtable::Filter::PassAllFilter(), std::move(retry_policy_),
      std::move(backoff_policy_), std::move(parser_factory_));

  EXPECT_EQ(reader.begin(), reader.end());
}

TEST_F(RowReaderTest, ReadOneRow) {
  auto parser = absl::make_unique<ReadRowsParserMock>();
  parser->SetRows({"r1"});
  EXPECT_CALL(*parser, HandleEndOfStreamHook()).Times(1);
  {
    testing::InSequence s;
    EXPECT_CALL(*bigtable_stub_, ReadRowsRaw(_, _)).WillOnce(Return(stream_));
    EXPECT_CALL(*stream_, Read(_)).WillOnce(Return(true));
    EXPECT_CALL(*stream_, Read(_)).WillOnce(Return(false));
    EXPECT_CALL(*stream_, Finish()).WillOnce(Return(grpc::Status::OK));
  }

  parser_factory_->AddParser(std::move(parser));
  bigtable::RowReader reader(
      client_, "", bigtable::RowSet(), bigtable::RowReader::NO_ROWS_LIMIT,
      bigtable::Filter::PassAllFilter(), std::move(retry_policy_),
      std::move(backoff_policy_), std::move(parser_factory_));

  auto it = reader.begin();
  EXPECT_NE(it, reader.end());
  EXPECT_EQ(it->row_key(), "r1");
  EXPECT_EQ(++it, reader.end());
}

TEST_F(RowReaderTest, ReadOneRowIteratorPostincrement) {
  auto parser = absl::make_unique<ReadRowsParserMock>();
  parser->SetRows({"r1"});
  EXPECT_CALL(*parser, HandleEndOfStreamHook()).Times(1);
  {
    testing::InSequence s;
    EXPECT_CALL(*bigtable_stub_, ReadRowsRaw(_, _)).WillOnce(Return(stream_));
    EXPECT_CALL(*stream_, Read(_)).WillOnce(Return(true));
    EXPECT_CALL(*stream_, Read(_)).WillOnce(Return(false));
    EXPECT_CALL(*stream_, Finish()).WillOnce(Return(grpc::Status::OK));
  }

  parser_factory_->AddParser(std::move(parser));
  bigtable::RowReader reader(
      client_, "", bigtable::RowSet(), bigtable::RowReader::NO_ROWS_LIMIT,
      bigtable::Filter::PassAllFilter(), std::move(retry_policy_),
      std::move(backoff_policy_), std::move(parser_factory_));

  auto it = reader.begin();
  EXPECT_NE(it, reader.end());
  // This postincrement is what we are testing
  EXPECT_EQ((it++)->row_key(), "r1");
  EXPECT_EQ(it, reader.end());
}

TEST_F(RowReaderTest, ReadOneOfTwoRowsClosesStream) {
  auto parser = absl::make_unique<ReadRowsParserMock>();
  parser->SetRows({"r1"});
  {
    testing::InSequence s;
    EXPECT_CALL(*bigtable_stub_, ReadRowsRaw(_, _)).WillOnce(Return(stream_));
    EXPECT_CALL(*stream_, Read(_)).WillOnce(Return(true));
    EXPECT_CALL(*stream_, Read(_)).WillOnce(Return(true));
    EXPECT_CALL(*stream_, Read(_)).WillOnce(Return(true));
    EXPECT_CALL(*stream_, Read(_)).WillOnce(Return(false));
    EXPECT_CALL(*stream_, Finish()).WillOnce(Return(grpc::Status::OK));
  }

  parser_factory_->AddParser(std::move(parser));
  bigtable::RowReader reader(
      client_, "", bigtable::RowSet(), bigtable::RowReader::NO_ROWS_LIMIT,
      bigtable::Filter::PassAllFilter(), std::move(retry_policy_),
      std::move(backoff_policy_), std::move(parser_factory_));

  auto it = reader.begin();
  EXPECT_NE(it, reader.end());
  EXPECT_EQ(it->row_key(), "r1");
  EXPECT_NE(it, reader.end());
  // Do not finish iterating, and expect the stream to be finalized anyway.
}

TEST_F(RowReaderTest, FailedStreamIsRetried) {
  auto parser = absl::make_unique<ReadRowsParserMock>();
  parser->SetRows({"r1"});
  {
    testing::InSequence s;
    EXPECT_CALL(*bigtable_stub_, ReadRowsRaw(_, _)).WillOnce(Return(stream_));
    EXPECT_CALL(*stream_, Read(_)).WillOnce(Return(false));
    EXPECT_CALL(*stream_, Finish())
        .WillOnce(Return(grpc::Status(grpc::INTERNAL, "retry")));

    EXPECT_CALL(*retry_policy_, on_failure_impl(_)).WillOnce(Return(true));
    EXPECT_CALL(*backoff_policy_, on_completion_impl(_))
        .WillOnce(Return(std::chrono::milliseconds(0)));

    auto stream_retry = new MockResponseStream();  // the stub will free it
    EXPECT_CALL(*bigtable_stub_, ReadRowsRaw(_, _))
        .WillOnce(Return(stream_retry));
    EXPECT_CALL(*stream_retry, Read(_)).WillOnce(Return(true));
    EXPECT_CALL(*stream_retry, Read(_)).WillOnce(Return(false));
    EXPECT_CALL(*stream_retry, Finish()).WillOnce(Return(grpc::Status::OK));
  }

  parser_factory_->AddParser(std::move(parser));
  bigtable::RowReader reader(
      client_, "", bigtable::RowSet(), bigtable::RowReader::NO_ROWS_LIMIT,
      bigtable::Filter::PassAllFilter(), std::move(retry_policy_),
      std::move(backoff_policy_), std::move(parser_factory_));

  auto it = reader.begin();
  EXPECT_NE(it, reader.end());
  EXPECT_EQ(it->row_key(), "r1");
  EXPECT_EQ(++it, reader.end());
}

TEST_F(RowReaderTest, FailedStreamWithNoRetryThrows) {
  auto parser = absl::make_unique<ReadRowsParserMock>();
  {
    testing::InSequence s;
    EXPECT_CALL(*bigtable_stub_, ReadRowsRaw(_, _)).WillOnce(Return(stream_));
    EXPECT_CALL(*stream_, Read(_)).WillOnce(Return(false));
    EXPECT_CALL(*stream_, Finish())
        .WillOnce(Return(grpc::Status(grpc::INTERNAL, "retry")));

    EXPECT_CALL(*retry_policy_, on_failure_impl(_)).WillOnce(Return(false));
    EXPECT_CALL(*backoff_policy_, on_completion_impl(_)).Times(0);
  }

  parser_factory_->AddParser(std::move(parser));
  bigtable::RowReader reader(
      client_, "", bigtable::RowSet(), bigtable::RowReader::NO_ROWS_LIMIT,
      bigtable::Filter::PassAllFilter(), std::move(retry_policy_),
      std::move(backoff_policy_), std::move(parser_factory_));

  EXPECT_THROW(reader.begin(), std::exception);
}

TEST_F(RowReaderTest, FailedStreamRetriesSkipAlreadyReadRows) {
  auto parser = absl::make_unique<ReadRowsParserMock>();
  parser->SetRows({"r1"});
  {
    testing::InSequence s;
    // For sanity, check we have two rows in the initial request
    EXPECT_CALL(*bigtable_stub_, ReadRowsRaw(_, RequestWithRowKeysCount(2)))
        .WillOnce(Return(stream_));

    EXPECT_CALL(*stream_, Read(_)).WillOnce(Return(true));
    EXPECT_CALL(*stream_, Read(_)).WillOnce(Return(false));
    EXPECT_CALL(*stream_, Finish())
        .WillOnce(Return(grpc::Status(grpc::INTERNAL, "retry")));

    EXPECT_CALL(*retry_policy_, on_failure_impl(_)).WillOnce(Return(true));
    EXPECT_CALL(*backoff_policy_, on_completion_impl(_))
        .WillOnce(Return(std::chrono::milliseconds(0)));

    auto stream_retry = new MockResponseStream();  // the stub will free it
    // First row should be removed from the retried request, leaving one row
    EXPECT_CALL(*bigtable_stub_, ReadRowsRaw(_, RequestWithRowKeysCount(1)))
        .WillOnce(Return(stream_retry));
    EXPECT_CALL(*stream_retry, Read(_)).WillOnce(Return(false));
    EXPECT_CALL(*stream_retry, Finish()).WillOnce(Return(grpc::Status::OK));
  }

  parser_factory_->AddParser(std::move(parser));
  bigtable::RowReader reader(
      client_, "", bigtable::RowSet("r1", "r2"),
      bigtable::RowReader::NO_ROWS_LIMIT, bigtable::Filter::PassAllFilter(),
      std::move(retry_policy_), std::move(backoff_policy_),
      std::move(parser_factory_));

  auto it = reader.begin();
  EXPECT_NE(it, reader.end());
  EXPECT_EQ(it->row_key(), "r1");
  EXPECT_EQ(++it, reader.end());
}

TEST_F(RowReaderTest, FailedParseIsRetried) {
  auto parser = absl::make_unique<ReadRowsParserMock>();
  parser->SetRows({"r1"});
  auto response = bigtable::testing::ReadRowsResponseFromString("chunks {}");
  {
    testing::InSequence s;
    EXPECT_CALL(*bigtable_stub_, ReadRowsRaw(_, _)).WillOnce(Return(stream_));
    EXPECT_CALL(*stream_, Read(_))
        .WillOnce(DoAll(SetArgPointee<0>(response), Return(true)));
    EXPECT_CALL(*parser, HandleChunkHook(_))
        .WillOnce(Throw(std::runtime_error("parser exception")));

    EXPECT_CALL(*retry_policy_, on_failure_impl(_)).WillOnce(Return(true));
    EXPECT_CALL(*backoff_policy_, on_completion_impl(_))
        .WillOnce(Return(std::chrono::milliseconds(0)));

    auto stream_retry = new MockResponseStream();  // the stub will free it
    EXPECT_CALL(*bigtable_stub_, ReadRowsRaw(_, _))
        .WillOnce(Return(stream_retry));
    EXPECT_CALL(*stream_retry, Read(_)).WillOnce(Return(true));
    EXPECT_CALL(*stream_retry, Read(_)).WillOnce(Return(false));
    EXPECT_CALL(*stream_retry, Finish()).WillOnce(Return(grpc::Status::OK));
  }

  parser_factory_->AddParser(std::move(parser));
  bigtable::RowReader reader(
      client_, "", bigtable::RowSet(), bigtable::RowReader::NO_ROWS_LIMIT,
      bigtable::Filter::PassAllFilter(), std::move(retry_policy_),
      std::move(backoff_policy_), std::move(parser_factory_));

  auto it = reader.begin();
  EXPECT_NE(it, reader.end());
  EXPECT_EQ(it->row_key(), "r1");
  EXPECT_EQ(++it, reader.end());
}

TEST_F(RowReaderTest, FailedParseWithNoRetryThrows) {
  auto parser = absl::make_unique<ReadRowsParserMock>();
  {
    testing::InSequence s;
    EXPECT_CALL(*bigtable_stub_, ReadRowsRaw(_, _)).WillOnce(Return(stream_));
    EXPECT_CALL(*stream_, Read(_)).WillOnce(Return(false));
    EXPECT_CALL(*stream_, Finish()).WillOnce(Return(grpc::Status::OK));
    EXPECT_CALL(*parser, HandleEndOfStreamHook())
        .WillOnce(Throw(std::runtime_error("parser exception")));

    EXPECT_CALL(*retry_policy_, on_failure_impl(_)).WillOnce(Return(false));
    EXPECT_CALL(*backoff_policy_, on_completion_impl(_)).Times(0);
  }

  parser_factory_->AddParser(std::move(parser));
  bigtable::RowReader reader(
      client_, "", bigtable::RowSet(), bigtable::RowReader::NO_ROWS_LIMIT,
      bigtable::Filter::PassAllFilter(), std::move(retry_policy_),
      std::move(backoff_policy_), std::move(parser_factory_));

  EXPECT_THROW(reader.begin(), std::exception);
}

TEST_F(RowReaderTest, FailedParseRetriesSkipAlreadyReadRows) {
  auto parser = absl::make_unique<ReadRowsParserMock>();
  parser->SetRows({"r1"});
  {
    testing::InSequence s;
    // For sanity, check we have two rows in the initial request
    EXPECT_CALL(*bigtable_stub_, ReadRowsRaw(_, RequestWithRowKeysCount(2)))
        .WillOnce(Return(stream_));

    EXPECT_CALL(*stream_, Read(_)).WillOnce(Return(true));
    EXPECT_CALL(*stream_, Read(_)).WillOnce(Return(false));
    EXPECT_CALL(*stream_, Finish()).WillOnce(Return(grpc::Status::OK));
    EXPECT_CALL(*parser, HandleEndOfStreamHook())
        .WillOnce(Throw(std::runtime_error("parser exception")));

    EXPECT_CALL(*retry_policy_, on_failure_impl(_)).WillOnce(Return(true));
    EXPECT_CALL(*backoff_policy_, on_completion_impl(_))
        .WillOnce(Return(std::chrono::milliseconds(0)));

    auto stream_retry = new MockResponseStream();  // the stub will free it
    // First row should be removed from the retried request, leaving one row
    EXPECT_CALL(*bigtable_stub_, ReadRowsRaw(_, RequestWithRowKeysCount(1)))
        .WillOnce(Return(stream_retry));
    EXPECT_CALL(*stream_retry, Read(_)).WillOnce(Return(false));
    EXPECT_CALL(*stream_retry, Finish()).WillOnce(Return(grpc::Status::OK));
  }

  parser_factory_->AddParser(std::move(parser));
  bigtable::RowReader reader(
      client_, "", bigtable::RowSet("r1", "r2"),
      bigtable::RowReader::NO_ROWS_LIMIT, bigtable::Filter::PassAllFilter(),
      std::move(retry_policy_), std::move(backoff_policy_),
      std::move(parser_factory_));

  auto it = reader.begin();
  EXPECT_NE(it, reader.end());
  EXPECT_EQ(it->row_key(), "r1");
  EXPECT_EQ(++it, reader.end());
}

TEST_F(RowReaderTest, FailedStreamWithAllRequiedRowsSeenShouldNotRetry) {
  auto parser = absl::make_unique<ReadRowsParserMock>();
  parser->SetRows({"r2"});
  {
    testing::InSequence s;
    EXPECT_CALL(*bigtable_stub_, ReadRowsRaw(_, _)).WillOnce(Return(stream_));

    EXPECT_CALL(*stream_, Read(_)).WillOnce(Return(true));
    EXPECT_CALL(*stream_, Read(_)).WillOnce(Return(false));
    EXPECT_CALL(*stream_, Finish())
        .WillOnce(Return(
            grpc::Status(grpc::INTERNAL, "this exception must be ignored")));

    // Note there is no expectation of a new connection, because the
    // set of rows to read should become empty after reading "r2" and
    // intersecting the requested ["r1", "r2"] with ("r2", "") for the
    // retry.
  }

  parser_factory_->AddParser(std::move(parser));
  bigtable::RowReader reader(
      client_, "", bigtable::RowSet(bigtable::RowRange::Closed("r1", "r2")),
      bigtable::RowReader::NO_ROWS_LIMIT, bigtable::Filter::PassAllFilter(),
      std::move(retry_policy_), std::move(backoff_policy_),
      std::move(parser_factory_));

  auto it = reader.begin();
  EXPECT_NE(it, reader.end());
  EXPECT_EQ(it->row_key(), "r2");
  EXPECT_EQ(++it, reader.end());
}

TEST_F(RowReaderTest, RowLimitIsSent) {
  EXPECT_CALL(*bigtable_stub_, ReadRowsRaw(_, RequestWithRowsLimit(442)))
      .WillOnce(Return(stream_));
  EXPECT_CALL(*stream_, Read(_)).WillOnce(Return(false));
  EXPECT_CALL(*stream_, Finish()).WillOnce(Return(grpc::Status::OK));

  bigtable::RowReader reader(
      client_, "", bigtable::RowSet(), 442, bigtable::Filter::PassAllFilter(),
      std::move(retry_policy_), std::move(backoff_policy_),
      std::move(parser_factory_));

  auto it = reader.begin();
  EXPECT_EQ(it, reader.end());
}

TEST_F(RowReaderTest, RowLimitIsDecreasedOnRetry) {
  auto parser = absl::make_unique<ReadRowsParserMock>();
  parser->SetRows({"r1"});
  {
    testing::InSequence s;
    EXPECT_CALL(*bigtable_stub_, ReadRowsRaw(_, RequestWithRowsLimit(42)))
        .WillOnce(Return(stream_));

    EXPECT_CALL(*stream_, Read(_)).WillOnce(Return(true));
    EXPECT_CALL(*stream_, Read(_)).WillOnce(Return(false));
    EXPECT_CALL(*stream_, Finish())
        .WillOnce(Return(grpc::Status(grpc::INTERNAL, "retry")));

    EXPECT_CALL(*retry_policy_, on_failure_impl(_)).WillOnce(Return(true));
    EXPECT_CALL(*backoff_policy_, on_completion_impl(_))
        .WillOnce(Return(std::chrono::milliseconds(0)));

    auto stream_retry = new MockResponseStream();  // the stub will free it
    // 41 instead of 42
    EXPECT_CALL(*bigtable_stub_, ReadRowsRaw(_, RequestWithRowsLimit(41)))
        .WillOnce(Return(stream_retry));
    EXPECT_CALL(*stream_retry, Read(_)).WillOnce(Return(false));
    EXPECT_CALL(*stream_retry, Finish()).WillOnce(Return(grpc::Status::OK));
  }

  parser_factory_->AddParser(std::move(parser));
  bigtable::RowReader reader(
      client_, "", bigtable::RowSet(), 42, bigtable::Filter::PassAllFilter(),
      std::move(retry_policy_), std::move(backoff_policy_),
      std::move(parser_factory_));

  auto it = reader.begin();
  EXPECT_NE(it, reader.end());
  EXPECT_EQ(it->row_key(), "r1");
  EXPECT_EQ(++it, reader.end());
}

TEST_F(RowReaderTest, RowLimitIsNotDecreasedToZero) {
  auto parser = absl::make_unique<ReadRowsParserMock>();
  parser->SetRows({"r1"});
  {
    testing::InSequence s;
    EXPECT_CALL(*bigtable_stub_, ReadRowsRaw(_, RequestWithRowsLimit(1)))
        .WillOnce(Return(stream_));

    EXPECT_CALL(*stream_, Read(_)).WillOnce(Return(true));
    EXPECT_CALL(*stream_, Read(_)).WillOnce(Return(false));
    EXPECT_CALL(*stream_, Finish())
        .WillOnce(Return(
            grpc::Status(grpc::INTERNAL, "this exception must be ignored")));

    // Note there is no expectation of a new connection, because the
    // row limit reaches zero.
  }

  parser_factory_->AddParser(std::move(parser));
  bigtable::RowReader reader(
      client_, "", bigtable::RowSet(), 1, bigtable::Filter::PassAllFilter(),
      std::move(retry_policy_), std::move(backoff_policy_),
      std::move(parser_factory_));

  auto it = reader.begin();
  EXPECT_NE(it, reader.end());
  EXPECT_EQ(it->row_key(), "r1");
  EXPECT_EQ(++it, reader.end());
}

TEST_F(RowReaderTest, BeginThrowsAfterCancelClosesStream) {
  auto parser = absl::make_unique<ReadRowsParserMock>();
  parser->SetRows({"r1"});
  {
    testing::InSequence s;
    EXPECT_CALL(*bigtable_stub_, ReadRowsRaw(_, _)).WillOnce(Return(stream_));
    EXPECT_CALL(*stream_, Read(_)).WillOnce(Return(true));
    EXPECT_CALL(*stream_, Read(_)).WillOnce(Return(true));
    EXPECT_CALL(*stream_, Read(_)).WillOnce(Return(true));
    EXPECT_CALL(*stream_, Read(_)).WillOnce(Return(false));
    EXPECT_CALL(*stream_, Finish()).WillOnce(Return(grpc::Status::OK));
  }

  parser_factory_->AddParser(std::move(parser));
  bigtable::RowReader reader(
      client_, "", bigtable::RowSet(), bigtable::RowReader::NO_ROWS_LIMIT,
      bigtable::Filter::PassAllFilter(), std::move(retry_policy_),
      std::move(backoff_policy_), std::move(parser_factory_));

  auto it = reader.begin();
  EXPECT_NE(it, reader.end());
  EXPECT_EQ(it->row_key(), "r1");
  EXPECT_NE(it, reader.end());
  // Manually cancel the call.
  reader.Cancel();
  EXPECT_THROW(reader.begin(), std::runtime_error);
}

TEST_F(RowReaderTest, BeginThrowsAfterImmediateCancel) {
  bigtable::RowReader reader(
      client_, "", bigtable::RowSet(), bigtable::RowReader::NO_ROWS_LIMIT,
      bigtable::Filter::PassAllFilter(), std::move(retry_policy_),
      std::move(backoff_policy_), std::move(parser_factory_));
  // Manually cancel the call before a stream was created.
  reader.Cancel();

  EXPECT_THROW(reader.begin(), std::runtime_error);
}
