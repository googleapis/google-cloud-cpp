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

#include "google/cloud/bigtable/row_reader.h"
#include "google/cloud/bigtable/bigtable_strong_types.h"
#include "google/cloud/bigtable/table.h"
#include "google/cloud/bigtable/testing/mock_read_rows_reader.h"
#include "google/cloud/bigtable/testing/table_test_fixture.h"
#include "google/cloud/internal/make_unique.h"
#include "google/cloud/internal/throw_delegate.h"
#include "google/cloud/testing_util/assert_ok.h"
#include "google/cloud/testing_util/capture_log_lines_backend.h"
#include <gmock/gmock.h>
#include <deque>
#include <initializer_list>

using testing::_;
using testing::DoAll;
using testing::Eq;
using testing::Invoke;
using testing::Matcher;
using testing::Property;
using testing::Return;
using testing::SetArgPointee;

using google::bigtable::v2::ReadRowsRequest;
using google::bigtable::v2::ReadRowsResponse;
using google::bigtable::v2::ReadRowsResponse_CellChunk;

namespace bigtable = google::cloud::bigtable;
using bigtable::Row;
using bigtable::testing::MockReadRowsReader;

namespace {
class ReadRowsParserMock : public bigtable::internal::ReadRowsParser {
 public:
  MOCK_METHOD2(HandleChunkHook,
               void(ReadRowsResponse_CellChunk chunk, grpc::Status& status));
  void HandleChunk(ReadRowsResponse_CellChunk chunk,
                   grpc::Status& status) override {
    HandleChunkHook(chunk, status);
  }

  MOCK_METHOD1(HandleEndOfStreamHook, void(grpc::Status& status));
  void HandleEndOfStream(grpc::Status& status) override {
    HandleEndOfStreamHook(status);
  }

  bool HasNext() const override { return !rows_.empty(); }

