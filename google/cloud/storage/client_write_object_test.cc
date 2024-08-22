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
#include <gmock/gmock.h>
#include <fstream>
#include <string>
#include <vector>

namespace google {
namespace cloud {
namespace storage {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::google::cloud::storage::testing::canonical_errors::PermanentError;
using ::google::cloud::storage::testing::canonical_errors::TransientError;
using ::google::cloud::testing_util::StatusIs;
using ::testing::Return;
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

  ::testing::InSequence sequence;
  EXPECT_CALL(*mock_, CreateResumableUpload).WillOnce(Return(TransientError()));
  EXPECT_CALL(*mock_, CreateResumableUpload)
      .WillOnce([&](internal::ResumableUploadRequest const& request) {
        EXPECT_EQ("test-bucket-name", request.bucket_name());
        EXPECT_EQ("test-object-name", request.object_name());
        return internal::CreateResumableUploadResponse{"test-only-upload-id"};
      });
  EXPECT_CALL(*mock_, UploadChunk).WillOnce(Return(TransientError()));
  EXPECT_CALL(*mock_, QueryResumableUpload).WillOnce(Return(TransientError()));
  EXPECT_CALL(*mock_, QueryResumableUpload)
      .WillOnce(Return(
          internal::QueryResumableUploadResponse{absl::nullopt, expected}));
  EXPECT_CALL(*mock_, UploadChunk)
      .WillRepeatedly(Return(
          internal::QueryResumableUploadResponse{absl::nullopt, expected}));

