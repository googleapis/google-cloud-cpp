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

#include "google/cloud/internal/make_unique.h"
#include "google/cloud/storage/client.h"
#include "google/cloud/storage/oauth2/google_credentials.h"
#include "google/cloud/storage/retry_policy.h"
#include "google/cloud/storage/testing/canonical_errors.h"
#include "google/cloud/storage/testing/mock_client.h"
#include "google/cloud/testing_util/assert_ok.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace {

using ::google::cloud::internal::make_unique;
using ::google::cloud::storage::testing::canonical_errors::PermanentError;
using ::google::cloud::storage::testing::canonical_errors::TransientError;
using ::testing::_;
using ::testing::HasSubstr;
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
    mock = std::make_shared<testing::MockClient>();
    EXPECT_CALL(*mock, client_options())
        .WillRepeatedly(ReturnRef(client_options));
    client.reset(new Client{
        std::shared_ptr<internal::RawClient>(mock),
        ExponentialBackoffPolicy(std::chrono::milliseconds(1),
                                 std::chrono::milliseconds(1), 2.0)});
  }
  void TearDown() override {
    client.reset();
    mock.reset();
  }

  std::shared_ptr<testing::MockClient> mock;
  std::unique_ptr<Client> client;
  ClientOptions client_options =
      ClientOptions(oauth2::CreateAnonymousCredentials());
};

TEST_F(WriteObjectTest, WriteObject) {
  std::string text = R"""({
      "name": "test-bucket-name/test-object-name/1"
})""";
  auto expected = internal::ObjectMetadataParser::FromString(text).value();

  EXPECT_CALL(*mock, CreateResumableSession(_))
      .WillOnce(
          Invoke([&expected](internal::ResumableUploadRequest const& request) {
            EXPECT_EQ("test-bucket-name", request.bucket_name());
            EXPECT_EQ("test-object-name", request.object_name());

            auto mock = make_unique<testing::MockResumableUploadSession>();
            using internal::ResumableUploadResponse;
            EXPECT_CALL(*mock, done()).WillRepeatedly(Return(false));
            EXPECT_CALL(*mock, next_expected_byte()).WillRepeatedly(Return(0));
            EXPECT_CALL(*mock, UploadChunk(_))
                .WillRepeatedly(Return(make_status_or(ResumableUploadResponse{
                    "fake-url", 0, {}, ResumableUploadResponse::kInProgress})));
            EXPECT_CALL(*mock, ResetSession())
                .WillOnce(Return(make_status_or(ResumableUploadResponse{
                    "fake-url", 0, {}, ResumableUploadResponse::kInProgress})));
            EXPECT_CALL(*mock, UploadFinalChunk(_, _))
                .WillOnce(
                    Return(StatusOr<ResumableUploadResponse>(TransientError())))
                .WillOnce(Return(make_status_or(ResumableUploadResponse{
                    "fake-url", 0, expected, ResumableUploadResponse::kDone})));

            return make_status_or(
                std::unique_ptr<internal ::ResumableUploadSession>(
                    std::move(mock)));
          }));

  auto stream = client->WriteObject("test-bucket-name", "test-object-name");
  stream << "Hello World!";
  stream.Close();
  ObjectMetadata actual = stream.metadata().value();
  EXPECT_EQ(expected, actual);
}

TEST_F(WriteObjectTest, WriteObjectTooManyFailures) {
  Client client{std::shared_ptr<internal::RawClient>(mock),
                LimitedErrorCountRetryPolicy(2),
                ExponentialBackoffPolicy(std::chrono::milliseconds(1),
                                         std::chrono::milliseconds(1), 2.0)};

  auto returner = [](internal::ResumableUploadRequest const&) {
    return StatusOr<std::unique_ptr<internal::ResumableUploadSession>>(
        TransientError());
  };
  EXPECT_CALL(*mock, CreateResumableSession(_))
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
  EXPECT_CALL(*mock, CreateResumableSession(_)).WillOnce(Invoke(returner));
  auto stream = client->WriteObject("test-bucket-name", "test-object-name");
  EXPECT_TRUE(stream.bad());
  EXPECT_FALSE(stream.metadata().status().ok());
  EXPECT_EQ(PermanentError().code(), stream.metadata().status().code())
      << ", status=" << stream.metadata().status();
}

TEST_F(WriteObjectTest, WriteObjectPermanentSessionFailurePropagates) {
  testing::MockResumableUploadSession* mock_session =
      new testing::MockResumableUploadSession;
  auto returner = [mock_session](internal::ResumableUploadRequest const&) {
    return StatusOr<std::unique_ptr<internal::ResumableUploadSession>>(
        std::unique_ptr<internal::ResumableUploadSession>(mock_session));
  };
  EXPECT_CALL(*mock, CreateResumableSession(_)).WillOnce(Invoke(returner));
  EXPECT_CALL(*mock_session, UploadChunk(_))
      .WillRepeatedly(Return(PermanentError()));
  EXPECT_CALL(*mock_session, done()).WillRepeatedly(Return(false));
  auto stream = client->WriteObject("test-bucket-name", "test-object-name");

  // make sure it is actually sent
  std::vector<char> data(client_options.upload_buffer_size() + 1, 'X');
  stream.write(data.data(), data.size());
  EXPECT_TRUE(stream.bad());
  stream.Close();
  EXPECT_FALSE(stream.metadata());
  EXPECT_EQ(PermanentError().code(), stream.metadata().status().code())
      << ", status=" << stream.metadata().status();
}

}  // namespace
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google
