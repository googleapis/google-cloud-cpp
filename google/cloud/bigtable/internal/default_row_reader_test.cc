// Copyright 2022 Google LLC
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

#include "google/cloud/bigtable/internal/default_row_reader.h"
#include "google/cloud/bigtable/row_reader.h"
#include "google/cloud/bigtable/testing/mock_bigtable_stub.h"
#include "google/cloud/bigtable/testing/mock_policies.h"
#include "google/cloud/testing_util/mock_backoff_policy.h"
#include "google/cloud/testing_util/status_matchers.h"
#include "absl/memory/memory.h"
#include <gmock/gmock.h>
#include <grpcpp/client_context.h>
#include <chrono>

namespace google {
namespace cloud {
namespace bigtable_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::google::bigtable::v2::ReadRowsRequest;
using ::google::cloud::bigtable::testing::MockBigtableStub;
using ::google::cloud::bigtable::testing::MockDataRetryPolicy;
using ::google::cloud::bigtable::testing::MockReadRowsStream;
using ::google::cloud::testing_util::MockBackoffPolicy;
using ::google::cloud::testing_util::StatusIs;
using ::testing::Eq;
using ::testing::Matcher;
using ::testing::MockFunction;
using ::testing::Property;
using ::testing::Return;
using ms = std::chrono::milliseconds;

auto constexpr kNumRetries = 2;
auto const* const kAppProfile = "the-profile";
auto const* const kTableName =
    "projects/the-project/instances/the-instance/tables/the-table";

// Match the number of expected row keys in a request in EXPECT_CALL
Matcher<ReadRowsRequest const&> HasCorrectResourceNames() {
  return AllOf(Property(&ReadRowsRequest::app_profile_id, Eq(kAppProfile)),
               Property(&ReadRowsRequest::table_name, Eq(kTableName)));
}

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

google::bigtable::v2::ReadRowsResponse MakeRow(std::string row_key) {
  google::bigtable::v2::ReadRowsResponse response;
  auto& chunk = *response.add_chunks();
  *chunk.mutable_row_key() = std::move(row_key);
  chunk.mutable_family_name()->set_value("cf");
  chunk.mutable_qualifier()->set_value("cq");
  chunk.set_commit_row(true);
  return response;
}

google::bigtable::v2::ReadRowsResponse MalformedResponse() {
  // We return a malformed response that we know will fail in the parser, with
  // some INTERNAL error. This response fails because the column family is set,
  // but the column qualifier is not.
  google::bigtable::v2::ReadRowsResponse resp;
  auto& chunk = *resp.add_chunks();
  chunk.mutable_family_name()->set_value("cf");
  return resp;
}

class DefaultRowReaderTest : public ::testing::Test {
 protected:
  // Ensure that we set up the ClientContext once per stream
  Options TestOptions(int expected_streams) {
    EXPECT_CALL(mock_setup_, Call).Times(expected_streams);
    return Options{}.set<internal::GrpcSetupOption>(
        mock_setup_.AsStdFunction());
  }

