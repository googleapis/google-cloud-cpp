// Copyright 2018 Google LLC
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

#include "google/cloud/storage/client.h"
#include "google/cloud/storage/internal/object_metadata_parser.h"
#include "google/cloud/storage/oauth2/google_credentials.h"
#include "google/cloud/storage/retry_policy.h"
#include "google/cloud/storage/testing/canonical_errors.h"
#include "google/cloud/storage/testing/client_unit_test.h"
#include "google/cloud/storage/testing/mock_client.h"
#include "google/cloud/storage/testing/random_names.h"
#include "google/cloud/storage/testing/temp_file.h"
#include "google/cloud/testing_util/status_matchers.h"
#include "absl/memory/memory.h"
#include <gmock/gmock.h>
#include <fstream>

namespace google {
namespace cloud {
namespace storage {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::google::cloud::storage::testing::canonical_errors::PermanentError;
using ::google::cloud::storage::testing::canonical_errors::TransientError;
using ::google::cloud::testing_util::StatusIs;
using ::testing::Return;
using ::testing::ReturnRef;
using ms = std::chrono::milliseconds;

/**
 * Test the functions in Storage::Client related to writing objects.
 */
class WriteObjectTest
    : public ::google::cloud::storage::testing::ClientUnitTest {};

TEST_F(WriteObjectTest, WriteObject) {
  std::string text = R"""({
      "name": "test-bucket-name/test-object-name/1"
})""";
  auto expected = internal::ObjectMetadataParser::FromString(text).value();

  EXPECT_CALL(*mock_, CreateResumableSession)
      .WillOnce([&expected](internal::ResumableUploadRequest const& request) {
        EXPECT_EQ("test-bucket-name", request.bucket_name());
        EXPECT_EQ("test-object-name", request.object_name());

        auto mock = absl::make_unique<testing::MockResumableUploadSession>();
        using internal::ResumableUploadResponse;
        EXPECT_CALL(*mock, done()).WillRepeatedly(Return(false));
        EXPECT_CALL(*mock, next_expected_byte()).WillRepeatedly(Return(0));
        EXPECT_CALL(*mock, UploadChunk)
            .WillRepeatedly(Return(make_status_or(ResumableUploadResponse{
                "fake-url", ResumableUploadResponse::kInProgress, 0, {}, {}})));
        EXPECT_CALL(*mock, ResetSession())
            .WillOnce(Return(make_status_or(ResumableUploadResponse{
                "fake-url", ResumableUploadResponse::kInProgress, 0, {}, {}})));
        EXPECT_CALL(*mock, UploadFinalChunk)
            .WillOnce(
                Return(StatusOr<ResumableUploadResponse>(TransientError())))
            .WillOnce(Return(make_status_or(ResumableUploadResponse{
                "fake-url", ResumableUploadResponse::kDone, 0, expected, {}})));

        return make_status_or(internal::CreateResumableSessionResponse{
            std::move(mock),
            ResumableUploadResponse{"fake-url",
                                    ResumableUploadResponse::kInProgress,
                                    /*committed_size=*/absl::nullopt,
                                    /*object_metadata=*/absl::nullopt,
                                    /*.annotations=*/std::string{}}});
      });

  auto client = ClientForMock();
  auto stream = client.WriteObject("test-bucket-name", "test-object-name");
  stream << "Hello World!";
  stream.Close();
  ObjectMetadata actual = stream.metadata().value();
  EXPECT_EQ(expected, actual);
}

TEST_F(WriteObjectTest, WriteObjectTooManyFailures) {
  auto client = testing::ClientFromMock(
      mock_, LimitedErrorCountRetryPolicy(2),
      ExponentialBackoffPolicy(std::chrono::milliseconds(1),
                               std::chrono::milliseconds(1), 2.0));

  auto returner = [](internal::ResumableUploadRequest const&) {
    return StatusOr<internal::CreateResumableSessionResponse>(TransientError());
  };
  EXPECT_CALL(*mock_, CreateResumableSession)
      .WillOnce(returner)
      .WillOnce(returner)
      .WillOnce(returner);

  auto stream = client.WriteObject("test-bucket-name", "test-object-name");
  EXPECT_TRUE(stream.bad());
  EXPECT_THAT(stream.metadata(), StatusIs(TransientError().code()));
}

TEST_F(WriteObjectTest, WriteObjectPermanentFailure) {
  auto returner = [](internal::ResumableUploadRequest const&) {
    return StatusOr<internal::CreateResumableSessionResponse>(PermanentError());
  };
  EXPECT_CALL(*mock_, CreateResumableSession).WillOnce(returner);
  auto client = ClientForMock();
  auto stream = client.WriteObject("test-bucket-name", "test-object-name");
  EXPECT_TRUE(stream.bad());
  EXPECT_THAT(stream.metadata(), StatusIs(PermanentError().code()));
}

TEST_F(WriteObjectTest, WriteObjectErrorInChunk) {
  auto const session_id = std::string{"test-session-id"};
  EXPECT_CALL(*mock_, CreateResumableSession)
      .WillOnce([&](internal::ResumableUploadRequest const& request) {
        EXPECT_EQ("test-bucket-name", request.bucket_name());
        EXPECT_EQ("test-object-name", request.object_name());

        auto mock = absl::make_unique<testing::MockResumableUploadSession>();
        using internal::ResumableUploadResponse;
        EXPECT_CALL(*mock, done()).WillRepeatedly(Return(false));
        EXPECT_CALL(*mock, next_expected_byte()).WillRepeatedly(Return(0));
        EXPECT_CALL(*mock, UploadChunk)
            .WillOnce(Return(Status(StatusCode::kDataLoss, "ooops")));
        EXPECT_CALL(*mock, session_id()).WillRepeatedly(ReturnRef(session_id));
        // The call to Close() below should have no effect and no "final" chunk
        // should be uploaded.
        EXPECT_CALL(*mock, UploadFinalChunk).Times(0);

        return make_status_or(internal::CreateResumableSessionResponse{
            std::move(mock),
            ResumableUploadResponse{"fake-url",
                                    ResumableUploadResponse::kInProgress,
                                    /*committed_size=*/absl::nullopt,
                                    /*object_metadata=*/absl::nullopt,
                                    /*.annotations=*/std::string{}}});
      });
  auto constexpr kQuantum = internal::UploadChunkRequest::kChunkSizeQuantum;
  client_options_.SetUploadBufferSize(kQuantum);
  auto client = ClientForMock();
  auto stream = client.WriteObject("test-bucket-name", "test-object-name",
                                   IfGenerationMatch(0));
  auto const data = std::string(2 * kQuantum, 'A');
  auto const size = static_cast<std::streamsize>(data.size());
  // The stream is set up to flush for buffers of `data`'s size. This triggers
  // the UploadChunk() mock, which returns an error. Because this is a permanent
  // error, no further upload attempts are made.
  stream.write(data.data(), size);
  EXPECT_TRUE(stream.bad());
  EXPECT_THAT(stream.last_status(), StatusIs(StatusCode::kDataLoss));
  stream.write(data.data(), size);
  EXPECT_TRUE(stream.bad());
  EXPECT_THAT(stream.last_status(), StatusIs(StatusCode::kDataLoss));
  EXPECT_THAT(stream.metadata(), StatusIs(StatusCode::kUnknown));
  stream.Close();
  EXPECT_THAT(stream.metadata(), StatusIs(StatusCode::kDataLoss));
}

TEST_F(WriteObjectTest, WriteObjectPermanentSessionFailurePropagates) {
  std::string const empty;
  auto create_mock = [&](internal::ResumableUploadRequest const&) {
    auto mock = absl::make_unique<testing::MockResumableUploadSession>();
    EXPECT_CALL(*mock, UploadChunk).WillRepeatedly(Return(PermanentError()));
    EXPECT_CALL(*mock, next_expected_byte()).WillRepeatedly(Return(0));
    EXPECT_CALL(*mock, done()).WillRepeatedly(Return(false));
    EXPECT_CALL(*mock, session_id()).WillRepeatedly(ReturnRef(empty));

    return make_status_or(internal::CreateResumableSessionResponse{
        std::move(mock),
        internal::ResumableUploadResponse{
            "fake-url", internal::ResumableUploadResponse::kInProgress,
            /*committed_size=*/absl::nullopt,
            /*object_metadata=*/absl::nullopt,
            /*.annotations=*/std::string{}}});
  };

  EXPECT_CALL(*mock_, CreateResumableSession).WillOnce(create_mock);
  auto client = ClientForMock();
  auto stream = client.WriteObject("test-bucket-name", "test-object-name");

  // make sure it is actually sent
  std::vector<char> data(client_options_.upload_buffer_size() + 1, 'X');
  stream.write(data.data(), data.size());
  EXPECT_TRUE(stream.bad());
  stream.Close();
  EXPECT_THAT(stream.metadata(), StatusIs(PermanentError().code()));
}

// A std::filebuf which allows to learn about seekpos calls.
class MockFilebuf : public std::filebuf {
 public:
  MOCK_METHOD(void, SeekoffEvent, (std::streamoff));

