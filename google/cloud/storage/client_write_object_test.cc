// Copyright 2018 Google LLC
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

#include "google/cloud/storage/client.h"
#include "google/cloud/storage/oauth2/google_credentials.h"
#include "google/cloud/storage/retry_policy.h"
#include "google/cloud/storage/testing/canonical_errors.h"
#include "google/cloud/storage/testing/mock_client.h"
#include "google/cloud/storage/testing/random_names.h"
#include "google/cloud/storage/testing/temp_file.h"
#include "google/cloud/testing_util/assert_ok.h"
#include "absl/memory/memory.h"
#include <gmock/gmock.h>
#include <fstream>

namespace google {
namespace cloud {
namespace storage {
namespace testing {
class ClientTester {
 public:
  static StatusOr<ObjectMetadata> UploadStreamResumable(
      Client& client, std::istream& source,
      internal::ResumableUploadRequest const& request) {
    return client.UploadStreamResumable(source, request);
  }
};
}  // namespace testing
inline namespace STORAGE_CLIENT_NS {
namespace {

using ::google::cloud::storage::testing::canonical_errors::PermanentError;
using ::google::cloud::storage::testing::canonical_errors::TransientError;
using ::testing::_;
using ::testing::Invoke;
using ::testing::Return;
using ::testing::ReturnRef;
using ms = std::chrono::milliseconds;

/**
 * Test the functions in Storage::Client related to writing objects.'Objects:
 * *'.
 */
class WriteObjectTest : public ::testing::Test {
 protected:
  void SetUp() override {
    mock_ = std::make_shared<testing::MockClient>();
    EXPECT_CALL(*mock_, client_options())
        .WillRepeatedly(ReturnRef(client_options_));
    client_.reset(new Client{
        std::shared_ptr<internal::RawClient>(mock_),
        ExponentialBackoffPolicy(std::chrono::milliseconds(1),
                                 std::chrono::milliseconds(1), 2.0)});
  }
  void TearDown() override {
    client_.reset();
    mock_.reset();
  }

  std::shared_ptr<testing::MockClient> mock_;
  std::unique_ptr<Client> client_;
  ClientOptions client_options_ =
      ClientOptions(oauth2::CreateAnonymousCredentials())
          .SetUploadBufferSize(2 *
                               internal::UploadChunkRequest::kChunkSizeQuantum);
};

TEST_F(WriteObjectTest, WriteObject) {
  std::string text = R"""({
      "name": "test-bucket-name/test-object-name/1"
})""";
  auto expected = internal::ObjectMetadataParser::FromString(text).value();

  EXPECT_CALL(*mock_, CreateResumableSession(_))
      .WillOnce(Invoke([&expected](
                           internal::ResumableUploadRequest const& request) {
        EXPECT_EQ("test-bucket-name", request.bucket_name());
        EXPECT_EQ("test-object-name", request.object_name());

        auto mock = absl::make_unique<testing::MockResumableUploadSession>();
        using internal::ResumableUploadResponse;
        EXPECT_CALL(*mock, done()).WillRepeatedly(Return(false));
        EXPECT_CALL(*mock, next_expected_byte()).WillRepeatedly(Return(0));
        EXPECT_CALL(*mock, UploadChunk(_))
            .WillRepeatedly(Return(make_status_or(ResumableUploadResponse{
                "fake-url", 0, {}, ResumableUploadResponse::kInProgress, {}})));
        EXPECT_CALL(*mock, ResetSession())
            .WillOnce(Return(make_status_or(ResumableUploadResponse{
                "fake-url", 0, {}, ResumableUploadResponse::kInProgress, {}})));
        EXPECT_CALL(*mock, UploadFinalChunk(_, _))
            .WillOnce(
                Return(StatusOr<ResumableUploadResponse>(TransientError())))
            .WillOnce(Return(make_status_or(ResumableUploadResponse{
                "fake-url", 0, expected, ResumableUploadResponse::kDone, {}})));

        return make_status_or(
            std::unique_ptr<internal ::ResumableUploadSession>(
                std::move(mock)));
      }));

  auto stream = client_->WriteObject("test-bucket-name", "test-object-name");
  stream << "Hello World!";
  stream.Close();
  ObjectMetadata actual = stream.metadata().value();
  EXPECT_EQ(expected, actual);
}

TEST_F(WriteObjectTest, WriteObjectTooManyFailures) {
  Client client{std::shared_ptr<internal::RawClient>(mock_),
                LimitedErrorCountRetryPolicy(2),
                ExponentialBackoffPolicy(std::chrono::milliseconds(1),
                                         std::chrono::milliseconds(1), 2.0)};

  auto returner = [](internal::ResumableUploadRequest const&) {
    return StatusOr<std::unique_ptr<internal::ResumableUploadSession>>(
        TransientError());
  };
  EXPECT_CALL(*mock_, CreateResumableSession(_))
      .WillOnce(Invoke(returner))
      .WillOnce(Invoke(returner))
      .WillOnce(Invoke(returner));

  auto stream = client.WriteObject("test-bucket-name", "test-object-name");
  EXPECT_TRUE(stream.bad());
  EXPECT_FALSE(stream.metadata().status().ok());
  EXPECT_EQ(TransientError().code(), stream.metadata().status().code())
      << ", status=" << stream.metadata().status();
}

TEST_F(WriteObjectTest, WriteObjectPermanentFailure) {
  auto returner = [](internal::ResumableUploadRequest const&) {
    return StatusOr<std::unique_ptr<internal::ResumableUploadSession>>(
        PermanentError());
  };
  EXPECT_CALL(*mock_, CreateResumableSession(_)).WillOnce(Invoke(returner));
  auto stream = client_->WriteObject("test-bucket-name", "test-object-name");
  EXPECT_TRUE(stream.bad());
  EXPECT_FALSE(stream.metadata().status().ok());
  EXPECT_EQ(PermanentError().code(), stream.metadata().status().code())
      << ", status=" << stream.metadata().status();
}

TEST_F(WriteObjectTest, WriteObjectPermanentSessionFailurePropagates) {
  auto* mock_session = new testing::MockResumableUploadSession;
  auto returner = [mock_session](internal::ResumableUploadRequest const&) {
    return StatusOr<std::unique_ptr<internal::ResumableUploadSession>>(
        std::unique_ptr<internal::ResumableUploadSession>(mock_session));
  };
  std::string const empty;
  EXPECT_CALL(*mock_, CreateResumableSession(_)).WillOnce(Invoke(returner));
  EXPECT_CALL(*mock_session, UploadChunk(_))
      .WillRepeatedly(Return(PermanentError()));
  EXPECT_CALL(*mock_session, done()).WillRepeatedly(Return(false));
  EXPECT_CALL(*mock_session, session_id()).WillRepeatedly(ReturnRef(empty));
  auto stream = client_->WriteObject("test-bucket-name", "test-object-name");

  // make sure it is actually sent
  std::vector<char> data(client_options_.upload_buffer_size() + 1, 'X');
  stream.write(data.data(), data.size());
  EXPECT_TRUE(stream.bad());
  stream.Close();
  EXPECT_FALSE(stream.metadata());
  EXPECT_EQ(PermanentError().code(), stream.metadata().status().code())
      << ", status=" << stream.metadata().status();
}

// A std::filebuf which allows to learn about seekpos calls.
class MockFilebuf : public std::filebuf {
 public:
  MOCK_METHOD1(SeekoffEvent, void(std::streamoff));

 protected:
  std::streampos seekoff(std::streamoff off, std::ios_base::seekdir way,
                         std::ios_base::openmode which) override {
    SeekoffEvent(off);
    return std::filebuf::seekoff(off, way, which);
  }
};

class MockFilebufStream : public std::ifstream {
 public:
  explicit MockFilebufStream(std::string const& s) {
    buf_.open(s.c_str(), std::ios_base::in);
    init(&buf_);
    clear();
    std::cout << "OPEN? " << buf_.is_open() << std::endl;
  }

 private:
  MockFilebuf buf_;
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
  EXPECT_CALL(*mock_, CreateResumableSession(_))
      .WillOnce(Invoke([&expected, &bytes_written](
                           internal::ResumableUploadRequest const& request) {
        EXPECT_EQ("test-bucket-name", request.bucket_name());
        EXPECT_EQ("test-object-name", request.object_name());

        auto mock = absl::make_unique<testing::MockResumableUploadSession>();
        using internal::ResumableUploadResponse;
        EXPECT_CALL(*mock, done()).WillRepeatedly(Return(false));
        EXPECT_CALL(*mock, next_expected_byte())
            .WillRepeatedly(
                Invoke([&bytes_written]() { return bytes_written; }));

        EXPECT_CALL(*mock, UploadChunk(_))
            .WillRepeatedly(Invoke([&bytes_written](std::string const& data) {
              bytes_written += data.size();
              return make_status_or(
                  ResumableUploadResponse{"fake-url",
                                          bytes_written,
                                          {},
                                          ResumableUploadResponse::kInProgress,
                                          {}});
            }));
        EXPECT_CALL(*mock, UploadFinalChunk(_, _))
            .WillOnce(Invoke([expected, &bytes_written](std::string const& data,
                                                        size_t size) {
              bytes_written += data.size();
              EXPECT_EQ(bytes_written, size);
              return make_status_or(ResumableUploadResponse{
                  "fake-url", 0, expected, ResumableUploadResponse::kDone, {}});
            }));

        return make_status_or(
            std::unique_ptr<internal ::ResumableUploadSession>(
                std::move(mock)));
      }));

  MockFilebuf filebuf;
  // Don't expect any seekoff events
  EXPECT_CALL(filebuf, SeekoffEvent(_))
      .WillOnce(
          Invoke([quantum](std::streamoff off) { EXPECT_EQ(quantum, off); }));
  ASSERT_NE(nullptr, filebuf.open(temp_file.name().c_str(), std::ios_base::in));
  std::istream stream(&filebuf);

  ASSERT_TRUE(stream);
  auto res = testing::ClientTester::UploadStreamResumable(
      *client_, stream,
      internal::ResumableUploadRequest("test-bucket-name", "test-object-name"));
  ASSERT_STATUS_OK(res);
  EXPECT_EQ(expected, *res);
}

TEST_F(WriteObjectTest, UploadStreamResumableSimulateBug) {
  auto rng = google::cloud::internal::MakeDefaultPRNG();
  google::cloud::storage::testing::TempFile temp_file(
      google::cloud::storage::testing::MakeRandomData(
          rng, 2 * internal::UploadChunkRequest::kChunkSizeQuantum + 10));

  std::size_t bytes_written = 0;
  EXPECT_CALL(*mock_, CreateResumableSession(_))
      .WillOnce(Invoke([&bytes_written](
                           internal::ResumableUploadRequest const& request) {
        EXPECT_EQ("test-bucket-name", request.bucket_name());
        EXPECT_EQ("test-object-name", request.object_name());

        auto mock = absl::make_unique<testing::MockResumableUploadSession>();
        using internal::ResumableUploadResponse;
        EXPECT_CALL(*mock, done()).WillRepeatedly(Return(false));
        EXPECT_CALL(*mock, next_expected_byte())
            .WillOnce(Return(0))
            .WillOnce(Return(0))
            .WillOnce(Return(0))
            .WillOnce(Return(0))
            .WillOnce(Return(0))
            .WillOnce(Return(524288))
            .WillRepeatedly(Return(524287));  // start lying
        EXPECT_CALL(*mock, UploadChunk(_))
            .WillRepeatedly(Invoke([&bytes_written](std::string const& data) {
              bytes_written += data.size();
              return make_status_or(
                  ResumableUploadResponse{"fake-url",
                                          bytes_written,
                                          {},
                                          ResumableUploadResponse::kInProgress,
                                          {}});
            }));

        return make_status_or(
            std::unique_ptr<internal ::ResumableUploadSession>(
                std::move(mock)));
      }));

  MockFilebuf filebuf;
  // Don't expect any seekoff events
  EXPECT_CALL(filebuf, SeekoffEvent(_)).WillOnce(Invoke([](std::streamoff off) {
    EXPECT_EQ(0, off);
  }));
  ASSERT_NE(nullptr, filebuf.open(temp_file.name().c_str(), std::ios_base::in));
  std::istream stream(&filebuf);

  ASSERT_TRUE(stream);
  auto res = testing::ClientTester::UploadStreamResumable(
      *client_, stream,
      internal::ResumableUploadRequest("test-bucket-name", "test-object-name"));
  ASSERT_FALSE(res);
  EXPECT_EQ(StatusCode::kInternal, res.status().code());
  EXPECT_THAT(res.status().message(), ::testing::HasSubstr("This is a bug"));
}

TEST_F(WriteObjectTest, UploadFile) {
  auto const quantum = internal::UploadChunkRequest::kChunkSizeQuantum;
  auto rng = google::cloud::internal::MakeDefaultPRNG();
  std::uintmax_t const file_size = 10;
  google::cloud::storage::testing::TempFile temp_file(
      google::cloud::storage::testing::MakeRandomData(rng, file_size));

  std::string text = R"""({
      "name": "test-bucket-name/test-object-name/1"
})""";
  auto expected = internal::ObjectMetadataParser::FromString(text).value();

  // Simulate situation when a quantum has already been uploaded.
  std::size_t bytes_written = quantum;
  EXPECT_CALL(*mock_, CreateResumableSession(_))
      .WillOnce(Invoke([&expected, &bytes_written, file_size](
                           internal::ResumableUploadRequest const& request) {
        EXPECT_TRUE(request.HasOption<UploadContentLength>());
        EXPECT_EQ(file_size, request.GetOption<UploadContentLength>().value());
        EXPECT_EQ("test-bucket-name", request.bucket_name());
        EXPECT_EQ("test-object-name", request.object_name());

        auto mock = absl::make_unique<testing::MockResumableUploadSession>();
        using internal::ResumableUploadResponse;
        EXPECT_CALL(*mock, done()).WillRepeatedly(Return(false));
        EXPECT_CALL(*mock, next_expected_byte())
            .WillRepeatedly(
                Invoke([&bytes_written]() { return bytes_written; }));
        EXPECT_CALL(*mock, UploadFinalChunk(_, _))
            .WillOnce(Invoke([expected, &bytes_written](std::string const& data,
                                                        size_t size) {
              bytes_written += data.size();
              EXPECT_EQ(bytes_written, size);
              return make_status_or(ResumableUploadResponse{
                  "fake-url", 0, expected, ResumableUploadResponse::kDone, {}});
            }));

        return make_status_or(
            std::unique_ptr<internal ::ResumableUploadSession>(
                std::move(mock)));
      }));

  auto res =
      client_->UploadFile(temp_file.name(), "test-bucket-name",
                          "test-object-name", UseResumableUploadSession());
  ASSERT_STATUS_OK(res);
  EXPECT_EQ(expected, *res);
}

}  // namespace
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google