  DataLimitedErrorCountRetryPolicy retry_ =
      DataLimitedErrorCountRetryPolicy(kNumRetries);
  ExponentialBackoffPolicy backoff_ =
      ExponentialBackoffPolicy(ms(0), ms(0), 2.0);
  MockFunction<void(grpc::ClientContext&)> mock_setup_;
};

TEST_F(DefaultRowReaderTest, EmptyReaderHasNoRows) {
  auto mock = std::make_shared<MockBigtableStub>();
  EXPECT_CALL(*mock, ReadRows)
      .WillOnce([](std::unique_ptr<grpc::ClientContext>,
                   google::bigtable::v2::ReadRowsRequest const& request) {
        EXPECT_THAT(request, HasCorrectResourceNames());
        auto stream = absl::make_unique<MockReadRowsStream>();
        EXPECT_CALL(*stream, Read).WillOnce(Return(Status()));
        return stream;
      });

  internal::OptionsSpan span(TestOptions(/*expected_streams=*/1));

  auto impl = std::make_shared<DefaultRowReader>(
      mock, kAppProfile, kTableName, bigtable::RowSet(),
      bigtable::RowReader::NO_ROWS_LIMIT, bigtable::Filter::PassAllFilter(),
      retry_.clone(), backoff_.clone());
  auto reader = bigtable_internal::MakeRowReader(std::move(impl));

  EXPECT_EQ(reader.begin(), reader.end());
}

TEST_F(DefaultRowReaderTest, ReadOneRow) {
  auto mock = std::make_shared<MockBigtableStub>();
  EXPECT_CALL(*mock, ReadRows)
      .WillOnce([](std::unique_ptr<grpc::ClientContext>,
                   google::bigtable::v2::ReadRowsRequest const& request) {
        EXPECT_THAT(request, HasCorrectResourceNames());
        auto stream = absl::make_unique<MockReadRowsStream>();
        EXPECT_CALL(*stream, Read)
            .WillOnce(Return(MakeRow("r1")))
            .WillOnce(Return(Status()));
        return stream;
      });

  internal::OptionsSpan span(TestOptions(/*expected_streams=*/1));

  auto impl = std::make_shared<DefaultRowReader>(
      mock, kAppProfile, kTableName, bigtable::RowSet(),
      bigtable::RowReader::NO_ROWS_LIMIT, bigtable::Filter::PassAllFilter(),
      retry_.clone(), backoff_.clone());
  auto reader = bigtable_internal::MakeRowReader(std::move(impl));

  auto it = reader.begin();
  EXPECT_NE(it, reader.end());
  ASSERT_STATUS_OK(*it);
  EXPECT_EQ((*it)->row_key(), "r1");
  EXPECT_EQ(++it, reader.end());
}

TEST_F(DefaultRowReaderTest, StreamIsDrained) {
  auto mock = std::make_shared<MockBigtableStub>();
  EXPECT_CALL(*mock, ReadRows)
      .WillOnce([](std::unique_ptr<grpc::ClientContext>,
                   google::bigtable::v2::ReadRowsRequest const& request) {
        EXPECT_THAT(request, HasCorrectResourceNames());
        auto stream = absl::make_unique<MockReadRowsStream>();
        ::testing::InSequence s;
        EXPECT_CALL(*stream, Read).WillOnce(Return(MakeRow("r1")));
        EXPECT_CALL(*stream, Cancel);
        EXPECT_CALL(*stream, Read)
            .WillOnce(Return(MakeRow("discarded-row")))
            .WillOnce(Return(MakeRow("discarded-row")))
            .WillOnce(Return(Status()));
        return stream;
      });

  internal::OptionsSpan span(TestOptions(/*expected_streams=*/1));

  auto impl = std::make_shared<DefaultRowReader>(
      mock, kAppProfile, kTableName, bigtable::RowSet(),
      bigtable::RowReader::NO_ROWS_LIMIT, bigtable::Filter::PassAllFilter(),
      retry_.clone(), backoff_.clone());
  auto reader = bigtable_internal::MakeRowReader(std::move(impl));

  auto it = reader.begin();
  EXPECT_NE(it, reader.end());
  ASSERT_STATUS_OK(*it);
  EXPECT_EQ((*it)->row_key(), "r1");
  EXPECT_NE(it, reader.end());
  // Do not finish the iteration.  We still expect the stream to be finalized,
  // and the previously setup expectations on the mock `stream` check that.
}

TEST_F(DefaultRowReaderTest, RetryThenSuccess) {
  auto mock = std::make_shared<MockBigtableStub>();
  EXPECT_CALL(*mock, ReadRows)
      .WillOnce([](std::unique_ptr<grpc::ClientContext>,
                   google::bigtable::v2::ReadRowsRequest const& request) {
        EXPECT_THAT(request, HasCorrectResourceNames());
        auto stream = absl::make_unique<MockReadRowsStream>();
        EXPECT_CALL(*stream, Read)
            .WillOnce(Return(Status(StatusCode::kUnavailable, "try again")));
        return stream;
      })
      .WillOnce([](std::unique_ptr<grpc::ClientContext>,
                   google::bigtable::v2::ReadRowsRequest const& request) {
        EXPECT_THAT(request, HasCorrectResourceNames());
        auto stream = absl::make_unique<MockReadRowsStream>();
        EXPECT_CALL(*stream, Read)
            .WillOnce(Return(MakeRow("r1")))
            .WillOnce(Return(Status()));
        return stream;
      });

  internal::OptionsSpan span(TestOptions(/*expected_streams=*/2));

  auto impl = std::make_shared<DefaultRowReader>(
      mock, kAppProfile, kTableName, bigtable::RowSet(),
      bigtable::RowReader::NO_ROWS_LIMIT, bigtable::Filter::PassAllFilter(),
      retry_.clone(), backoff_.clone());
  auto reader = bigtable_internal::MakeRowReader(std::move(impl));

  auto it = reader.begin();
  EXPECT_NE(it, reader.end());
  ASSERT_STATUS_OK(*it);
  EXPECT_EQ((*it)->row_key(), "r1");
  EXPECT_EQ(++it, reader.end());
}

TEST_F(DefaultRowReaderTest, NoRetryOnPermanentError) {
  auto mock = std::make_shared<MockBigtableStub>();
  EXPECT_CALL(*mock, ReadRows)
      .WillOnce([](std::unique_ptr<grpc::ClientContext>,
                   google::bigtable::v2::ReadRowsRequest const& request) {
        EXPECT_THAT(request, HasCorrectResourceNames());
        auto stream = absl::make_unique<MockReadRowsStream>();
        EXPECT_CALL(*stream, Read)
            .WillOnce(Return(Status(StatusCode::kPermissionDenied, "fail")));
        return stream;
      });

  internal::OptionsSpan span(TestOptions(/*expected_streams=*/1));

  auto impl = std::make_shared<DefaultRowReader>(
      mock, kAppProfile, kTableName, bigtable::RowSet(),
      bigtable::RowReader::NO_ROWS_LIMIT, bigtable::Filter::PassAllFilter(),
      retry_.clone(), backoff_.clone());
  auto reader = bigtable_internal::MakeRowReader(std::move(impl));

  auto it = reader.begin();
  EXPECT_NE(it, reader.end());
  EXPECT_THAT(*it, StatusIs(StatusCode::kPermissionDenied));
  EXPECT_EQ(++it, reader.end());
}

TEST_F(DefaultRowReaderTest, RetryPolicyExhausted) {
  auto mock = std::make_shared<MockBigtableStub>();
  EXPECT_CALL(*mock, ReadRows)
      .Times(kNumRetries + 1)
      .WillRepeatedly([](std::unique_ptr<grpc::ClientContext>,
                         google::bigtable::v2::ReadRowsRequest const& request) {
        EXPECT_THAT(request, HasCorrectResourceNames());
        auto stream = absl::make_unique<MockReadRowsStream>();
        EXPECT_CALL(*stream, Read)
            .WillOnce(Return(Status(StatusCode::kUnavailable, "try again")));
        return stream;
      });

  internal::OptionsSpan span(TestOptions(/*expected_streams=*/kNumRetries + 1));

  // Let's use a mock just to check that the backoff policy is used at all.
  auto backoff = absl::make_unique<MockBackoffPolicy>();
  EXPECT_CALL(*backoff, OnCompletion)
      .Times(kNumRetries)
      .WillRepeatedly(Return(ms(0)));

  auto impl = std::make_shared<DefaultRowReader>(
      mock, kAppProfile, kTableName, bigtable::RowSet(),
      bigtable::RowReader::NO_ROWS_LIMIT, bigtable::Filter::PassAllFilter(),
      retry_.clone(), std::move(backoff));
  auto reader = bigtable_internal::MakeRowReader(std::move(impl));

  auto it = reader.begin();
  EXPECT_NE(it, reader.end());
  EXPECT_THAT(*it, StatusIs(StatusCode::kUnavailable));
  EXPECT_EQ(++it, reader.end());
}

TEST_F(DefaultRowReaderTest, RetrySkipsAlreadyReadRows) {
  auto mock = std::make_shared<MockBigtableStub>();
  EXPECT_CALL(*mock, ReadRows)
      .WillOnce([](std::unique_ptr<grpc::ClientContext>,
                   google::bigtable::v2::ReadRowsRequest const& request) {
        EXPECT_THAT(request, HasCorrectResourceNames());
        // We should have two rows in the initial request: "r1" and "r2".
        EXPECT_THAT(request, RequestWithRowKeysCount(2));
        auto stream = absl::make_unique<MockReadRowsStream>();
        EXPECT_CALL(*stream, Read)
            .WillOnce(Return(MakeRow("r1")))
            .WillOnce(Return(Status(StatusCode::kUnavailable, "try again")));
        return stream;
      })
      .WillOnce([](std::unique_ptr<grpc::ClientContext>,
                   google::bigtable::v2::ReadRowsRequest const& request) {
        EXPECT_THAT(request, HasCorrectResourceNames());
        // We have read "r1". The new request should only contain: "r2".
        EXPECT_THAT(request, RequestWithRowKeysCount(1));
        auto stream = absl::make_unique<MockReadRowsStream>();
        EXPECT_CALL(*stream, Read).WillOnce(Return(Status()));
        return stream;
      });

  internal::OptionsSpan span(TestOptions(/*expected_streams=*/2));

  auto impl = std::make_shared<DefaultRowReader>(
      mock, kAppProfile, kTableName, bigtable::RowSet("r1", "r2"),
      bigtable::RowReader::NO_ROWS_LIMIT, bigtable::Filter::PassAllFilter(),
      retry_.clone(), backoff_.clone());
  auto reader = bigtable_internal::MakeRowReader(std::move(impl));

  auto it = reader.begin();
  EXPECT_NE(it, reader.end());
  ASSERT_STATUS_OK(*it);
  EXPECT_EQ((*it)->row_key(), "r1");
  EXPECT_EQ(++it, reader.end());
}

TEST_F(DefaultRowReaderTest, RetrySkipsAlreadyScannedRows) {
  auto mock = std::make_shared<MockBigtableStub>();
  EXPECT_CALL(*mock, ReadRows)
      .WillOnce([](std::unique_ptr<grpc::ClientContext>,
                   google::bigtable::v2::ReadRowsRequest const& request) {
        EXPECT_THAT(request, HasCorrectResourceNames());
        // We start our call with 3 rows in the set: "r1", "r2", "r3".
        EXPECT_THAT(request, RequestWithRowKeysCount(3));
        auto stream = absl::make_unique<MockReadRowsStream>();
        EXPECT_CALL(*stream, Read)
            .WillOnce(Return(MakeRow("r1")))
            // Simulate the server returning an empty chunk with
            // `last_scanned_row_key` set to "r2".
            .WillOnce([]() {
              google::bigtable::v2::ReadRowsResponse resp;
              resp.set_last_scanned_row_key("r2");
              return resp;
            })
            .WillOnce(Return(Status(StatusCode::kUnavailable, "try again")));
        return stream;
      })
      .WillOnce([](std::unique_ptr<grpc::ClientContext>,
                   google::bigtable::v2::ReadRowsRequest const& request) {
        EXPECT_THAT(request, HasCorrectResourceNames());
        // We retry the remaining rows. We have "r1" returned, but the service
        // has also told us that "r2" was scanned. This means there is only one
        // row remaining to read: "r3".
        EXPECT_THAT(request, RequestWithRowKeysCount(1));
        auto stream = absl::make_unique<MockReadRowsStream>();
        EXPECT_CALL(*stream, Read).WillOnce(Return(Status()));
        return stream;
      });

  internal::OptionsSpan span(TestOptions(/*expected_streams=*/2));

  auto impl = std::make_shared<DefaultRowReader>(
      mock, kAppProfile, kTableName, bigtable::RowSet("r1", "r2", "r3"),
      bigtable::RowReader::NO_ROWS_LIMIT, bigtable::Filter::PassAllFilter(),
      retry_.clone(), backoff_.clone());
  auto reader = bigtable_internal::MakeRowReader(std::move(impl));

  auto it = reader.begin();
  EXPECT_NE(it, reader.end());
  ASSERT_STATUS_OK(*it);
  EXPECT_EQ((*it)->row_key(), "r1");
  EXPECT_EQ(++it, reader.end());
}

TEST_F(DefaultRowReaderTest, FailedParseIsRetried) {
  auto mock = std::make_shared<MockBigtableStub>();
  EXPECT_CALL(*mock, ReadRows)
      .WillOnce([](std::unique_ptr<grpc::ClientContext>,
                   google::bigtable::v2::ReadRowsRequest const& request) {
        EXPECT_THAT(request, HasCorrectResourceNames());
        auto stream = absl::make_unique<MockReadRowsStream>();
        EXPECT_CALL(*stream, Read).WillOnce(Return(MalformedResponse()));
        return stream;
      })
      .WillOnce([](std::unique_ptr<grpc::ClientContext>,
                   google::bigtable::v2::ReadRowsRequest const& request) {
        EXPECT_THAT(request, HasCorrectResourceNames());
        auto stream = absl::make_unique<MockReadRowsStream>();
        EXPECT_CALL(*stream, Read)
            .WillOnce(Return(MakeRow("r1")))
            .WillOnce(Return(Status()));
        return stream;
      });

  internal::OptionsSpan span(TestOptions(/*expected_streams=*/2));

  // The parser only returns INTERNAL errors. Our default policies do not retry
  // on this StatusCode. We will use a mock policy to override this behavior.
  auto retry = absl::make_unique<MockDataRetryPolicy>();
  EXPECT_CALL(*retry, OnFailure).WillOnce(Return(true));

  auto impl = std::make_shared<DefaultRowReader>(
      mock, kAppProfile, kTableName, bigtable::RowSet(),
      bigtable::RowReader::NO_ROWS_LIMIT, bigtable::Filter::PassAllFilter(),
      std::move(retry), backoff_.clone());
  auto reader = bigtable_internal::MakeRowReader(std::move(impl));

  auto it = reader.begin();
  EXPECT_NE(it, reader.end());
  ASSERT_STATUS_OK(*it);
  EXPECT_EQ((*it)->row_key(), "r1");
  EXPECT_EQ(++it, reader.end());
}

TEST_F(DefaultRowReaderTest, FailedParseSkipsAlreadyReadRows) {
  auto mock = std::make_shared<MockBigtableStub>();
  EXPECT_CALL(*mock, ReadRows)
      .WillOnce([](std::unique_ptr<grpc::ClientContext>,
                   google::bigtable::v2::ReadRowsRequest const& request) {
        EXPECT_THAT(request, HasCorrectResourceNames());
        // We start our call with 3 rows in the set: "r1", "r2", "r3".
        EXPECT_THAT(request, RequestWithRowKeysCount(3));
        auto stream = absl::make_unique<MockReadRowsStream>();
        EXPECT_CALL(*stream, Read)
            .WillOnce(Return(MakeRow("r1")))
            // Simulate the server returning an empty chunk with
            // `last_scanned_row_key` set to "r2".
            .WillOnce([]() {
              google::bigtable::v2::ReadRowsResponse resp;
              resp.set_last_scanned_row_key("r2");
              return resp;
            })
            .WillOnce(Return(MalformedResponse()));
        return stream;
      })
      .WillOnce([](std::unique_ptr<grpc::ClientContext>,
                   google::bigtable::v2::ReadRowsRequest const& request) {
        EXPECT_THAT(request, HasCorrectResourceNames());
        // We retry the remaining rows. We have "r1" returned, but the service
        // has also told us that "r2" was scanned. This means there is only one
        // row remaining to read: "r3".
        EXPECT_THAT(request, RequestWithRowKeysCount(1));
        auto stream = absl::make_unique<MockReadRowsStream>();
        EXPECT_CALL(*stream, Read).WillOnce(Return(Status()));
        return stream;
      });

  internal::OptionsSpan span(TestOptions(/*expected_streams=*/2));

  // The parser only returns INTERNAL errors. Our default policies do not retry
  // on this StatusCode. We will use a mock policy to override this behavior.
  auto retry = absl::make_unique<MockDataRetryPolicy>();
  EXPECT_CALL(*retry, OnFailure).WillOnce(Return(true));

  auto impl = std::make_shared<DefaultRowReader>(
      mock, kAppProfile, kTableName, bigtable::RowSet("r1", "r2", "r3"),
      bigtable::RowReader::NO_ROWS_LIMIT, bigtable::Filter::PassAllFilter(),
      std::move(retry), backoff_.clone());
  auto reader = bigtable_internal::MakeRowReader(std::move(impl));

  auto it = reader.begin();
  EXPECT_NE(it, reader.end());
  ASSERT_STATUS_OK(*it);
  EXPECT_EQ((*it)->row_key(), "r1");
  EXPECT_EQ(++it, reader.end());
}

TEST_F(DefaultRowReaderTest, FailedParseSkipsAlreadyScannedRows) {}

TEST_F(DefaultRowReaderTest, FailedParseWithPermanentError) {
  auto mock = std::make_shared<MockBigtableStub>();
  EXPECT_CALL(*mock, ReadRows)
      .WillOnce([](std::unique_ptr<grpc::ClientContext>,
                   google::bigtable::v2::ReadRowsRequest const& request) {
        EXPECT_THAT(request, HasCorrectResourceNames());
        auto stream = absl::make_unique<MockReadRowsStream>();
        ::testing::InSequence s;
        EXPECT_CALL(*stream, Read).WillOnce(Return(MalformedResponse()));
        // The stream is cancelled when the RowReader goes out of scope.
        EXPECT_CALL(*stream, Cancel);
        EXPECT_CALL(*stream, Read).WillOnce(Return(Status()));
        return stream;
      });

  internal::OptionsSpan span(TestOptions(/*expected_streams=*/1));

  auto impl = std::make_shared<DefaultRowReader>(
      mock, kAppProfile, kTableName, bigtable::RowSet(),
      bigtable::RowReader::NO_ROWS_LIMIT, bigtable::Filter::PassAllFilter(),
      retry_.clone(), backoff_.clone());
  auto reader = bigtable_internal::MakeRowReader(std::move(impl));

  auto it = reader.begin();
  EXPECT_NE(it, reader.end());
  EXPECT_THAT(*it, StatusIs(StatusCode::kInternal));
  EXPECT_EQ(++it, reader.end());
}

TEST_F(DefaultRowReaderTest, NoRetryOnEmptyRowSet) {
  auto mock = std::make_shared<MockBigtableStub>();
  EXPECT_CALL(*mock, ReadRows)
      .WillOnce([](std::unique_ptr<grpc::ClientContext>,
                   google::bigtable::v2::ReadRowsRequest const& request) {
        EXPECT_THAT(request, HasCorrectResourceNames());
        auto stream = absl::make_unique<MockReadRowsStream>();
        EXPECT_CALL(*stream, Read)
            .WillOnce(Return(MakeRow("r2")))
            .WillOnce(Return(Status(StatusCode::kUnavailable, "try again")));
        return stream;
      });
  // After receiving "r2", the row set will be empty. So even though we
  // encountered a transient error, there is no need to retry the stream.

  internal::OptionsSpan span(TestOptions(/*expected_streams=*/1));

  auto impl = std::make_shared<DefaultRowReader>(
      mock, kAppProfile, kTableName, bigtable::RowSet("r1", "r2"),
      bigtable::RowReader::NO_ROWS_LIMIT, bigtable::Filter::PassAllFilter(),
      retry_.clone(), backoff_.clone());
  auto reader = bigtable_internal::MakeRowReader(std::move(impl));

  auto it = reader.begin();
  EXPECT_NE(it, reader.end());
  ASSERT_STATUS_OK(*it);
  EXPECT_EQ((*it)->row_key(), "r2");
  EXPECT_EQ(++it, reader.end());
}

TEST_F(DefaultRowReaderTest, RowLimitIsSent) {
  auto mock = std::make_shared<MockBigtableStub>();
  EXPECT_CALL(*mock, ReadRows)
      .WillOnce([](std::unique_ptr<grpc::ClientContext>,
                   google::bigtable::v2::ReadRowsRequest const& request) {
        EXPECT_THAT(request, HasCorrectResourceNames());
        EXPECT_THAT(request, RequestWithRowsLimit(42));
        auto stream = absl::make_unique<MockReadRowsStream>();
        EXPECT_CALL(*stream, Read).WillOnce(Return(Status()));
        return stream;
      });

  internal::OptionsSpan span(TestOptions(/*expected_streams=*/1));

  auto impl = std::make_shared<DefaultRowReader>(
      mock, kAppProfile, kTableName, bigtable::RowSet(), 42,
      bigtable::Filter::PassAllFilter(), retry_.clone(), backoff_.clone());
  auto reader = bigtable_internal::MakeRowReader(std::move(impl));

  auto it = reader.begin();
  EXPECT_EQ(it, reader.end());
}

TEST_F(DefaultRowReaderTest, RowLimitIsDecreasedOnRetry) {
  auto mock = std::make_shared<MockBigtableStub>();
  EXPECT_CALL(*mock, ReadRows)
      .WillOnce([](std::unique_ptr<grpc::ClientContext>,
                   google::bigtable::v2::ReadRowsRequest const& request) {
        EXPECT_THAT(request, HasCorrectResourceNames());
        EXPECT_THAT(request, RequestWithRowsLimit(42));
        auto stream = absl::make_unique<MockReadRowsStream>();
        EXPECT_CALL(*stream, Read)
            .WillOnce(Return(MakeRow("r1")))
            .WillOnce(Return(Status(StatusCode::kUnavailable, "try again")));
        return stream;
      })
      .WillOnce([](std::unique_ptr<grpc::ClientContext>,
                   google::bigtable::v2::ReadRowsRequest const& request) {
        EXPECT_THAT(request, HasCorrectResourceNames());
        EXPECT_THAT(request, RequestWithRowsLimit(41));
        auto stream = absl::make_unique<MockReadRowsStream>();
        EXPECT_CALL(*stream, Read).WillOnce(Return(Status()));
        return stream;
      });

  internal::OptionsSpan span(TestOptions(/*expected_streams=*/2));

  auto impl = std::make_shared<DefaultRowReader>(
      mock, kAppProfile, kTableName, bigtable::RowSet(), 42,
      bigtable::Filter::PassAllFilter(), retry_.clone(), backoff_.clone());
  auto reader = bigtable_internal::MakeRowReader(std::move(impl));

  auto it = reader.begin();
  EXPECT_NE(it, reader.end());
  ASSERT_STATUS_OK(*it);
  EXPECT_EQ((*it)->row_key(), "r1");
  EXPECT_EQ(++it, reader.end());
}

TEST_F(DefaultRowReaderTest, NoRetryIfRowLimitReached) {
  auto mock = std::make_shared<MockBigtableStub>();
  EXPECT_CALL(*mock, ReadRows)
      .WillOnce([](std::unique_ptr<grpc::ClientContext>,
                   google::bigtable::v2::ReadRowsRequest const& request) {
        EXPECT_THAT(request, HasCorrectResourceNames());
        EXPECT_THAT(request, RequestWithRowsLimit(1));
        auto stream = absl::make_unique<MockReadRowsStream>();
        EXPECT_CALL(*stream, Read)
            .WillOnce(Return(MakeRow("r1")))
            .WillOnce(Return(Status(StatusCode::kUnavailable, "try again")));
        return stream;
      });
  // After receiving "r2", the row set will be empty. So even though we
  // encountered a transient error, there is no need to retry the stream.

  internal::OptionsSpan span(TestOptions(/*expected_streams=*/1));

  auto impl = std::make_shared<DefaultRowReader>(
      mock, kAppProfile, kTableName, bigtable::RowSet(), 1,
      bigtable::Filter::PassAllFilter(), retry_.clone(), backoff_.clone());
  auto reader = bigtable_internal::MakeRowReader(std::move(impl));

  auto it = reader.begin();
  EXPECT_NE(it, reader.end());
  ASSERT_STATUS_OK(*it);
  EXPECT_EQ((*it)->row_key(), "r1");
  EXPECT_EQ(++it, reader.end());
}

TEST_F(DefaultRowReaderTest, CancelDrainsStream) {
  auto mock = std::make_shared<MockBigtableStub>();
  EXPECT_CALL(*mock, ReadRows)
      .WillOnce([](std::unique_ptr<grpc::ClientContext>,
                   google::bigtable::v2::ReadRowsRequest const& request) {
        EXPECT_THAT(request, HasCorrectResourceNames());
        auto stream = absl::make_unique<MockReadRowsStream>();
        ::testing::InSequence s;
        EXPECT_CALL(*stream, Read).WillOnce(Return(MakeRow("r1")));
        EXPECT_CALL(*stream, Cancel);
        EXPECT_CALL(*stream, Read)
            .WillOnce(Return(MakeRow("discarded-row")))
            .WillOnce(Return(MakeRow("discarded-row")))
            .WillOnce(Return(Status()));
        return stream;
      });

  internal::OptionsSpan span(TestOptions(/*expected_streams=*/1));

  auto impl = std::make_shared<DefaultRowReader>(
      mock, kAppProfile, kTableName, bigtable::RowSet(),
      bigtable::RowReader::NO_ROWS_LIMIT, bigtable::Filter::PassAllFilter(),
      retry_.clone(), backoff_.clone());
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
  EXPECT_THAT(*it, StatusIs(StatusCode::kCancelled));
  EXPECT_EQ(++it, reader.end());
}

TEST_F(DefaultRowReaderTest, CancelBeforeBegin) {
  auto mock = std::make_shared<MockBigtableStub>();
  EXPECT_CALL(*mock, ReadRows).Times(0);

  internal::OptionsSpan span(TestOptions(/*expected_streams=*/0));

  auto impl = std::make_shared<DefaultRowReader>(
      mock, kAppProfile, kTableName, bigtable::RowSet(),
      bigtable::RowReader::NO_ROWS_LIMIT, bigtable::Filter::PassAllFilter(),
      retry_.clone(), backoff_.clone());
  auto reader = bigtable_internal::MakeRowReader(std::move(impl));

  // Manually cancel the call before a stream was created.
  reader.Cancel();
  reader.begin();

  auto it = reader.begin();
  EXPECT_NE(it, reader.end());
  EXPECT_THAT(*it, StatusIs(StatusCode::kCancelled));
  EXPECT_EQ(++it, reader.end());
}

TEST_F(DefaultRowReaderTest, RowReaderConstructorDoesNotCallRpc) {
  // The RowReader constructor/destructor by themselves should not
  // invoke the RPC or create parsers (the latter restriction because
  // parsers are per-connection and non-reusable).
  auto mock = std::make_shared<MockBigtableStub>();
  EXPECT_CALL(*mock, ReadRows).Times(0);

  internal::OptionsSpan span(TestOptions(/*expected_streams=*/0));

  auto impl = std::make_shared<DefaultRowReader>(
      mock, kAppProfile, kTableName, bigtable::RowSet(),
      bigtable::RowReader::NO_ROWS_LIMIT, bigtable::Filter::PassAllFilter(),
      retry_.clone(), backoff_.clone());
  auto reader = bigtable_internal::MakeRowReader(std::move(impl));
}

TEST_F(DefaultRowReaderTest, RetryUsesNewContext) {
  auto mock = std::make_shared<MockBigtableStub>();
  EXPECT_CALL(*mock, ReadRows)
      .Times(kNumRetries + 1)
      .WillRepeatedly([](std::unique_ptr<grpc::ClientContext> context,
                         google::bigtable::v2::ReadRowsRequest const& request) {
        // This is a hack. A new request will have the default compression
        // algorithm (GRPC_COMPRESS_NONE). We then change the value in this
        // call. If the context is reused, it will no longer have the default
        // value.
        EXPECT_EQ(GRPC_COMPRESS_NONE, context->compression_algorithm());
        context->set_compression_algorithm(GRPC_COMPRESS_GZIP);

        EXPECT_THAT(request, HasCorrectResourceNames());
        auto stream = absl::make_unique<MockReadRowsStream>();
        EXPECT_CALL(*stream, Read)
            .WillOnce(Return(Status(StatusCode::kUnavailable, "try again")));
        return stream;
      });

  internal::OptionsSpan span(TestOptions(/*expected_streams=*/kNumRetries + 1));

  auto impl = std::make_shared<DefaultRowReader>(
      mock, kAppProfile, kTableName, bigtable::RowSet(),
      bigtable::RowReader::NO_ROWS_LIMIT, bigtable::Filter::PassAllFilter(),
      retry_.clone(), backoff_.clone());
  auto reader = bigtable_internal::MakeRowReader(std::move(impl));

  auto it = reader.begin();
  EXPECT_NE(it, reader.end());
  EXPECT_THAT(*it, StatusIs(StatusCode::kUnavailable));
  EXPECT_EQ(++it, reader.end());
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable_internal
}  // namespace cloud
}  // namespace google