 protected:
  std::streampos seekoff(std::streamoff off, std::ios_base::seekdir way,
                         std::ios_base::openmode which) override {
    SeekoffEvent(off);
    return std::filebuf::seekoff(off, way, which);
  }
};

TEST_F(WriteObjectTest, UploadStreamResumable) {
  auto const quantum = internal::UploadChunkRequest::kChunkSizeQuantum;
  auto rng = google::cloud::internal::MakeDefaultPRNG();
  google::cloud::storage::testing::TempFile temp_file(
      google::cloud::storage::testing::MakeRandomData(rng, 2 * quantum + 10));

  std::string text = R"""({
      "name": "test-bucket-name/test-object-name/1"
})""";
  auto expected = internal::ObjectMetadataParser::FromString(text).value();

  // Simulate situation when a quantum has already been uploaded.
  std::size_t bytes_written = quantum;
  EXPECT_CALL(*mock_, CreateResumableSession)
      .WillOnce([&](internal::ResumableUploadRequest const& request) {
        EXPECT_EQ("test-bucket-name", request.bucket_name());
        EXPECT_EQ("test-object-name", request.object_name());

        auto mock = absl::make_unique<testing::MockResumableUploadSession>();
        using internal::ResumableUploadResponse;
        EXPECT_CALL(*mock, done()).WillRepeatedly(Return(false));
        EXPECT_CALL(*mock, next_expected_byte())
            .WillRepeatedly([&bytes_written]() { return bytes_written; });

        EXPECT_CALL(*mock, UploadFinalChunk)
            .WillOnce([expected, &bytes_written](
                          internal::ConstBufferSequence const& data,
                          std::uint64_t size, internal::HashValues const&) {
              bytes_written += internal::TotalBytes(data);
              EXPECT_EQ(bytes_written, size);
              return make_status_or(
                  ResumableUploadResponse{"fake-url",
                                          ResumableUploadResponse::kDone,
                                          bytes_written,
                                          expected,
                                          {}});
            });

        return make_status_or(internal::CreateResumableSessionResponse{
            std::move(mock),
            internal::ResumableUploadResponse{
                "fake-url", internal::ResumableUploadResponse::kInProgress,
                /*.committed_size=*/quantum,
                /*.object_metadata=*/absl::nullopt,
                /*.annotations=*/std::string{}}});
      });

  MockFilebuf filebuf;
  // Don't expect any seekoff events
  EXPECT_CALL(filebuf, SeekoffEvent).WillOnce([quantum](std::streamoff off) {
    EXPECT_EQ(quantum, off);
  });
  ASSERT_NE(nullptr, filebuf.open(temp_file.name().c_str(), std::ios_base::in));
  std::istream stream(&filebuf);

  ASSERT_TRUE(stream);
  auto client = ClientForMock();
  auto res = internal::ClientImplDetails::UploadStreamResumable(
      client, stream,
      internal::ResumableUploadRequest("test-bucket-name", "test-object-name"));
  ASSERT_STATUS_OK(res);
  EXPECT_EQ(expected, *res);
}

TEST_F(WriteObjectTest, UploadFile) {
  auto rng = google::cloud::internal::MakeDefaultPRNG();
  auto const quantum = internal::UploadChunkRequest::kChunkSizeQuantum;
  auto const initial_size = quantum;
  auto const file_size = initial_size + 10;
  google::cloud::storage::testing::TempFile temp_file(
      google::cloud::storage::testing::MakeRandomData(rng, file_size));

  std::string text = R"""({
      "name": "test-bucket-name/test-object-name/1"
})""";
  auto expected = internal::ObjectMetadataParser::FromString(text).value();

  // Simulate situation when a quantum has already been uploaded.
  std::uint64_t bytes_written = initial_size;
  EXPECT_CALL(*mock_, CreateResumableSession)
      .WillOnce([&](internal::ResumableUploadRequest const& request) {
        EXPECT_TRUE(request.HasOption<UploadContentLength>());
        EXPECT_EQ(file_size, request.GetOption<UploadContentLength>().value());
        EXPECT_EQ("test-bucket-name", request.bucket_name());
        EXPECT_EQ("test-object-name", request.object_name());

        auto mock = absl::make_unique<testing::MockResumableUploadSession>();
        using internal::ResumableUploadResponse;
        EXPECT_CALL(*mock, done()).WillRepeatedly(Return(false));
        EXPECT_CALL(*mock, next_expected_byte())
            .WillRepeatedly([&bytes_written]() { return bytes_written; });
        EXPECT_CALL(*mock, UploadFinalChunk)
            .WillOnce([&](internal::ConstBufferSequence const& data,
                          std::uint64_t size, internal::HashValues const&) {
              auto const data_size = internal::TotalBytes(data);
              EXPECT_EQ(size, file_size);
              EXPECT_EQ(data_size, file_size - initial_size);
              bytes_written += data_size;
              return make_status_or(
                  ResumableUploadResponse{"fake-url",
                                          ResumableUploadResponse::kDone,
                                          bytes_written,
                                          expected,
                                          {}});
            });

        return make_status_or(internal::CreateResumableSessionResponse{
            std::move(mock),
            internal::ResumableUploadResponse{
                "fake-url", internal::ResumableUploadResponse::kInProgress,
                /*committed_size=*/initial_size,
                /*object_metadata=*/absl::nullopt,
                /*.annotations=*/std::string{}}});
      });

  auto client = ClientForMock();
  auto res = client.UploadFile(temp_file.name(), "test-bucket-name",
                               "test-object-name", UseResumableUploadSession());
  ASSERT_STATUS_OK(res);
  EXPECT_EQ(expected, *res);
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google
