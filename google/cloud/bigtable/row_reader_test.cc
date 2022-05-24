// Copyright 2017 Google Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "google/cloud/bigtable/row_reader.h"
#include "google/cloud/bigtable/internal/legacy_row_reader_impl.h"
#include "google/cloud/bigtable/testing/mock_policies.h"
#include "google/cloud/bigtable/testing/mock_read_rows_reader.h"
#include "google/cloud/bigtable/testing/table_test_fixture.h"
#include "google/cloud/internal/api_client_header.h"
#include "google/cloud/internal/throw_delegate.h"
#include "google/cloud/testing_util/scoped_log.h"
#include "google/cloud/testing_util/status_matchers.h"
#include "google/cloud/testing_util/validate_metadata.h"
#include "absl/memory/memory.h"
#include <gmock/gmock.h>
#include <deque>
#include <initializer_list>

namespace google {
namespace cloud {
namespace bigtable {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::google::bigtable::v2::ReadRowsRequest;
using ::google::bigtable::v2::ReadRowsResponse_CellChunk;
using ::google::cloud::bigtable::Row;
using ::google::cloud::bigtable::testing::MockBackoffPolicy;
using ::google::cloud::bigtable::testing::MockReadRowsReader;
using ::google::cloud::bigtable::testing::MockRetryPolicy;
using ::google::cloud::testing_util::StatusIs;
using ::google::cloud::testing_util::ValidateMetadataFixture;
using ::testing::_;
using ::testing::An;
using ::testing::Contains;
using ::testing::DoAll;
using ::testing::Eq;
using ::testing::HasSubstr;
using ::testing::Matcher;
using ::testing::NiceMock;
using ::testing::Not;
using ::testing::Property;
using ::testing::Return;
using ::testing::SetArgPointee;
using ::testing::SetArgReferee;

class ReadRowsParserMock : public bigtable::internal::ReadRowsParser {
 public:
  MOCK_METHOD(void, HandleChunkHook,
              (ReadRowsResponse_CellChunk chunk, grpc::Status& status));
  void HandleChunk(ReadRowsResponse_CellChunk chunk,
                   grpc::Status& status) override {
    HandleChunkHook(chunk, status);
  }

  MOCK_METHOD(void, HandleEndOfStreamHook, (grpc::Status & status));
  void HandleEndOfStream(grpc::Status& status) override {
    HandleEndOfStreamHook(status);
  }

  bool HasNext() const override { return !rows_.empty(); }

