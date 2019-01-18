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
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace {
using ::testing::_;
using ::testing::HasSubstr;
using ::testing::Invoke;
using ::testing::Return;
using ::testing::ReturnRef;
using testing::canonical_errors::PermanentError;
using testing::canonical_errors::TransientError;
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
    client.reset(new Client{std::shared_ptr<internal::RawClient>(mock)});
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

class MockStreambuf : public internal::ObjectWriteStreambuf {
 public:
  MOCK_CONST_METHOD0(IsOpen, bool());
  MOCK_METHOD0(DoClose, StatusOr<internal::HttpResponse>());
  MOCK_METHOD1(ValidateHash, bool(ObjectMetadata const&));
  MOCK_CONST_METHOD0(received_hash, std::string const&());
  MOCK_CONST_METHOD0(computed_hash, std::string const&());
  MOCK_CONST_METHOD0(resumable_session_id, std::string const&());
  MOCK_CONST_METHOD0(next_expected_byte, std::uint64_t());
};

TEST_F(WriteObjectTest, WriteObject) {
  std::string text = R"""({
      "name": "test-bucket-name/test-object-name/1"
})""";
  auto expected = internal::ObjectMetadataParser::FromString(text).value();

  EXPECT_CALL(*mock, WriteObject(_))
      .WillOnce(Invoke(
          [&text](internal::InsertObjectStreamingRequest const& request) {
            EXPECT_EQ("test-bucket-name", request.bucket_name());
            EXPECT_EQ("test-object-name", request.object_name());
            auto* mock_result = new MockStreambuf;
            EXPECT_CALL(*mock_result, DoClose())
                .WillRepeatedly(Return(internal::HttpResponse{200, text, {}}));
            EXPECT_CALL(*mock_result, IsOpen()).WillRepeatedly(Return(true));
            std::unique_ptr<internal::ObjectWriteStreambuf> result(mock_result);
            return make_status_or(std::move(result));
          }));

  auto stream = client->WriteObject("test-bucket-name", "test-object-name");
  stream << "Hello World!";
  stream.Close();
  ObjectMetadata actual = stream.metadata().value();
  EXPECT_EQ(expected, actual);
}

TEST_F(WriteObjectTest, WriteObjectTooManyFailures) {
  Client client{std::shared_ptr<internal::RawClient>(mock),
                LimitedErrorCountRetryPolicy(2)};

  auto returner = [](internal::InsertObjectStreamingRequest const&) {
    return StatusOr<std::unique_ptr<internal::ObjectWriteStreambuf>>(
        TransientError());
  };
#if GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
  EXPECT_CALL(*mock, WriteObject(_))
      .WillOnce(Invoke(returner))
      .WillOnce(Invoke(returner))
      .WillOnce(Invoke(returner));
  EXPECT_THROW(
      try {
        client.WriteObject("test-bucket-name", "test-object-name").Close();
      } catch (std::runtime_error const& ex) {
        EXPECT_THAT(ex.what(), HasSubstr("Retry policy exhausted"));
        EXPECT_THAT(ex.what(), HasSubstr("WriteObject"));
        throw;
      },
      std::runtime_error);
#else
  // With EXPECT_DEATH*() the mocking framework cannot detect how many times the
  // operation is called.
  EXPECT_CALL(*mock, WriteObject(_)).WillRepeatedly(Invoke(returner));
  EXPECT_DEATH_IF_SUPPORTED(
      client.WriteObject("test-bucket-name", "test-object-name").Close(),
      "exceptions are disabled");
#endif  // GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
}

TEST_F(WriteObjectTest, WriteObjectPermanentFailure) {
  auto returner = [](internal::InsertObjectStreamingRequest const&) {
    return StatusOr<std::unique_ptr<internal::ObjectWriteStreambuf>>(
        PermanentError());
  };
#if GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
  EXPECT_CALL(*mock, WriteObject(_)).WillOnce(Invoke(returner));
  EXPECT_THROW(
      try {
        client->WriteObject("test-bucket-name", "test-object-name").Close();
      } catch (std::runtime_error const& ex) {
        EXPECT_THAT(ex.what(), HasSubstr("Permanent error"));
        EXPECT_THAT(ex.what(), HasSubstr("WriteObject"));
        throw;
      },
      std::runtime_error);
#else
  // With EXPECT_DEATH*() the mocking framework cannot detect how many times the
  // operation is called.
  EXPECT_CALL(*mock, WriteObject(_)).WillRepeatedly(Invoke(returner));
  EXPECT_DEATH_IF_SUPPORTED(
      client->WriteObject("test-bucket-name", "test-object-name").Close(),
      "exceptions are disabled");
#endif  // GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
}

}  // namespace
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google