  auto client = ClientForMock();
  auto stream = client.WriteObject("test-bucket-name", "test-object-name");
  stream << "Hello World!";
  stream.Close();
  ObjectMetadata actual = stream.metadata().value();
  EXPECT_EQ(expected, actual);
}

TEST_F(WriteObjectTest, WriteObjectAutoFinalize) {
  std::string text = R"""({
      "name": "test-bucket-name/test-object-name/1"
})""";
  auto expected = internal::ObjectMetadataParser::FromString(text).value();

  ::testing::InSequence sequence;
  EXPECT_CALL(*mock_, CreateResumableUpload).WillOnce(Return(TransientError()));
  EXPECT_CALL(*mock_, CreateResumableUpload)
      .WillOnce([&](internal::ResumableUploadRequest const& request) {
        EXPECT_EQ("test-bucket-name", request.bucket_name());
        EXPECT_EQ("test-object-name", request.object_name());
        return internal::CreateResumableUploadResponse{"test-only-upload-id"};
      });
  EXPECT_CALL(*mock_, UploadChunk).WillOnce(Return(TransientError()));
  EXPECT_CALL(*mock_, QueryResumableUpload).WillOnce(Return(TransientError()));
  EXPECT_CALL(*mock_, QueryResumableUpload)
      .WillOnce(Return(
          internal::QueryResumableUploadResponse{absl::nullopt, expected}));
  EXPECT_CALL(*mock_, UploadChunk)
      .WillRepeatedly(Return(
          internal::QueryResumableUploadResponse{absl::nullopt, expected}));

  auto client = ClientForMock();
  {
    auto stream = client.WriteObject("test-bucket-name", "test-object-name");
    stream << "Hello World!";
  }
}

TEST_F(WriteObjectTest, WriteObjectTooManyFailures) {
  auto client = testing::ClientFromMock(
      mock_, LimitedErrorCountRetryPolicy(2),
      ExponentialBackoffPolicy(std::chrono::milliseconds(1),
                               std::chrono::milliseconds(1), 2.0));

  EXPECT_CALL(*mock_, CreateResumableUpload)
      .Times(3)
      .WillRepeatedly(Return(TransientError()));

  auto stream = client.WriteObject("test-bucket-name", "test-object-name");
  EXPECT_TRUE(stream.bad());
  EXPECT_THAT(stream.metadata(), StatusIs(TransientError().code()));
}

TEST_F(WriteObjectTest, WriteObjectPermanentFailure) {
  EXPECT_CALL(*mock_, CreateResumableUpload).WillOnce(Return(PermanentError()));
  auto client = ClientForMock();
  auto stream = client.WriteObject("test-bucket-name", "test-object-name");
  EXPECT_TRUE(stream.bad());
  EXPECT_THAT(stream.metadata(), StatusIs(PermanentError().code()));
}

TEST_F(WriteObjectTest, WriteObjectErrorInChunk) {
  EXPECT_CALL(*mock_, CreateResumableUpload)
      .WillOnce([&](internal::ResumableUploadRequest const& request) {
        EXPECT_EQ("test-bucket-name", request.bucket_name());
        EXPECT_EQ("test-object-name", request.object_name());
        return internal::CreateResumableUploadResponse{"test-session-id"};
      });
  EXPECT_CALL(*mock_, UploadChunk)
      .WillOnce(Return(Status(StatusCode::kDataLoss, "ooops")));

  auto constexpr kQuantum = internal::UploadChunkRequest::kChunkSizeQuantum;
  auto client = ClientForMock();
  auto stream = client.WriteObject(
      "test-bucket-name", "test-object-name", IfGenerationMatch(0),
      Options{}.set<UploadBufferSizeOption>(kQuantum));
  auto const data = std::string(2 * kQuantum, 'A');
  auto const size = static_cast<std::streamsize>(data.size());
  // The stream is set up to flush for buffers of `data`'s size. This triggers
  // the UploadChunk() mock, which returns an error. Because this is a permanent
  // error, no further upload attempts are made.
  stream.write(data.data(), size);
  stream.flush();
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
  EXPECT_CALL(*mock_, CreateResumableUpload)
      .WillOnce(Return(
          internal::CreateResumableUploadResponse{"test-only-upload-id"}));
  EXPECT_CALL(*mock_, UploadChunk).WillOnce(Return(PermanentError()));
  auto client = ClientForMock();
  auto stream = client.WriteObject("test-bucket-name", "test-object-name");

  // make sure it is actually sent
  std::vector<char> data(mock_->options().get<UploadBufferSizeOption>() + 1,
                         'X');
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
  auto rng = google::cloud::internal::DefaultPRNG(std::random_device{}());
  auto const quantum = internal::UploadChunkRequest::kChunkSizeQuantum;
  google::cloud::storage::testing::TempFile temp_file(
      google::cloud::storage::testing::MakeRandomData(rng, 2 * quantum + 10));

  std::string text = R"""({
      "name": "test-bucket-name/test-object-name/1"
})""";
  auto expected = internal::ObjectMetadataParser::FromString(text).value();

  EXPECT_CALL(*mock_, QueryResumableUpload)
      .WillOnce([&](internal::QueryResumableUploadRequest const& request) {
        EXPECT_EQ("test-only-upload-id", request.upload_session_url());
        return internal::QueryResumableUploadResponse{quantum, absl::nullopt};
      });

  EXPECT_CALL(*mock_, UploadChunk)
      .WillOnce([&](internal::UploadChunkRequest const& r) {
        EXPECT_TRUE(r.last_chunk());
        return internal::QueryResumableUploadResponse{
            quantum + r.payload_size(), expected};
      });

  MockFilebuf filebuf;
  EXPECT_CALL(filebuf, SeekoffEvent).WillOnce([&](std::streamoff off) {
    EXPECT_EQ(quantum, off);
  });
  ASSERT_NE(nullptr, filebuf.open(temp_file.name().c_str(), std::ios_base::in));
  std::istream stream(&filebuf);

  ASSERT_TRUE(stream);
  auto client = ClientForMock();
  google::cloud::internal::OptionsSpan const span(mock_->options());
  auto res = internal::ClientImplDetails::UploadStreamResumable(
      client, stream,
      internal::ResumableUploadRequest("test-bucket-name", "test-object-name")
          .set_option(RestoreResumableUploadSession("test-only-upload-id")));
  ASSERT_STATUS_OK(res);
  EXPECT_EQ(expected, *res);
}

TEST_F(WriteObjectTest, UploadFile) {
  auto rng = google::cloud::internal::DefaultPRNG(std::random_device{}());
  auto const quantum = internal::UploadChunkRequest::kChunkSizeQuantum;
  auto const initial_size = quantum;
  auto const file_size = initial_size + 10;
  google::cloud::storage::testing::TempFile temp_file(
      google::cloud::storage::testing::MakeRandomData(rng, file_size));

  std::string text = R"""({"name": "test-bucket-name/test-object-name/1"})""";
  auto expected = internal::ObjectMetadataParser::FromString(text).value();

  EXPECT_CALL(*mock_, CreateResumableUpload)
      .WillOnce([&](internal::ResumableUploadRequest const& request) {
        EXPECT_TRUE(request.HasOption<UploadContentLength>());
        EXPECT_EQ(file_size, request.GetOption<UploadContentLength>().value());
        EXPECT_EQ("test-bucket-name", request.bucket_name());
        EXPECT_EQ("test-object-name", request.object_name());
        return internal::CreateResumableUploadResponse{"test-only-upload-id"};
      });

  EXPECT_CALL(*mock_, UploadChunk)
      .WillOnce([&](internal::UploadChunkRequest const& r) {
        EXPECT_TRUE(r.last_chunk());
        return internal::QueryResumableUploadResponse{file_size, expected};
      });

  auto client = ClientForMock();
  auto res = client.UploadFile(temp_file.name(), "test-bucket-name",
                               "test-object-name", UseResumableUploadSession());
  ASSERT_STATUS_OK(res);
  EXPECT_EQ(expected, *res);
}

/// @test Verify custom headers are preserved in UploadChunk() requests.
TEST_F(WriteObjectTest, UploadStreamResumableWithCustomHeader) {
  auto rng = google::cloud::internal::DefaultPRNG(std::random_device{}());
  auto const quantum = internal::UploadChunkRequest::kChunkSizeQuantum;
  auto const initial_size = quantum;
  auto const file_size = initial_size + 10;
  google::cloud::storage::testing::TempFile temp_file(
      google::cloud::storage::testing::MakeRandomData(rng, file_size));

  std::string text = R"""({"name": "test-bucket-name/test-object-name/1"})""";
  auto expected = internal::ObjectMetadataParser::FromString(text).value();

  EXPECT_CALL(*mock_, CreateResumableUpload)
      .WillOnce([&](internal::ResumableUploadRequest const& request) {
        EXPECT_TRUE(request.HasOption<UploadContentLength>());
        EXPECT_EQ(file_size, request.GetOption<UploadContentLength>().value());
        EXPECT_EQ("test-bucket-name", request.bucket_name());
        EXPECT_EQ("test-object-name", request.object_name());
        return internal::CreateResumableUploadResponse{"test-only-upload-id"};
      });

  EXPECT_CALL(*mock_, UploadChunk)
      .WillOnce([&](internal::UploadChunkRequest const& r) {
        EXPECT_TRUE(r.last_chunk());
        EXPECT_EQ("header-value", r.GetOption<CustomHeader>().value_or(""));
        return internal::QueryResumableUploadResponse{file_size, expected};
      });

  auto client = ClientForMock();
  auto res =
      client.UploadFile(temp_file.name(), "test-bucket-name",
                        "test-object-name", UseResumableUploadSession(),
                        CustomHeader("x-test-custom-header", "header-value"));
  ASSERT_STATUS_OK(res);
  EXPECT_EQ(expected, *res);
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google