  Row Next(grpc::Status&) override {
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

  MOCK_METHOD(void, CreateHook, ());
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

// Match the number of expected row keys in a request in EXPECT_CALL
Matcher<ReadRowsRequest const&> RequestWithRowKeysCount(int n) {
  return Property(
      &ReadRowsRequest::rows,
      Property(&google::bigtable::v2::RowSet::row_keys_size, Eq(n)));
}

// Match the row limit in a request
Matcher<ReadRowsRequest const&> RequestWithRowsLimit(std::int64_t n) {
  return Property(&ReadRowsRequest::rows_limit, Eq(n));
}

class RowReaderTest : public bigtable::testing::TableTestFixture {
 public:
  RowReaderTest()
      : TableTestFixture(CompletionQueue{}),
        retry_policy_(absl::make_unique<NiceMock<MockRetryPolicy>>()),
        backoff_policy_(absl::make_unique<NiceMock<MockBackoffPolicy>>()),
        metadata_update_policy_(kTableName,
                                bigtable::MetadataParamTypes::TABLE_NAME),
        parser_factory_(
            absl::make_unique<NiceMock<ReadRowsParserMockFactory>>()) {}

  std::unique_ptr<MockRetryPolicy> retry_policy_;
  std::unique_ptr<MockBackoffPolicy> backoff_policy_;
  bigtable::MetadataUpdatePolicy metadata_update_policy_;
  std::unique_ptr<NiceMock<ReadRowsParserMockFactory>> parser_factory_;
};

TEST_F(RowReaderTest, EmptyReaderHasNoRows) {
  // wrapped in unique_ptr by ReadRows
  auto* stream = new MockReadRowsReader("google.bigtable.v2.Bigtable.ReadRows");
  EXPECT_CALL(*stream, Read).WillOnce(Return(false));
  EXPECT_CALL(*stream, Finish).WillOnce(Return(grpc::Status::OK));
  EXPECT_CALL(*client_, ReadRows).WillOnce(stream->MakeMockReturner());

  auto impl = std::make_shared<bigtable_internal::LegacyRowReaderImpl>(
      client_, "", bigtable::RowSet(), bigtable::RowReader::NO_ROWS_LIMIT,
      bigtable::Filter::PassAllFilter(), std::move(retry_policy_),
      std::move(backoff_policy_), metadata_update_policy_,
      std::move(parser_factory_));
  auto reader = bigtable_internal::MakeRowReader(std::move(impl));

  EXPECT_EQ(reader.begin(), reader.end());
}

TEST_F(RowReaderTest, ReadOneRow) {
  // wrapped in unique_ptr by ReadRows
  auto* stream = new MockReadRowsReader("google.bigtable.v2.Bigtable.ReadRows");
  auto parser = absl::make_unique<ReadRowsParserMock>();
  parser->SetRows({"r1"});
  EXPECT_CALL(*parser, HandleEndOfStreamHook).Times(1);
  {
    ::testing::InSequence s;
    EXPECT_CALL(*client_, ReadRows).WillOnce(stream->MakeMockReturner());
    EXPECT_CALL(*stream, Read).WillOnce(Return(true));
    EXPECT_CALL(*stream, Read).WillOnce(Return(false));
    EXPECT_CALL(*stream, Finish()).WillOnce(Return(grpc::Status::OK));
  }

  parser_factory_->AddParser(std::move(parser));
  auto impl = std::make_shared<bigtable_internal::LegacyRowReaderImpl>(
      client_, "", bigtable::RowSet(), bigtable::RowReader::NO_ROWS_LIMIT,
      bigtable::Filter::PassAllFilter(), std::move(retry_policy_),
      std::move(backoff_policy_), metadata_update_policy_,
      std::move(parser_factory_));
  auto reader = bigtable_internal::MakeRowReader(std::move(impl));

  auto it = reader.begin();
  EXPECT_NE(it, reader.end());
  ASSERT_STATUS_OK(*it);
  EXPECT_EQ((*it)->row_key(), "r1");
  EXPECT_EQ(++it, reader.end());
}

TEST_F(RowReaderTest, ReadOneRowAppProfileId) {
  // wrapped in unique_ptr by ReadRows
  auto parser = absl::make_unique<ReadRowsParserMock>();
  parser->SetRows({"r1"});
  EXPECT_CALL(*parser, HandleEndOfStreamHook).Times(1);
  std::string expected_id = "test-id";
  EXPECT_CALL(*client_, ReadRows)
      .WillOnce([expected_id](grpc::ClientContext* context,
                              ReadRowsRequest const& req) {
        ValidateMetadataFixture fixture;
        EXPECT_STATUS_OK(fixture.IsContextMDValid(
            *context, "google.bigtable.v2.Bigtable.ReadRows",
            google::cloud::internal::ApiClientHeader()));
        EXPECT_EQ(expected_id, req.app_profile_id());
        auto stream = absl::make_unique<MockReadRowsReader>(
            "google.bigtable.v2.Bigtable.ReadRows");
        ::testing::InSequence s;
        EXPECT_CALL(*stream, Read).WillOnce(Return(true));
        EXPECT_CALL(*stream, Read).WillOnce(Return(false));
        EXPECT_CALL(*stream, Finish).WillOnce(Return(grpc::Status::OK));
        return stream;
      });

  parser_factory_->AddParser(std::move(parser));
  auto impl = std::make_shared<bigtable_internal::LegacyRowReaderImpl>(
      client_, "test-id", "", bigtable::RowSet(),
      bigtable::RowReader::NO_ROWS_LIMIT, bigtable::Filter::PassAllFilter(),
      std::move(retry_policy_), std::move(backoff_policy_),
      metadata_update_policy_, std::move(parser_factory_));
  auto reader = bigtable_internal::MakeRowReader(std::move(impl));

  auto it = reader.begin();
  EXPECT_NE(it, reader.end());
  ASSERT_STATUS_OK(*it);
  EXPECT_EQ((*it)->row_key(), "r1");
  EXPECT_EQ(++it, reader.end());
}

TEST_F(RowReaderTest, ReadOneRowIteratorPostincrement) {
  // wrapped in unique_ptr by ReadRows
  auto* stream = new MockReadRowsReader("google.bigtable.v2.Bigtable.ReadRows");
  auto parser = absl::make_unique<ReadRowsParserMock>();
  parser->SetRows({"r1"});
  EXPECT_CALL(*parser, HandleEndOfStreamHook).Times(1);
  {
    ::testing::InSequence s;
    EXPECT_CALL(*client_, ReadRows).WillOnce(stream->MakeMockReturner());
    EXPECT_CALL(*stream, Read).WillOnce(Return(true));
    EXPECT_CALL(*stream, Read).WillOnce(Return(false));
    EXPECT_CALL(*stream, Finish()).WillOnce(Return(grpc::Status::OK));
  }

  parser_factory_->AddParser(std::move(parser));
  auto impl = std::make_shared<bigtable_internal::LegacyRowReaderImpl>(
      client_, "", bigtable::RowSet(), bigtable::RowReader::NO_ROWS_LIMIT,
      bigtable::Filter::PassAllFilter(), std::move(retry_policy_),
      std::move(backoff_policy_), metadata_update_policy_,
      std::move(parser_factory_));
  auto reader = bigtable_internal::MakeRowReader(std::move(impl));

  auto it = reader.begin();
  EXPECT_NE(it, reader.end());
  // This postincrement is what we are testing
  auto it2 = it++;
  ASSERT_STATUS_OK(*it2);
  EXPECT_EQ((*it2)->row_key(), "r1");
  EXPECT_EQ(it, reader.end());
}

TEST_F(RowReaderTest, ReadOneOfTwoRowsClosesStream) {
  // wrapped in unique_ptr by ReadRows
  auto* stream = new MockReadRowsReader("google.bigtable.v2.Bigtable.ReadRows");
  auto parser = absl::make_unique<ReadRowsParserMock>();
  parser->SetRows({"r1"});
  {
    ::testing::InSequence s;
    EXPECT_CALL(*client_, ReadRows).WillOnce(stream->MakeMockReturner());
    EXPECT_CALL(*stream, Read).WillOnce(Return(true));
    EXPECT_CALL(*stream, Read).WillOnce(Return(true));
    EXPECT_CALL(*stream, Read).WillOnce(Return(true));
    EXPECT_CALL(*stream, Read).WillOnce(Return(false));
    EXPECT_CALL(*stream, Finish()).WillOnce(Return(grpc::Status::OK));
  }

  parser_factory_->AddParser(std::move(parser));
  auto impl = std::make_shared<bigtable_internal::LegacyRowReaderImpl>(
      client_, "", bigtable::RowSet(), bigtable::RowReader::NO_ROWS_LIMIT,
      bigtable::Filter::PassAllFilter(), std::move(retry_policy_),
      std::move(backoff_policy_), metadata_update_policy_,
      std::move(parser_factory_));
  auto reader = bigtable_internal::MakeRowReader(std::move(impl));

  auto it = reader.begin();
  EXPECT_NE(it, reader.end());
  ASSERT_STATUS_OK(*it);
  EXPECT_EQ((*it)->row_key(), "r1");
  EXPECT_NE(it, reader.end());
  // Do not finish the iteration.  We still expect the stream to be finalized,
  // and the previously setup expectations on the mock `stream` check that.
}

TEST_F(RowReaderTest, FailedStreamIsRetried) {
  // wrapped in unique_ptr by ReadRows
  auto* stream = new MockReadRowsReader("google.bigtable.v2.Bigtable.ReadRows");
  auto parser = absl::make_unique<ReadRowsParserMock>();
  parser->SetRows({"r1"});
  {
    ::testing::InSequence s;
    EXPECT_CALL(*client_, ReadRows).WillOnce(stream->MakeMockReturner());
    EXPECT_CALL(*stream, Read).WillOnce(Return(false));
    EXPECT_CALL(*stream, Finish())
        .WillOnce(Return(grpc::Status(grpc::StatusCode::INTERNAL, "retry")));

    EXPECT_CALL(*retry_policy_, OnFailure(An<Status const&>()))
        .WillOnce(Return(true));
    EXPECT_CALL(*backoff_policy_, OnCompletion(An<Status const&>()))
        .WillOnce(Return(std::chrono::milliseconds(0)));

    // the stub will free it
    auto* stream_retry =
        new MockReadRowsReader("google.bigtable.v2.Bigtable.ReadRows");
    EXPECT_CALL(*client_, ReadRows).WillOnce(stream_retry->MakeMockReturner());
    EXPECT_CALL(*stream_retry, Read).WillOnce(Return(true));
    EXPECT_CALL(*stream_retry, Read).WillOnce(Return(false));
    EXPECT_CALL(*stream_retry, Finish()).WillOnce(Return(grpc::Status::OK));
  }

  parser_factory_->AddParser(std::move(parser));
  auto impl = std::make_shared<bigtable_internal::LegacyRowReaderImpl>(
      client_, "", bigtable::RowSet(), bigtable::RowReader::NO_ROWS_LIMIT,
      bigtable::Filter::PassAllFilter(), std::move(retry_policy_),
      std::move(backoff_policy_), metadata_update_policy_,
      std::move(parser_factory_));
  auto reader = bigtable_internal::MakeRowReader(std::move(impl));

  auto it = reader.begin();
  EXPECT_NE(it, reader.end());
  ASSERT_STATUS_OK(*it);
  EXPECT_EQ((*it)->row_key(), "r1");
  EXPECT_EQ(++it, reader.end());
}

TEST_F(RowReaderTest, FailedStreamWithNoRetryThrowsNoExcept) {
  // wrapped in unique_ptr by ReadRows
  auto* stream = new MockReadRowsReader("google.bigtable.v2.Bigtable.ReadRows");
  auto parser = absl::make_unique<ReadRowsParserMock>();
  {
    ::testing::InSequence s;
    EXPECT_CALL(*client_, ReadRows).WillOnce(stream->MakeMockReturner());
    EXPECT_CALL(*stream, Read).WillOnce(Return(false));
    EXPECT_CALL(*stream, Finish())
        .WillOnce(Return(grpc::Status(grpc::StatusCode::INTERNAL, "retry")));

    EXPECT_CALL(*retry_policy_, OnFailure(An<Status const&>()))
        .WillOnce(Return(false));
    EXPECT_CALL(*backoff_policy_, OnCompletion(An<Status const&>())).Times(0);
  }

  parser_factory_->AddParser(std::move(parser));
  auto impl = std::make_shared<bigtable_internal::LegacyRowReaderImpl>(
      client_, "", bigtable::RowSet(), bigtable::RowReader::NO_ROWS_LIMIT,
      bigtable::Filter::PassAllFilter(), std::move(retry_policy_),
      std::move(backoff_policy_), metadata_update_policy_,
      std::move(parser_factory_));
  auto reader = bigtable_internal::MakeRowReader(std::move(impl));

  auto it = reader.begin();
  EXPECT_NE(it, reader.end());
  EXPECT_FALSE(*it);
}

TEST_F(RowReaderTest, FailedStreamRetriesSkipAlreadyReadRows) {
  // wrapped in unique_ptr by ReadRows
  auto* stream = new MockReadRowsReader("google.bigtable.v2.Bigtable.ReadRows");
  auto parser = absl::make_unique<ReadRowsParserMock>();
  parser->SetRows({"r1"});
  {
    ::testing::InSequence s;
    // As a baseline, check we have two rows in the initial request
    EXPECT_CALL(*client_, ReadRows(_, RequestWithRowKeysCount(2)))
        .WillOnce(stream->MakeMockReturner());

    EXPECT_CALL(*stream, Read).WillOnce(Return(true));
    EXPECT_CALL(*stream, Read).WillOnce(Return(false));
    EXPECT_CALL(*stream, Finish())
        .WillOnce(Return(grpc::Status(grpc::StatusCode::INTERNAL, "retry")));

    EXPECT_CALL(*retry_policy_, OnFailure(An<Status const&>()))
        .WillOnce(Return(true));
    EXPECT_CALL(*backoff_policy_, OnCompletion(An<Status const&>()))
        .WillOnce(Return(std::chrono::milliseconds(0)));

    // the stub will free it
    auto* stream_retry =
        new MockReadRowsReader("google.bigtable.v2.Bigtable.ReadRows");
    // First row should be removed from the retried request, leaving one row
    EXPECT_CALL(*client_, ReadRows(_, RequestWithRowKeysCount(1)))
        .WillOnce(stream_retry->MakeMockReturner());
    EXPECT_CALL(*stream_retry, Read).WillOnce(Return(false));
    EXPECT_CALL(*stream_retry, Finish()).WillOnce(Return(grpc::Status::OK));
  }

  parser_factory_->AddParser(std::move(parser));
  auto impl = std::make_shared<bigtable_internal::LegacyRowReaderImpl>(
      client_, "", bigtable::RowSet("r1", "r2"),
      bigtable::RowReader::NO_ROWS_LIMIT, bigtable::Filter::PassAllFilter(),
      std::move(retry_policy_), std::move(backoff_policy_),
      metadata_update_policy_, std::move(parser_factory_));
  auto reader = bigtable_internal::MakeRowReader(std::move(impl));

  auto it = reader.begin();
  EXPECT_NE(it, reader.end());
  ASSERT_STATUS_OK(*it);
  EXPECT_EQ((*it)->row_key(), "r1");
  EXPECT_EQ(++it, reader.end());
}

TEST_F(RowReaderTest, FailedStreamRetriesSkipToLastScannedRow) {
  auto* stream = new MockReadRowsReader("google.bigtable.v2.Bigtable.ReadRows");
  auto parser = absl::make_unique<ReadRowsParserMock>();
  parser->SetRows({"r1"});
  google::bigtable::v2::ReadRowsResponse response;
  response.set_last_scanned_row_key("r2");
  {
    ::testing::InSequence s;
    // We start our call with 3 rows in the set: "r1", "r2", "r3".
    EXPECT_CALL(*client_, ReadRows(_, RequestWithRowKeysCount(3)))
        .WillOnce(stream->MakeMockReturner());

    // The mock `parser` will return "r1". Next, simulate the server returning
    // an empty chunk with `last_scanned_row_key` set to "r2".
    EXPECT_CALL(*stream, Read)
        .WillOnce(DoAll(SetArgPointee<0>(response), Return(true)));

    // The stream fails with a retry-able error
    EXPECT_CALL(*stream, Read).WillOnce(Return(false));
    EXPECT_CALL(*stream, Finish())
        .WillOnce(Return(grpc::Status(grpc::StatusCode::INTERNAL, "retry")));

    EXPECT_CALL(*retry_policy_, OnFailure(An<Status const&>()))
        .WillOnce(Return(true));
    EXPECT_CALL(*backoff_policy_, OnCompletion(An<Status const&>()))
        .WillOnce(Return(std::chrono::milliseconds(0)));

    auto* stream_retry =
        new MockReadRowsReader("google.bigtable.v2.Bigtable.ReadRows");

    // We retry the remaining rows. We have "r1" returned, but the service has
    // also told us that "r2" was scanned. This means there is only one row
    // remaining to read: "r3".
    EXPECT_CALL(*client_, ReadRows(_, RequestWithRowKeysCount(1)))
        .WillOnce(stream_retry->MakeMockReturner());

    // End the stream to clean up the test
    EXPECT_CALL(*stream_retry, Read).WillOnce(Return(false));
    EXPECT_CALL(*stream_retry, Finish()).WillOnce(Return(grpc::Status::OK));
  }

  parser_factory_->AddParser(std::move(parser));
  auto impl = std::make_shared<bigtable_internal::LegacyRowReaderImpl>(
      client_, "", bigtable::RowSet("r1", "r2", "r3"),
      bigtable::RowReader::NO_ROWS_LIMIT, bigtable::Filter::PassAllFilter(),
      std::move(retry_policy_), std::move(backoff_policy_),
      metadata_update_policy_, std::move(parser_factory_));
  auto reader = bigtable_internal::MakeRowReader(std::move(impl));

  auto it = reader.begin();
  EXPECT_NE(it, reader.end());
  ASSERT_STATUS_OK(*it);
  EXPECT_EQ((*it)->row_key(), "r1");
  EXPECT_EQ(++it, reader.end());
}

TEST_F(RowReaderTest, FailedParseIsRetried) {
  // wrapped in unique_ptr by ReadRows
  auto* stream = new MockReadRowsReader("google.bigtable.v2.Bigtable.ReadRows");
  auto parser = absl::make_unique<ReadRowsParserMock>();
  parser->SetRows({"r1"});
  auto response = bigtable::testing::ReadRowsResponseFromString("chunks {}");
  {
    ::testing::InSequence s;
    EXPECT_CALL(*client_, ReadRows).WillOnce(stream->MakeMockReturner());
    EXPECT_CALL(*stream, Read)
        .WillOnce(DoAll(SetArgPointee<0>(response), Return(true)));
    EXPECT_CALL(*parser, HandleChunkHook)
        .WillOnce(SetArgReferee<1>(
            grpc::Status(grpc::StatusCode::INTERNAL, "parser exception")));

    EXPECT_CALL(*retry_policy_, OnFailure(An<Status const&>()))
        .WillOnce(Return(true));
    EXPECT_CALL(*backoff_policy_, OnCompletion(An<Status const&>()))
        .WillOnce(Return(std::chrono::milliseconds(0)));

    // the stub will free it
    auto* stream_retry =
        new MockReadRowsReader("google.bigtable.v2.Bigtable.ReadRows");
    EXPECT_CALL(*client_, ReadRows).WillOnce(stream_retry->MakeMockReturner());
    EXPECT_CALL(*stream_retry, Read).WillOnce(Return(true));
    EXPECT_CALL(*stream_retry, Read).WillOnce(Return(false));
    EXPECT_CALL(*stream_retry, Finish()).WillOnce(Return(grpc::Status::OK));
  }

  parser_factory_->AddParser(std::move(parser));
  auto impl = std::make_shared<bigtable_internal::LegacyRowReaderImpl>(
      client_, "", bigtable::RowSet(), bigtable::RowReader::NO_ROWS_LIMIT,
      bigtable::Filter::PassAllFilter(), std::move(retry_policy_),
      std::move(backoff_policy_), metadata_update_policy_,
      std::move(parser_factory_));
  auto reader = bigtable_internal::MakeRowReader(std::move(impl));

  auto it = reader.begin();
  EXPECT_NE(it, reader.end());
  ASSERT_STATUS_OK(*it);
  EXPECT_EQ((*it)->row_key(), "r1");
  EXPECT_EQ(++it, reader.end());
}

TEST_F(RowReaderTest, FailedParseRetriesSkipAlreadyReadRows) {
  // wrapped in unique_ptr by ReadRows
  auto* stream = new MockReadRowsReader("google.bigtable.v2.Bigtable.ReadRows");
  auto parser = absl::make_unique<ReadRowsParserMock>();
  parser->SetRows({"r1"});
  {
    ::testing::InSequence s;
    // As a baseline, check we have two rows in the initial request
    EXPECT_CALL(*client_, ReadRows(_, RequestWithRowKeysCount(2)))
        .WillOnce(stream->MakeMockReturner());

    EXPECT_CALL(*stream, Read).WillOnce(Return(true));
    EXPECT_CALL(*stream, Read).WillOnce(Return(false));
    EXPECT_CALL(*stream, Finish()).WillOnce(Return(grpc::Status::OK));
    grpc::Status status;
    EXPECT_CALL(*parser, HandleEndOfStreamHook)
        .WillOnce(SetArgReferee<0>(
            grpc::Status(grpc::StatusCode::INTERNAL, "InternalError")));

    EXPECT_CALL(*retry_policy_, OnFailure(An<Status const&>()))
        .WillOnce(Return(true));
    EXPECT_CALL(*backoff_policy_, OnCompletion(An<Status const&>()))
        .WillOnce(Return(std::chrono::milliseconds(0)));

    // the stub will free it
    auto* stream_retry =
        new MockReadRowsReader("google.bigtable.v2.Bigtable.ReadRows");
    // First row should be removed from the retried request, leaving one row
    EXPECT_CALL(*client_, ReadRows(_, RequestWithRowKeysCount(1)))
        .WillOnce(stream_retry->MakeMockReturner());
    EXPECT_CALL(*stream_retry, Read).WillOnce(Return(false));
    EXPECT_CALL(*stream_retry, Finish()).WillOnce(Return(grpc::Status::OK));
  }

  parser_factory_->AddParser(std::move(parser));
  auto impl = std::make_shared<bigtable_internal::LegacyRowReaderImpl>(
      client_, "", bigtable::RowSet("r1", "r2"),
      bigtable::RowReader::NO_ROWS_LIMIT, bigtable::Filter::PassAllFilter(),
      std::move(retry_policy_), std::move(backoff_policy_),
      metadata_update_policy_, std::move(parser_factory_));
  auto reader = bigtable_internal::MakeRowReader(std::move(impl));

  auto it = reader.begin();
  EXPECT_NE(it, reader.end());
  ASSERT_STATUS_OK(*it);
  EXPECT_EQ((*it)->row_key(), "r1");
  EXPECT_EQ(++it, reader.end());
}

TEST_F(RowReaderTest, FailedParseWithNoRetryThrowsNoExcept) {
  // wrapped in unique_ptr by ReadRows
  auto* stream = new MockReadRowsReader("google.bigtable.v2.Bigtable.ReadRows");
  auto parser = absl::make_unique<ReadRowsParserMock>();
  {
    ::testing::InSequence s;

    EXPECT_CALL(*client_, ReadRows).WillOnce(stream->MakeMockReturner());
    EXPECT_CALL(*stream, Read).WillOnce(Return(false));
    EXPECT_CALL(*stream, Finish()).WillOnce(Return(grpc::Status::OK));
    grpc::Status status;
    EXPECT_CALL(*parser, HandleEndOfStreamHook)
        .WillOnce(SetArgReferee<0>(
            grpc::Status(grpc::StatusCode::INTERNAL, "InternalError")));
    EXPECT_CALL(*retry_policy_, OnFailure(An<Status const&>()))
        .WillOnce(Return(false));
    EXPECT_CALL(*backoff_policy_, OnCompletion(An<Status const&>())).Times(0);
  }

  parser_factory_->AddParser(std::move(parser));
  auto impl = std::make_shared<bigtable_internal::LegacyRowReaderImpl>(
      client_, "", bigtable::RowSet(), bigtable::RowReader::NO_ROWS_LIMIT,
      bigtable::Filter::PassAllFilter(), std::move(retry_policy_),
      std::move(backoff_policy_), metadata_update_policy_,
      std::move(parser_factory_));
  auto reader = bigtable_internal::MakeRowReader(std::move(impl));

  auto it = reader.begin();
  EXPECT_NE(it, reader.end());
  EXPECT_FALSE(*it);
}

TEST_F(RowReaderTest, FailedStreamWithAllRequiredRowsSeenShouldNotRetry) {
  // wrapped in unique_ptr by ReadRows
  auto* stream = new MockReadRowsReader("google.bigtable.v2.Bigtable.ReadRows");
  auto parser = absl::make_unique<ReadRowsParserMock>();
  parser->SetRows({"r2"});
  {
    ::testing::InSequence s;
    EXPECT_CALL(*client_, ReadRows).WillOnce(stream->MakeMockReturner());

    EXPECT_CALL(*stream, Read).WillOnce(Return(true));
    EXPECT_CALL(*stream, Read).WillOnce(Return(false));
    EXPECT_CALL(*stream, Finish())
        .WillOnce(Return(grpc::Status(grpc::StatusCode::INTERNAL,
                                      "this exception must be ignored")));

    // Note there is no expectation of a new connection, because the
    // set of rows to read should become empty after reading "r2" and
    // intersecting the requested ["r1", "r2"] with ("r2", "") for the
    // retry.
  }

  parser_factory_->AddParser(std::move(parser));
  auto impl = std::make_shared<bigtable_internal::LegacyRowReaderImpl>(
      client_, "", bigtable::RowSet(bigtable::RowRange::Closed("r1", "r2")),
      bigtable::RowReader::NO_ROWS_LIMIT, bigtable::Filter::PassAllFilter(),
      std::move(retry_policy_), std::move(backoff_policy_),
      metadata_update_policy_, std::move(parser_factory_));
  auto reader = bigtable_internal::MakeRowReader(std::move(impl));

  auto it = reader.begin();
  EXPECT_NE(it, reader.end());
  ASSERT_STATUS_OK(*it);
  EXPECT_EQ((*it)->row_key(), "r2");
  EXPECT_EQ(++it, reader.end());
}

TEST_F(RowReaderTest, RowLimitIsSent) {
  // wrapped in unique_ptr by ReadRows
  auto* stream = new MockReadRowsReader("google.bigtable.v2.Bigtable.ReadRows");
  EXPECT_CALL(*client_, ReadRows(_, RequestWithRowsLimit(442)))
      .WillOnce(stream->MakeMockReturner());
  EXPECT_CALL(*stream, Read).WillOnce(Return(false));
  EXPECT_CALL(*stream, Finish()).WillOnce(Return(grpc::Status::OK));

  auto impl = std::make_shared<bigtable_internal::LegacyRowReaderImpl>(
      client_, "", bigtable::RowSet(), 442, bigtable::Filter::PassAllFilter(),
      std::move(retry_policy_), std::move(backoff_policy_),
      metadata_update_policy_, std::move(parser_factory_));
  auto reader = bigtable_internal::MakeRowReader(std::move(impl));

  auto it = reader.begin();
  EXPECT_EQ(it, reader.end());
}

TEST_F(RowReaderTest, RowLimitIsDecreasedOnRetry) {
  // wrapped in unique_ptr by ReadRows
  auto* stream = new MockReadRowsReader("google.bigtable.v2.Bigtable.ReadRows");
  auto parser = absl::make_unique<ReadRowsParserMock>();
  parser->SetRows({"r1"});
  {
    ::testing::InSequence s;
    EXPECT_CALL(*client_, ReadRows(_, RequestWithRowsLimit(42)))
        .WillOnce(stream->MakeMockReturner());

    EXPECT_CALL(*stream, Read).WillOnce(Return(true));
    EXPECT_CALL(*stream, Read).WillOnce(Return(false));
    EXPECT_CALL(*stream, Finish())
        .WillOnce(Return(grpc::Status(grpc::StatusCode::INTERNAL, "retry")));

    EXPECT_CALL(*retry_policy_, OnFailure(An<Status const&>()))
        .WillOnce(Return(true));
    EXPECT_CALL(*backoff_policy_, OnCompletion(An<Status const&>()))
        .WillOnce(Return(std::chrono::milliseconds(0)));

    // the stub will free it
    auto* stream_retry =
        new MockReadRowsReader("google.bigtable.v2.Bigtable.ReadRows");
    // 41 instead of 42
    EXPECT_CALL(*client_, ReadRows(_, RequestWithRowsLimit(41)))
        .WillOnce(stream_retry->MakeMockReturner());
    EXPECT_CALL(*stream_retry, Read).WillOnce(Return(false));
    EXPECT_CALL(*stream_retry, Finish()).WillOnce(Return(grpc::Status::OK));
  }

  parser_factory_->AddParser(std::move(parser));
  auto impl = std::make_shared<bigtable_internal::LegacyRowReaderImpl>(
      client_, "", bigtable::RowSet(), 42, bigtable::Filter::PassAllFilter(),
      std::move(retry_policy_), std::move(backoff_policy_),
      metadata_update_policy_, std::move(parser_factory_));
  auto reader = bigtable_internal::MakeRowReader(std::move(impl));

  auto it = reader.begin();
  EXPECT_NE(it, reader.end());
  ASSERT_STATUS_OK(*it);
  EXPECT_EQ((*it)->row_key(), "r1");
  EXPECT_EQ(++it, reader.end());
}

TEST_F(RowReaderTest, RowLimitIsNotDecreasedToZero) {
  // wrapped in unique_ptr by ReadRows
  auto* stream = new MockReadRowsReader("google.bigtable.v2.Bigtable.ReadRows");
  auto parser = absl::make_unique<ReadRowsParserMock>();
  parser->SetRows({"r1"});
  {
    ::testing::InSequence s;
    EXPECT_CALL(*client_, ReadRows(_, RequestWithRowsLimit(1)))
        .WillOnce(stream->MakeMockReturner());

    EXPECT_CALL(*stream, Read).WillOnce(Return(true));
    EXPECT_CALL(*stream, Read).WillOnce(Return(false));
    EXPECT_CALL(*stream, Finish())
        .WillOnce(Return(grpc::Status(grpc::StatusCode::INTERNAL,
                                      "this exception must be ignored")));

    // Note there is no expectation of a new connection, because the
    // row limit reaches zero.
  }

  parser_factory_->AddParser(std::move(parser));
  auto impl = std::make_shared<bigtable_internal::LegacyRowReaderImpl>(
      client_, "", bigtable::RowSet(), 1, bigtable::Filter::PassAllFilter(),
      std::move(retry_policy_), std::move(backoff_policy_),
      metadata_update_policy_, std::move(parser_factory_));
  auto reader = bigtable_internal::MakeRowReader(std::move(impl));

  auto it = reader.begin();
  EXPECT_NE(it, reader.end());
  ASSERT_STATUS_OK(*it);
  EXPECT_EQ((*it)->row_key(), "r1");
  EXPECT_EQ(++it, reader.end());
}

TEST_F(RowReaderTest, BeginThrowsAfterCancelClosesStreamNoExcept) {
  auto parser = absl::make_unique<ReadRowsParserMock>();
  parser->SetRows({"r1"});
  auto* stream = new MockReadRowsReader("google.bigtable.v2.Bigtable.ReadRows");
  {
    ::testing::InSequence s;
    EXPECT_CALL(*client_, ReadRows).WillOnce(stream->MakeMockReturner());
    EXPECT_CALL(*stream, Read).WillOnce(Return(true));
    EXPECT_CALL(*stream, Read).WillOnce(Return(true));
    EXPECT_CALL(*stream, Read).WillOnce(Return(true));
    EXPECT_CALL(*stream, Read).WillOnce(Return(false));
    EXPECT_CALL(*stream, Finish()).WillOnce(Return(grpc::Status::OK));
  }

  parser_factory_->AddParser(std::move(parser));
  auto impl = std::make_shared<bigtable_internal::LegacyRowReaderImpl>(
      client_, "", bigtable::RowSet(), bigtable::RowReader::NO_ROWS_LIMIT,
      bigtable::Filter::PassAllFilter(), std::move(retry_policy_),
      std::move(backoff_policy_), metadata_update_policy_,
      std::move(parser_factory_));
  auto reader = bigtable_internal::MakeRowReader(std::move(impl));

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
  testing_util::ScopedLog log;

  {
    auto impl = std::make_shared<bigtable_internal::LegacyRowReaderImpl>(
        client_, "", bigtable::RowSet(), bigtable::RowReader::NO_ROWS_LIMIT,
        bigtable::Filter::PassAllFilter(), std::move(retry_policy_),
        std::move(backoff_policy_), metadata_update_policy_,
        std::move(parser_factory_));
    auto reader = bigtable_internal::MakeRowReader(std::move(impl));

    // Manually cancel the call before a stream was created.
    reader.Cancel();
    reader.begin();

    auto it = reader.begin();
    EXPECT_NE(it, reader.end());
    EXPECT_FALSE(*it);

    // Delete the reader and verify no log is produced because we handled the
    // error.
  }

  EXPECT_THAT(
      log.ExtractLines(),
      Not(Contains(HasSubstr(
          "RowReader has an error, and the error status was not retrieved"))));
}

TEST_F(RowReaderTest, RowReaderConstructorDoesNotCallRpc) {
  // The RowReader constructor/destructor by themselves should not
  // invoke the RPC or create parsers (the latter restriction because
  // parsers are per-connection and non-reusable).
  EXPECT_CALL(*client_, ReadRows).Times(0);
  EXPECT_CALL(*parser_factory_, CreateHook()).Times(0);

  auto impl = std::make_shared<bigtable_internal::LegacyRowReaderImpl>(
      client_, "", bigtable::RowSet(), bigtable::RowReader::NO_ROWS_LIMIT,
      bigtable::Filter::PassAllFilter(), std::move(retry_policy_),
      std::move(backoff_policy_), metadata_update_policy_,
      std::move(parser_factory_));
  auto reader = bigtable_internal::MakeRowReader(std::move(impl));
}

TEST_F(RowReaderTest, FailedStreamRetryNewContext) {
  // Every retry should use a new ClientContext object.
  // wrapped in unique_ptr by ReadRows
  auto* stream = new MockReadRowsReader("google.bigtable.v2.Bigtable.ReadRows");
  auto parser = absl::make_unique<ReadRowsParserMock>();
  parser->SetRows({"r1"});

  void* previous_context = nullptr;
  EXPECT_CALL(*retry_policy_, Setup)
      .Times(2)
      .WillRepeatedly([&previous_context](grpc::ClientContext& context) {
        // This is a big hack, we want to make sure the context is new,
        // but there is no easy way to check that, so we compare addresses.
        EXPECT_NE(previous_context, &context);
        previous_context = &context;
      });

  {
    ::testing::InSequence s;
    EXPECT_CALL(*client_, ReadRows).WillOnce(stream->MakeMockReturner());
    EXPECT_CALL(*stream, Read).WillOnce(Return(false));
    EXPECT_CALL(*stream, Finish())
        .WillOnce(Return(grpc::Status(grpc::StatusCode::INTERNAL, "retry")));

    EXPECT_CALL(*retry_policy_, OnFailure(An<Status const&>()))
        .WillOnce(Return(true));
    EXPECT_CALL(*backoff_policy_, OnCompletion(An<Status const&>()))
        .WillOnce(Return(std::chrono::milliseconds(0)));

    // the stub will free it
    auto* stream_retry =
        new MockReadRowsReader("google.bigtable.v2.Bigtable.ReadRows");
    EXPECT_CALL(*client_, ReadRows).WillOnce(stream_retry->MakeMockReturner());
    EXPECT_CALL(*stream_retry, Read).WillOnce(Return(true));
    EXPECT_CALL(*stream_retry, Read).WillOnce(Return(false));
    EXPECT_CALL(*stream_retry, Finish()).WillOnce(Return(grpc::Status::OK));
  }

  parser_factory_->AddParser(std::move(parser));
  auto impl = std::make_shared<bigtable_internal::LegacyRowReaderImpl>(
      client_, "", bigtable::RowSet(), bigtable::RowReader::NO_ROWS_LIMIT,
      bigtable::Filter::PassAllFilter(), std::move(retry_policy_),
      std::move(backoff_policy_), metadata_update_policy_,
      std::move(parser_factory_));
  auto reader = bigtable_internal::MakeRowReader(std::move(impl));

  auto it = reader.begin();
  EXPECT_NE(it, reader.end());
  ASSERT_STATUS_OK(*it);
  EXPECT_EQ((*it)->row_key(), "r1");
  EXPECT_EQ(++it, reader.end());
}

TEST(RowReader, DefaultConstructor) {
  RowReader reader;
  EXPECT_EQ(reader.begin(), reader.end());
}

TEST(RowReader, BadStatusOnly) {
  auto impl = std::make_shared<bigtable_internal::StatusOnlyRowReader>(
      Status(StatusCode::kUnimplemented, "unimplemented"));
  auto reader = bigtable_internal::MakeRowReader(std::move(impl));

  auto it = reader.begin();
  EXPECT_NE(it, reader.end());
  EXPECT_THAT(*it, StatusIs(StatusCode::kUnimplemented));
  EXPECT_EQ(++it, reader.end());
}

TEST(RowReader, OptionsSpan) {
  struct TestOption {
    using Type = std::string;
  };

  class MockRowReader : public bigtable_internal::RowReaderImpl {
   public:
    MOCK_METHOD(void, Cancel, (), (override));
    MOCK_METHOD((absl::variant<Status, bigtable::Row>), Advance, (),
                (override));
  };

  auto mock = std::make_shared<MockRowReader>();

  ::testing::InSequence s;
  EXPECT_CALL(*mock, Advance).Times(3).WillRepeatedly([]() {
    // Verify that the OptionsSpan from construction applies for each Advance.
    EXPECT_TRUE(google::cloud::internal::CurrentOptions().has<TestOption>());
    return Row("row", {});
  });
  EXPECT_CALL(*mock, Advance).WillOnce(Return(Status()));

  // Construct a RowReader with an active OptionsSpan.
  google::cloud::internal::OptionsSpan span(Options{}.set<TestOption>("set"));
  auto reader = bigtable_internal::MakeRowReader(std::move(mock));

  // Clear the OptionsSpan before we call begin().
  google::cloud::internal::OptionsSpan overlay(Options{});
  for (auto const& sor : reader) {
    EXPECT_STATUS_OK(sor);
  }
}

}  // anonymous namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable
}  // namespace cloud
}  // namespace google
