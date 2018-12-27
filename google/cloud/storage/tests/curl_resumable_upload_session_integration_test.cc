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

#include "google/cloud/storage/internal/curl_client.h"
#include "google/cloud/storage/testing/storage_integration_test.h"
#include "google/cloud/testing_util/init_google_mock.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace internal {
namespace {
using ::testing::HasSubstr;

class ResumableUploadTestEnvironment : public ::testing::Environment {
 public:
  ResumableUploadTestEnvironment(std::string instance) {
    bucket_name_ = std::move(instance);
  }

  static std::string const& bucket_name() { return bucket_name_; }

 private:
  static std::string bucket_name_;
};

std::string ResumableUploadTestEnvironment::bucket_name_;

class CurlResumableUploadIntegrationTest
    : public google::cloud::storage::testing::StorageIntegrationTest {};

TEST_F(CurlResumableUploadIntegrationTest, Simple) {
  auto client = CurlClient::Create(ClientOptions());
  auto bucket_name = ResumableUploadTestEnvironment::bucket_name();
  auto object_name = MakeRandomObjectName();

  ResumableUploadRequest request(bucket_name, object_name);
  request.set_multiple_options(IfGenerationMatch(0));

  StatusOr<std::unique_ptr<ResumableUploadSession>> session = client->CreateResumableSession(request);

  ASSERT_TRUE(session.ok());

  std::string const contents = LoremIpsum();
  StatusOr<ResumableUploadResponse> response = (*session)->UploadChunk(contents, contents.size());

  ASSERT_TRUE(response.ok());
  EXPECT_FALSE(response->payload.empty());
  auto metadata = ObjectMetadata::ParseFromString(response->payload).value();
  EXPECT_EQ(object_name, metadata.name());
  EXPECT_EQ(bucket_name, metadata.bucket());
  EXPECT_EQ(contents.size(), metadata.size());

  client->DeleteObject(DeleteObjectRequest(bucket_name, object_name));
}

TEST_F(CurlResumableUploadIntegrationTest, WithReset) {
  auto client = CurlClient::Create(ClientOptions());
  auto bucket_name = ResumableUploadTestEnvironment::bucket_name();
  auto object_name = MakeRandomObjectName();

  ResumableUploadRequest request(bucket_name, object_name);
  request.set_multiple_options(IfGenerationMatch(0));

  StatusOr<std::unique_ptr<ResumableUploadSession>> session = client->CreateResumableSession(request);

  ASSERT_TRUE(session.ok());

  std::string const contents(UploadChunkRequest::kChunkSizeQuantum, '0');
  StatusOr<ResumableUploadResponse> response =
      (*session)->UploadChunk(contents, 2 * contents.size());
  ASSERT_TRUE(response.status().status_code() == 200 or response.status().status_code() == 308)
      << response.status();

  response = (*session)->ResetSession();
  ASSERT_TRUE(response.ok()) << response.status();

  response =
      (*session)->UploadChunk(contents, 2 * contents.size());
  ASSERT_TRUE(response.ok()) << response.status();

  EXPECT_FALSE(response->payload.empty());
  auto metadata = ObjectMetadata::ParseFromString(response->payload).value();
  EXPECT_EQ(object_name, metadata.name());
  EXPECT_EQ(bucket_name, metadata.bucket());
  EXPECT_EQ(2 * contents.size(), metadata.size());

  client->DeleteObject(DeleteObjectRequest(bucket_name, object_name));
}

TEST_F(CurlResumableUploadIntegrationTest, Restore) {
  auto client = CurlClient::Create(ClientOptions());
  auto bucket_name = ResumableUploadTestEnvironment::bucket_name();
  auto object_name = MakeRandomObjectName();

  ResumableUploadRequest request(bucket_name, object_name);
  request.set_multiple_options(IfGenerationMatch(0));

  StatusOr<std::unique_ptr<ResumableUploadSession>> old_session;
  old_session = client->CreateResumableSession(request);

  ASSERT_TRUE(old_session.ok());

  std::string const contents(UploadChunkRequest::kChunkSizeQuantum, '0');

  StatusOr<ResumableUploadResponse> response =
      (*old_session)->UploadChunk(contents, 3 * contents.size());
  ASSERT_TRUE(response.status().status_code() == 200 or response.status().status_code() == 308)
      << response.status();

  StatusOr<std::unique_ptr<ResumableUploadSession>> session =
      client->RestoreResumableSession((*old_session)->session_id());
  EXPECT_EQ(contents.size(), (*session)->next_expected_byte());
  old_session->reset();

  response =
      (*session)->UploadChunk(contents, 3 * contents.size());
  ASSERT_TRUE(response.ok()) << response.status();

  response =
      (*session)->UploadChunk(contents, 3 * contents.size());
  ASSERT_TRUE(response.ok()) << response.status();

  EXPECT_FALSE(response->payload.empty());
  auto metadata = ObjectMetadata::ParseFromString(response->payload).value();
  EXPECT_EQ(object_name, metadata.name());
  EXPECT_EQ(bucket_name, metadata.bucket());
  EXPECT_EQ(3 * contents.size(), metadata.size());

  client->DeleteObject(DeleteObjectRequest(bucket_name, object_name));
}

}  // namespace
}  // namespace internal
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google

int main(int argc, char* argv[]) {
  google::cloud::testing_util::InitGoogleMock(argc, argv);

  // Make sure the arguments are valid.
  if (argc != 2) {
    std::string const cmd = argv[0];
    auto last_slash = std::string(argv[0]).find_last_of('/');
    std::cerr << "Usage: " << cmd.substr(last_slash + 1) << " <bucket-name>"
              << std::endl;
    return 1;
  }

  std::string const bucket_name = argv[1];
  (void)::testing::AddGlobalTestEnvironment(
      new google::cloud::storage::internal::ResumableUploadTestEnvironment(
          bucket_name));

  return RUN_ALL_TESTS();
}