  Row Next(grpc::Status& status) override {
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

  // We only need a hook here because MOCK_METHOD0 would not add the
  // 'override' keyword that a compiler warning expects for Create().
  MOCK_METHOD0(CreateHook, void());
  ParserPtr Create() override {
    CreateHook();
    if (parsers_.empty()) {
      return ParserPtr(new bigtable::internal::ReadRowsParser);
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
    google::cloud::internal::ThrowRuntimeError("Mocks cannot be copied.");
  }

  MOCK_CONST_METHOD1(SetupHook, void(grpc::ClientContext&));
  void Setup(grpc::ClientContext& context) const override {
    SetupHook(context);
  }

  MOCK_METHOD1(OnFailureHook, bool(grpc::Status const& status));
  bool OnFailure(grpc::Status const& status) override {
    return OnFailureHook(status);
  }
};

class BackoffPolicyMock : public bigtable::RPCBackoffPolicy {
 public:
  BackoffPolicyMock() {}
  std::unique_ptr<RPCBackoffPolicy> clone() const override {
    google::cloud::internal::ThrowRuntimeError("Mocks cannot be copied.");
  }
  void Setup(grpc::ClientContext& context) const override {}
  MOCK_METHOD1(OnCompletionHook,
               std::chrono::milliseconds(grpc::Status const& s));
  std::chrono::milliseconds OnCompletion(grpc::Status const& s) override {
    return OnCompletionHook(s);
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
      : retry_policy_(new RetryPolicyMock),
        backoff_policy_(new BackoffPolicyMock),
        metadata_update_policy_(kTableName,
                                bigtable::MetadataParamTypes::TABLE_NAME),
        parser_factory_(new ReadRowsParserMockFactory) {}

  std::unique_ptr<RetryPolicyMock> retry_policy_;
  std::unique_ptr<BackoffPolicyMock> backoff_policy_;
  bigtable::MetadataUpdatePolicy metadata_update_policy_;
  std::unique_ptr<ReadRowsParserMockFactory> parser_factory_;
};

TEST_F(RowReaderTest, EmptyReaderHasNoRows) {
  auto* stream = new MockReadRowsReader;  // wrapped in unique_ptr by ReadRows
  EXPECT_CALL(*client_, ReadRows(_, _))
      .WillOnce(Invoke(stream->MakeMockReturner()));
  EXPECT_CALL(*stream, Read(_)).WillOnce(Return(false));
  EXPECT_CALL(*stream, Finish()).WillOnce(Return(grpc::Status::OK));

  bigtable::RowReader reader(
      client_, bigtable::TableId(""), bigtable::RowSet(),
      bigtable::RowReader::NO_ROWS_LIMIT, bigtable::Filter::PassAllFilter(),
      std::move(retry_policy_), std::move(backoff_policy_),
      metadata_update_policy_, std::move(parser_factory_));

  EXPECT_EQ(reader.begin(), reader.end());
}

TEST_F(RowReaderTest, ReadOneRow) {
  auto* stream = new MockReadRowsReader;  // wrapped in unique_ptr by ReadRows
  auto parser = google::cloud::internal::make_unique<ReadRowsParserMock>();
  parser->SetRows({"r1"});
  EXPECT_CALL(*parser, HandleEndOfStreamHook(_)).Times(1);
  {
    testing::InSequence s;
    EXPECT_CALL(*client_, ReadRows(_, _))
        .WillOnce(Invoke(stream->MakeMockReturner()));
    EXPECT_CALL(*stream, Read(_)).WillOnce(Return(true));
    EXPECT_CALL(*stream, Read(_)).WillOnce(Return(false));
    EXPECT_CALL(*stream, Finish()).WillOnce(Return(grpc::Status::OK));
  }

  parser_factory_->AddParser(std::move(parser));
  bigtable::RowReader reader(
      client_, bigtable::TableId(""), bigtable::RowSet(),
      bigtable::RowReader::NO_ROWS_LIMIT, bigtable::Filter::PassAllFilter(),
      std::move(retry_policy_), std::move(backoff_policy_),
      metadata_update_policy_, std::move(parser_factory_));

  auto it = reader.begin();
  EXPECT_NE(it, reader.end());
  ASSERT_STATUS_OK(*it);
  EXPECT_EQ((*it)->row_key(), "r1");
  EXPECT_EQ(++it, reader.end());
}

TEST_F(RowReaderTest, ReadOneRow_AppProfileId) {
  using namespace ::testing;
  auto* stream = new MockReadRowsReader;  // wrapped in unique_ptr by ReadRows
  auto parser = google::cloud::internal::make_unique<ReadRowsParserMock>();
  parser->SetRows({"r1"});
  EXPECT_CALL(*parser, HandleEndOfStreamHook(_)).Times(1);
  {
    testing::InSequence s;
    std::string expected_id = "test-id";
    EXPECT_CALL(*client_, ReadRows(_, _))
        .WillOnce(Invoke([expected_id, &stream](grpc::ClientContext* ctx,
                                                ReadRowsRequest req) {
          EXPECT_EQ(expected_id, req.app_profile_id());
          return stream->AsUniqueMocked();
        }));
    EXPECT_CALL(*stream, Read(_)).WillOnce(Return(true));
    EXPECT_CALL(*stream, Read(_)).WillOnce(Return(false));
    EXPECT_CALL(*stream, Finish()).WillOnce(Return(grpc::Status::OK));
  }

  parser_factory_->AddParser(std::move(parser));
  bigtable::AppProfileId app_profile_id("test-id");
  bigtable::RowReader reader(
      client_, app_profile_id, bigtable::TableId(""), bigtable::RowSet(),
      bigtable::RowReader::NO_ROWS_LIMIT, bigtable::Filter::PassAllFilter(),
      std::move(retry_policy_), std::move(backoff_policy_),
      metadata_update_policy_, std::move(parser_factory_));

  auto it = reader.begin();
  EXPECT_NE(it, reader.end());
  ASSERT_STATUS_OK(*it);
  EXPECT_EQ((*it)->row_key(), "r1");
  EXPECT_EQ(++it, reader.end());
}

TEST_F(RowReaderTest, ReadOneRowIteratorPostincrement) {
  auto* stream = new MockReadRowsReader;  // wrapped in unique_ptr by ReadRows
  auto parser = google::cloud::internal::make_unique<ReadRowsParserMock>();
  parser->SetRows({"r1"});
  EXPECT_CALL(*parser, HandleEndOfStreamHook(_)).Times(1);
  {
    testing::InSequence s;
    EXPECT_CALL(*client_, ReadRows(_, _))
        .WillOnce(Invoke(stream->MakeMockReturner()));
    EXPECT_CALL(*stream, Read(_)).WillOnce(Return(true));
    EXPECT_CALL(*stream, Read(_)).WillOnce(Return(false));
    EXPECT_CALL(*stream, Finish()).WillOnce(Return(grpc::Status::OK));
  }

  parser_factory_->AddParser(std::move(parser));
  bigtable::RowReader reader(
      client_, bigtable::TableId(""), bigtable::RowSet(),
      bigtable::RowReader::NO_ROWS_LIMIT, bigtable::Filter::PassAllFilter(),
      std::move(retry_policy_), std::move(backoff_policy_),
      metadata_update_policy_, std::move(parser_factory_));

  auto it = reader.begin();
  EXPECT_NE(it, reader.end());
  // This postincrement is what we are testing
  auto it2 = it++;
  ASSERT_STATUS_OK(*it2);
  EXPECT_EQ((*it2)->row_key(), "r1");
  EXPECT_EQ(it, reader.end());
}

TEST_F(RowReaderTest, ReadOneOfTwoRowsClosesStream) {
  auto* stream = new MockReadRowsReader;  // wrapped in unique_ptr by ReadRows
  auto parser = google::cloud::internal::make_unique<ReadRowsParserMock>();
  parser->SetRows({"r1"});
  {
    testing::InSequence s;
    EXPECT_CALL(*client_, ReadRows(_, _))
        .WillOnce(Invoke(stream->MakeMockReturner()));
    EXPECT_CALL(*stream, Read(_)).WillOnce(Return(true));
    EXPECT_CALL(*stream, Read(_)).WillOnce(Return(true));
    EXPECT_CALL(*stream, Read(_)).WillOnce(Return(true));
    EXPECT_CALL(*stream, Read(_)).WillOnce(Return(false));
    EXPECT_CALL(*stream, Finish()).WillOnce(Return(grpc::Status::OK));
  }

  parser_factory_->AddParser(std::move(parser));
  bigtable::RowReader reader(
      client_, bigtable::TableId(""), bigtable::RowSet(),
      bigtable::RowReader::NO_ROWS_LIMIT, bigtable::Filter::PassAllFilter(),
      std::move(retry_policy_), std::move(backoff_policy_),
      metadata_update_policy_, std::move(parser_factory_));

  auto it = reader.begin();
  EXPECT_NE(it, reader.end());
  ASSERT_STATUS_OK(*it);
  EXPECT_EQ((*it)->row_key(), "r1");
  EXPECT_NE(it, reader.end());
  // Do not finish the iteration.  We still expect the stream to be finalized,
  // and the previously setup expectations on the mock `stream` check that.
}

TEST_F(RowReaderTest, FailedStreamIsRetried) {
  auto* stream = new MockReadRowsReader;  // wrapped in unique_ptr by ReadRows
  auto parser = google::cloud::internal::make_unique<ReadRowsParserMock>();
  parser->SetRows({"r1"});
  {
    testing::InSequence s;
    EXPECT_CALL(*client_, ReadRows(_, _))
        .WillOnce(Invoke(stream->MakeMockReturner()));
    EXPECT_CALL(*stream, Read(_)).WillOnce(Return(false));
    EXPECT_CALL(*stream, Finish())
        .WillOnce(Return(grpc::Status(grpc::StatusCode::INTERNAL, "retry")));

    EXPECT_CALL(*retry_policy_, OnFailureHook(_)).WillOnce(Return(true));
    EXPECT_CALL(*backoff_policy_, OnCompletionHook(_))
        .WillOnce(Return(std::chrono::milliseconds(0)));

    auto stream_retry = new MockReadRowsReader;  // the stub will free it
    EXPECT_CALL(*client_, ReadRows(_, _))
        .WillOnce(Invoke(stream_retry->MakeMockReturner()));
    EXPECT_CALL(*stream_retry, Read(_)).WillOnce(Return(true));
    EXPECT_CALL(*stream_retry, Read(_)).WillOnce(Return(false));
    EXPECT_CALL(*stream_retry, Finish()).WillOnce(Return(grpc::Status::OK));
  }

  parser_factory_->AddParser(std::move(parser));
  bigtable::RowReader reader(
      client_, bigtable::TableId(""), bigtable::RowSet(),
      bigtable::RowReader::NO_ROWS_LIMIT, bigtable::Filter::PassAllFilter(),
      std::move(retry_policy_), std::move(backoff_policy_),
      metadata_update_policy_, std::move(parser_factory_));

  auto it = reader.begin();
  EXPECT_NE(it, reader.end());
  ASSERT_STATUS_OK(*it);
  EXPECT_EQ((*it)->row_key(), "r1");
  EXPECT_EQ(++it, reader.end());
}

TEST_F(RowReaderTest, FailedStreamWithNoRetryThrowsNoExcept) {
  auto* stream = new MockReadRowsReader;  // wrapped in unique_ptr by ReadRows
  auto parser = google::cloud::internal::make_unique<ReadRowsParserMock>();
  {
    testing::InSequence s;
    EXPECT_CALL(*client_, ReadRows(_, _))
        .WillOnce(Invoke(stream->MakeMockReturner()));
    EXPECT_CALL(*stream, Read(_)).WillOnce(Return(false));
    EXPECT_CALL(*stream, Finish())
        .WillOnce(Return(grpc::Status(grpc::StatusCode::INTERNAL, "retry")));

    EXPECT_CALL(*retry_policy_, OnFailureHook(_)).WillOnce(Return(false));
    EXPECT_CALL(*backoff_policy_, OnCompletionHook(_)).Times(0);
  }

  parser_factory_->AddParser(std::move(parser));
  bigtable::RowReader reader(
      client_, bigtable::TableId(""), bigtable::RowSet(),
      bigtable::RowReader::NO_ROWS_LIMIT, bigtable::Filter::PassAllFilter(),
      std::move(retry_policy_), std::move(backoff_policy_),
      metadata_update_policy_, std::move(parser_factory_));

  auto it = reader.begin();
  EXPECT_NE(it, reader.end());
  EXPECT_FALSE(*it);
}

TEST_F(RowReaderTest, FailedStreamRetriesSkipAlreadyReadRows) {
  auto* stream = new MockReadRowsReader;  // wrapped in unique_ptr by ReadRows
  auto parser = google::cloud::internal::make_unique<ReadRowsParserMock>();
  parser->SetRows({"r1"});
  {
    testing::InSequence s;
    // For sanity, check we have two rows in the initial request
    EXPECT_CALL(*client_, ReadRows(_, RequestWithRowKeysCount(2)))
        .WillOnce(Invoke(stream->MakeMockReturner()));

    EXPECT_CALL(*stream, Read(_)).WillOnce(Return(true));
    EXPECT_CALL(*stream, Read(_)).WillOnce(Return(false));
    EXPECT_CALL(*stream, Finish())
        .WillOnce(Return(grpc::Status(grpc::StatusCode::INTERNAL, "retry")));

    EXPECT_CALL(*retry_policy_, OnFailureHook(_)).WillOnce(Return(true));
    EXPECT_CALL(*backoff_policy_, OnCompletionHook(_))
        .WillOnce(Return(std::chrono::milliseconds(0)));

    auto stream_retry = new MockReadRowsReader;  // the stub will free it
    // First row should be removed from the retried request, leaving one row
    EXPECT_CALL(*client_, ReadRows(_, RequestWithRowKeysCount(1)))
        .WillOnce(Invoke(stream_retry->MakeMockReturner()));
    EXPECT_CALL(*stream_retry, Read(_)).WillOnce(Return(false));
    EXPECT_CALL(*stream_retry, Finish()).WillOnce(Return(grpc::Status::OK));
  }

  parser_factory_->AddParser(std::move(parser));
  bigtable::RowReader reader(
      client_, bigtable::TableId(""), bigtable::RowSet("r1", "r2"),
      bigtable::RowReader::NO_ROWS_LIMIT, bigtable::Filter::PassAllFilter(),
      std::move(retry_policy_), std::move(backoff_policy_),
      metadata_update_policy_, std::move(parser_factory_));

  auto it = reader.begin();
  EXPECT_NE(it, reader.end());
  ASSERT_STATUS_OK(*it);
  EXPECT_EQ((*it)->row_key(), "r1");
  EXPECT_EQ(++it, reader.end());
}

TEST_F(RowReaderTest, FailedParseIsRetried) {
  auto* stream = new MockReadRowsReader;  // wrapped in unique_ptr by ReadRows
  auto parser = google::cloud::internal::make_unique<ReadRowsParserMock>();
  parser->SetRows({"r1"});
  auto response = bigtable::testing::ReadRowsResponseFromString("chunks {}");
  {
    testing::InSequence s;
    EXPECT_CALL(*client_, ReadRows(_, _))
        .WillOnce(Invoke(stream->MakeMockReturner()));
    EXPECT_CALL(*stream, Read(_))
        .WillOnce(DoAll(SetArgPointee<0>(response), Return(true)));
    EXPECT_CALL(*parser, HandleChunkHook(_, _))
        .WillOnce(testing::SetArgReferee<1>(
            grpc::Status(grpc::StatusCode::INTERNAL, "parser exception")));

    EXPECT_CALL(*retry_policy_, OnFailureHook(_)).WillOnce(Return(true));
    EXPECT_CALL(*backoff_policy_, OnCompletionHook(_))
        .WillOnce(Return(std::chrono::milliseconds(0)));

    auto stream_retry = new MockReadRowsReader;  // the stub will free it
    EXPECT_CALL(*client_, ReadRows(_, _))
        .WillOnce(Invoke(stream_retry->MakeMockReturner()));
    EXPECT_CALL(*stream_retry, Read(_)).WillOnce(Return(true));
    EXPECT_CALL(*stream_retry, Read(_)).WillOnce(Return(false));
    EXPECT_CALL(*stream_retry, Finish()).WillOnce(Return(grpc::Status::OK));
  }

  parser_factory_->AddParser(std::move(parser));
  bigtable::RowReader reader(
      client_, bigtable::TableId(""), bigtable::RowSet(),
      bigtable::RowReader::NO_ROWS_LIMIT, bigtable::Filter::PassAllFilter(),
      std::move(retry_policy_), std::move(backoff_policy_),
      metadata_update_policy_, std::move(parser_factory_));

  auto it = reader.begin();
  EXPECT_NE(it, reader.end());
  ASSERT_STATUS_OK(*it);
  EXPECT_EQ((*it)->row_key(), "r1");
  EXPECT_EQ(++it, reader.end());
}

TEST_F(RowReaderTest, FailedParseRetriesSkipAlreadyReadRows) {
  auto* stream = new MockReadRowsReader;  // wrapped in unique_ptr by ReadRows
  auto parser = google::cloud::internal::make_unique<ReadRowsParserMock>();
  parser->SetRows({"r1"});
  {
    testing::InSequence s;
    // For sanity, check we have two rows in the initial request
    EXPECT_CALL(*client_, ReadRows(_, RequestWithRowKeysCount(2)))
        .WillOnce(Invoke(stream->MakeMockReturner()));

    EXPECT_CALL(*stream, Read(_)).WillOnce(Return(true));
    EXPECT_CALL(*stream, Read(_)).WillOnce(Return(false));
    EXPECT_CALL(*stream, Finish()).WillOnce(Return(grpc::Status::OK));
    grpc::Status status;
    EXPECT_CALL(*parser, HandleEndOfStreamHook(_))
        .WillOnce(testing::SetArgReferee<0>(
            grpc::Status(grpc::StatusCode::INTERNAL, "InternalError")));

    EXPECT_CALL(*retry_policy_, OnFailureHook(_)).WillOnce(Return(true));
    EXPECT_CALL(*backoff_policy_, OnCompletionHook(_))
        .WillOnce(Return(std::chrono::milliseconds(0)));

    auto stream_retry = new MockReadRowsReader;  // the stub will free it
    // First row should be removed from the retried request, leaving one row
    EXPECT_CALL(*client_, ReadRows(_, RequestWithRowKeysCount(1)))
        .WillOnce(Invoke(stream_retry->MakeMockReturner()));
    EXPECT_CALL(*stream_retry, Read(_)).WillOnce(Return(false));
    EXPECT_CALL(*stream_retry, Finish()).WillOnce(Return(grpc::Status::OK));
  }

  parser_factory_->AddParser(std::move(parser));
  bigtable::RowReader reader(
      client_, bigtable::TableId(""), bigtable::RowSet("r1", "r2"),
      bigtable::RowReader::NO_ROWS_LIMIT, bigtable::Filter::PassAllFilter(),
      std::move(retry_policy_), std::move(backoff_policy_),
      metadata_update_policy_, std::move(parser_factory_));

  auto it = reader.begin();
  EXPECT_NE(it, reader.end());
  ASSERT_STATUS_OK(*it);
  EXPECT_EQ((*it)->row_key(), "r1");
  EXPECT_EQ(++it, reader.end());
}

TEST_F(RowReaderTest, FailedParseWithNoRetryThrowsNoExcept) {
  auto* stream = new MockReadRowsReader;  // wrapped in unique_ptr by ReadRows
  auto parser = google::cloud::internal::make_unique<ReadRowsParserMock>();
  {
    testing::InSequence s;

    EXPECT_CALL(*client_, ReadRows(_, _))
        .WillOnce(Invoke(stream->MakeMockReturner()));
    EXPECT_CALL(*stream, Read(_)).WillOnce(Return(false));
    EXPECT_CALL(*stream, Finish()).WillOnce(Return(grpc::Status::OK));
    grpc::Status status;
    EXPECT_CALL(*parser, HandleEndOfStreamHook(_))
        .WillOnce(testing::SetArgReferee<0>(
            grpc::Status(grpc::StatusCode::INTERNAL, "InternalError")));
    EXPECT_CALL(*retry_policy_, OnFailureHook(_)).WillOnce(Return(false));
    EXPECT_CALL(*backoff_policy_, OnCompletionHook(_)).Times(0);
  }

  parser_factory_->AddParser(std::move(parser));
  bigtable::RowReader reader(
      client_, bigtable::TableId(""), bigtable::RowSet(),
      bigtable::RowReader::NO_ROWS_LIMIT, bigtable::Filter::PassAllFilter(),
      std::move(retry_policy_), std::move(backoff_policy_),
      metadata_update_policy_, std::move(parser_factory_));

  auto it = reader.begin();
  EXPECT_NE(it, reader.end());
  EXPECT_FALSE(*it);
}

TEST_F(RowReaderTest, FailedStreamWithAllRequiedRowsSeenShouldNotRetry) {
  auto* stream = new MockReadRowsReader;  // wrapped in unique_ptr by ReadRows
  auto parser = google::cloud::internal::make_unique<ReadRowsParserMock>();
  parser->SetRows({"r2"});
  {
    testing::InSequence s;
    EXPECT_CALL(*client_, ReadRows(_, _))
        .WillOnce(Invoke(stream->MakeMockReturner()));

    EXPECT_CALL(*stream, Read(_)).WillOnce(Return(true));
    EXPECT_CALL(*stream, Read(_)).WillOnce(Return(false));
    EXPECT_CALL(*stream, Finish())
        .WillOnce(Return(grpc::Status(grpc::StatusCode::INTERNAL,
                                      "this exception must be ignored")));

    // Note there is no expectation of a new connection, because the
    // set of rows to read should become empty after reading "r2" and
    // intersecting the requested ["r1", "r2"] with ("r2", "") for the
    // retry.
  }

  parser_factory_->AddParser(std::move(parser));
  bigtable::RowReader reader(
      client_, bigtable::TableId(""),
      bigtable::RowSet(bigtable::RowRange::Closed("r1", "r2")),
      bigtable::RowReader::NO_ROWS_LIMIT, bigtable::Filter::PassAllFilter(),
      std::move(retry_policy_), std::move(backoff_policy_),
      metadata_update_policy_, std::move(parser_factory_));

  auto it = reader.begin();
  EXPECT_NE(it, reader.end());
  ASSERT_STATUS_OK(*it);
  EXPECT_EQ((*it)->row_key(), "r2");
  EXPECT_EQ(++it, reader.end());
}

TEST_F(RowReaderTest, RowLimitIsSent) {
  auto* stream = new MockReadRowsReader;  // wrapped in unique_ptr by ReadRows
  EXPECT_CALL(*client_, ReadRows(_, RequestWithRowsLimit(442)))
      .WillOnce(Invoke(stream->MakeMockReturner()));
  EXPECT_CALL(*stream, Read(_)).WillOnce(Return(false));
  EXPECT_CALL(*stream, Finish()).WillOnce(Return(grpc::Status::OK));

  bigtable::RowReader reader(
      client_, bigtable::TableId(""), bigtable::RowSet(), 442,
      bigtable::Filter::PassAllFilter(), std::move(retry_policy_),
      std::move(backoff_policy_), metadata_update_policy_,
      std::move(parser_factory_));

  auto it = reader.begin();
  EXPECT_EQ(it, reader.end());
}

TEST_F(RowReaderTest, RowLimitIsDecreasedOnRetry) {
  auto* stream = new MockReadRowsReader;  // wrapped in unique_ptr by ReadRows
  auto parser = google::cloud::internal::make_unique<ReadRowsParserMock>();
  parser->SetRows({"r1"});
  {
    testing::InSequence s;
    EXPECT_CALL(*client_, ReadRows(_, RequestWithRowsLimit(42)))
        .WillOnce(Invoke(stream->MakeMockReturner()));

    EXPECT_CALL(*stream, Read(_)).WillOnce(Return(true));
    EXPECT_CALL(*stream, Read(_)).WillOnce(Return(false));
    EXPECT_CALL(*stream, Finish())
        .WillOnce(Return(grpc::Status(grpc::StatusCode::INTERNAL, "retry")));

    EXPECT_CALL(*retry_policy_, OnFailureHook(_)).WillOnce(Return(true));
    EXPECT_CALL(*backoff_policy_, OnCompletionHook(_))
        .WillOnce(Return(std::chrono::milliseconds(0)));

    auto stream_retry = new MockReadRowsReader;  // the stub will free it
    // 41 instead of 42
    EXPECT_CALL(*client_, ReadRows(_, RequestWithRowsLimit(41)))
        .WillOnce(Invoke(stream_retry->MakeMockReturner()));
    EXPECT_CALL(*stream_retry, Read(_)).WillOnce(Return(false));
    EXPECT_CALL(*stream_retry, Finish()).WillOnce(Return(grpc::Status::OK));
  }

  parser_factory_->AddParser(std::move(parser));
  bigtable::RowReader reader(
      client_, bigtable::TableId(""), bigtable::RowSet(), 42,
      bigtable::Filter::PassAllFilter(), std::move(retry_policy_),
      std::move(backoff_policy_), metadata_update_policy_,
      std::move(parser_factory_));

  auto it = reader.begin();
  EXPECT_NE(it, reader.end());
  ASSERT_STATUS_OK(*it);
  EXPECT_EQ((*it)->row_key(), "r1");
  EXPECT_EQ(++it, reader.end());
}

TEST_F(RowReaderTest, RowLimitIsNotDecreasedToZero) {
  auto* stream = new MockReadRowsReader;  // wrapped in unique_ptr by ReadRows
  auto parser = google::cloud::internal::make_unique<ReadRowsParserMock>();
  parser->SetRows({"r1"});
  {
    testing::InSequence s;
    EXPECT_CALL(*client_, ReadRows(_, RequestWithRowsLimit(1)))
        .WillOnce(Invoke(stream->MakeMockReturner()));

    EXPECT_CALL(*stream, Read(_)).WillOnce(Return(true));
    EXPECT_CALL(*stream, Read(_)).WillOnce(Return(false));
    EXPECT_CALL(*stream, Finish())
        .WillOnce(Return(grpc::Status(grpc::StatusCode::INTERNAL,
                                      "this exception must be ignored")));

    // Note there is no expectation of a new connection, because the
    // row limit reaches zero.
  }

  parser_factory_->AddParser(std::move(parser));
  bigtable::RowReader reader(
      client_, bigtable::TableId(""), bigtable::RowSet(), 1,
      bigtable::Filter::PassAllFilter(), std::move(retry_policy_),
      std::move(backoff_policy_), metadata_update_policy_,
      std::move(parser_factory_));

  auto it = reader.begin();
  EXPECT_NE(it, reader.end());
  ASSERT_STATUS_OK(*it);
  EXPECT_EQ((*it)->row_key(), "r1");
  EXPECT_EQ(++it, reader.end());
}

TEST_F(RowReaderTest, BeginThrowsAfterCancelClosesStreamNoExcept) {
  auto parser = google::cloud::internal::make_unique<ReadRowsParserMock>();
  parser->SetRows({"r1"});
  auto* stream = new MockReadRowsReader;
  {
    testing::InSequence s;
    EXPECT_CALL(*client_, ReadRows(_, _))
        .WillOnce(Invoke(stream->MakeMockReturner()));
    EXPECT_CALL(*stream, Read(_)).WillOnce(Return(true));
    EXPECT_CALL(*stream, Read(_)).WillOnce(Return(true));
    EXPECT_CALL(*stream, Read(_)).WillOnce(Return(true));
    EXPECT_CALL(*stream, Read(_)).WillOnce(Return(false));
    EXPECT_CALL(*stream, Finish()).WillOnce(Return(grpc::Status::OK));
  }

  parser_factory_->AddParser(std::move(parser));
  bigtable::RowReader reader(
      client_, bigtable::TableId(""), bigtable::RowSet(),
      bigtable::RowReader::NO_ROWS_LIMIT, bigtable::Filter::PassAllFilter(),
      std::move(retry_policy_), std::move(backoff_policy_),
      metadata_update_policy_, std::move(parser_factory_));

  auto it = reader.begin();
  EXPECT_NE(it, reader.end());
  ASSERT_STATUS_OK(*it);
  EXPECT_EQ((*it)->row_key(), "r1");
  EXPECT_NE(it, reader.end());
  // Manually cancel the call.
  reader.Cancel();
  it = reader.begin();
  EXPECT_NE(it, reader.end());
  EXPECT_FALSE(*it);
}

TEST_F(RowReaderTest, BeginThrowsAfterImmediateCancelNoExcept) {
  auto backend =
      std::make_shared<google::cloud::testing_util::CaptureLogLinesBackend>();
  auto id = google::cloud::LogSink::Instance().AddBackend(backend);

  std::unique_ptr<bigtable::RowReader> reader(new bigtable::RowReader(
      client_, bigtable::TableId(""), bigtable::RowSet(),
      bigtable::RowReader::NO_ROWS_LIMIT, bigtable::Filter::PassAllFilter(),
      std::move(retry_policy_), std::move(backoff_policy_),
      metadata_update_policy_, std::move(parser_factory_)));
  // Manually cancel the call before a stream was created.
  reader->Cancel();
  reader->begin();

  auto it = reader->begin();
  EXPECT_NE(it, reader->end());
  EXPECT_FALSE(*it);

  // Delete the reader and verify no log is produced because we handled the
  // error.
  reader.reset();

  google::cloud::LogSink::Instance().RemoveBackend(id);

  std::string const expected_message =
      "RowReader has an error, and the error status was not retrieved";
  auto count =
      std::count_if(backend->log_lines.begin(), backend->log_lines.end(),
                    [&expected_message](std::string const& line) {
                      return line.find(expected_message) != std::string::npos;
                    });
  EXPECT_EQ(0U, count) << "Unexpected log captured: "
                       << backend->log_lines.front();
}

TEST_F(RowReaderTest, RowReaderConstructorDoesNotCallRpc) {
  // The RowReader constructor/destructor by themselves should not
  // invoke the RPC or create parsers (the latter restriction because
  // parsers are per-connection and non-reusable).
  EXPECT_CALL(*client_, ReadRows(_, _)).Times(0);
  EXPECT_CALL(*parser_factory_, CreateHook()).Times(0);

  bigtable::RowReader reader(
      client_, bigtable::TableId(""), bigtable::RowSet(),
      bigtable::RowReader::NO_ROWS_LIMIT, bigtable::Filter::PassAllFilter(),
      std::move(retry_policy_), std::move(backoff_policy_),
      metadata_update_policy_, std::move(parser_factory_));
}

TEST_F(RowReaderTest, FailedStreamRetryNewContext) {
  using namespace ::testing;
  // Every retry should use a new ClientContext object.
  auto* stream = new MockReadRowsReader;  // wrapped in unique_ptr by ReadRows
  auto parser = google::cloud::internal::make_unique<ReadRowsParserMock>();
  parser->SetRows({"r1"});

  void* previous_context = nullptr;
  EXPECT_CALL(*retry_policy_, SetupHook(_))
      .Times(2)
      .WillRepeatedly(Invoke([&previous_context](grpc::ClientContext& context) {
        // This is a big hack, we want to make sure the context is new,
        // but there is no easy way to check that, so we compare addresses.
        EXPECT_NE(previous_context, &context);
        previous_context = &context;
      }));

  {
    testing::InSequence s;
    EXPECT_CALL(*client_, ReadRows(_, _))
        .WillOnce(Invoke(stream->MakeMockReturner()));
    EXPECT_CALL(*stream, Read(_)).WillOnce(Return(false));
    EXPECT_CALL(*stream, Finish())
        .WillOnce(Return(grpc::Status(grpc::StatusCode::INTERNAL, "retry")));

    EXPECT_CALL(*retry_policy_, OnFailureHook(_)).WillOnce(Return(true));
    EXPECT_CALL(*backoff_policy_, OnCompletionHook(_))
        .WillOnce(Return(std::chrono::milliseconds(0)));

    auto stream_retry = new MockReadRowsReader;  // the stub will free it
    EXPECT_CALL(*client_, ReadRows(_, _))
        .WillOnce(Invoke(stream_retry->MakeMockReturner()));
    EXPECT_CALL(*stream_retry, Read(_)).WillOnce(Return(true));
    EXPECT_CALL(*stream_retry, Read(_)).WillOnce(Return(false));
    EXPECT_CALL(*stream_retry, Finish()).WillOnce(Return(grpc::Status::OK));
  }

  parser_factory_->AddParser(std::move(parser));
  bigtable::RowReader reader(
      client_, bigtable::TableId(""), bigtable::RowSet(),
      bigtable::RowReader::NO_ROWS_LIMIT, bigtable::Filter::PassAllFilter(),
      std::move(retry_policy_), std::move(backoff_policy_),
      metadata_update_policy_, std::move(parser_factory_));

  auto it = reader.begin();
  EXPECT_NE(it, reader.end());
  ASSERT_STATUS_OK(*it);
  EXPECT_EQ((*it)->row_key(), "r1");
  EXPECT_EQ(++it, reader.end());
}
