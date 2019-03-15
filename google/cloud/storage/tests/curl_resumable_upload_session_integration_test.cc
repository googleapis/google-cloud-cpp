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
#include "google/cloud/testing_util/assert_ok.h"
#include "google/cloud/testing_util/init_google_mock.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace internal {
namespace {

using ::testing::HasSubstr;

// Initialized in main() below.
char const* flag_bucket_name;

class CurlResumableUploadIntegrationTest
    : public google::cloud::storage::testing::StorageIntegrationTest {};

TEST_F(CurlResumableUploadIntegrationTest, Simple) {
  auto client_options = ClientOptions::CreateDefaultClientOptions();
  ASSERT_STATUS_OK(client_options);
  auto client = CurlClient::Create(*client_options);
  std::string bucket_name = flag_bucket_name;
  auto object_name = MakeRandomObjectName();

  ResumableUploadRequest request(bucket_name, object_name);
  request.set_multiple_options(IfGenerationMatch(0));

  StatusOr<std::unique_ptr<ResumableUploadSession>> session =
      client->CreateResumableSession(request);

  ASSERT_STATUS_OK(session);

  std::string const contents = LoremIpsum();
  StatusOr<ResumableUploadResponse> response =
      (*session)->UploadChunk(contents, contents.size());

  ASSERT_STATUS_OK(response);
  EXPECT_FALSE(response->payload.empty());
  auto metadata =
      internal::ObjectMetadataParser::FromString(response->payload).value();
  EXPECT_EQ(object_name, metadata.name());
  EXPECT_EQ(bucket_name, metadata.bucket());
  EXPECT_EQ(contents.size(), metadata.size());

  auto status =
      client->DeleteObject(DeleteObjectRequest(bucket_name, object_name));
  ASSERT_STATUS_OK(status);
}

TEST_F(CurlResumableUploadIntegrationTest, WithReset) {
  auto client_options = ClientOptions::CreateDefaultClientOptions();
  ASSERT_STATUS_OK(client_options);
  auto client = CurlClient::Create(*client_options);
  std::string bucket_name = flag_bucket_name;
  auto object_name = MakeRandomObjectName();

  ResumableUploadRequest request(bucket_name, object_name);
  request.set_multiple_options(IfGenerationMatch(0));

  StatusOr<std::unique_ptr<ResumableUploadSession>> session =
      client->CreateResumableSession(request);

  ASSERT_STATUS_OK(session);

  std::string const contents(UploadChunkRequest::kChunkSizeQuantum, '0');
  StatusOr<ResumableUploadResponse> response =
      (*session)->UploadChunk(contents, 2 * contents.size());
  ASSERT_STATUS_OK(response.status());

  response = (*session)->ResetSession();
  ASSERT_STATUS_OK(response);

  response = (*session)->UploadChunk(contents, 2 * contents.size());
  ASSERT_STATUS_OK(response);

  EXPECT_FALSE(response->payload.empty());
  auto metadata =
      internal::ObjectMetadataParser::FromString(response->payload).value();
  EXPECT_EQ(object_name, metadata.name());
  EXPECT_EQ(bucket_name, metadata.bucket());
  EXPECT_EQ(2 * contents.size(), metadata.size());

  auto status =
      client->DeleteObject(DeleteObjectRequest(bucket_name, object_name));
  ASSERT_STATUS_OK(status);
}

TEST_F(CurlResumableUploadIntegrationTest, Restore) {
  auto client_options = ClientOptions::CreateDefaultClientOptions();
  ASSERT_STATUS_OK(client_options);
  auto client = CurlClient::Create(*client_options);
  std::string bucket_name = flag_bucket_name;
  auto object_name = MakeRandomObjectName();

  ResumableUploadRequest request(bucket_name, object_name);
  request.set_multiple_options(IfGenerationMatch(0));

  StatusOr<std::unique_ptr<ResumableUploadSession>> old_session;
  old_session = client->CreateResumableSession(request);

  ASSERT_STATUS_OK(old_session);

  std::string const contents(UploadChunkRequest::kChunkSizeQuantum, '0');

  StatusOr<ResumableUploadResponse> response =
      (*old_session)->UploadChunk(contents, 3 * contents.size());
  ASSERT_STATUS_OK(response.status());

  StatusOr<std::unique_ptr<ResumableUploadSession>> session =
      client->RestoreResumableSession((*old_session)->session_id());
  EXPECT_EQ(contents.size(), (*session)->next_expected_byte());
  old_session->reset();

  response = (*session)->UploadChunk(contents, 3 * contents.size());
  ASSERT_STATUS_OK(response);

  response = (*session)->UploadChunk(contents, 3 * contents.size());
  ASSERT_STATUS_OK(response);

  EXPECT_FALSE(response->payload.empty());
  auto metadata =
      internal::ObjectMetadataParser::FromString(response->payload).value();
  EXPECT_EQ(object_name, metadata.name());
  EXPECT_EQ(bucket_name, metadata.bucket());
  EXPECT_EQ(3 * contents.size(), metadata.size());

  auto status =
      client->DeleteObject(DeleteObjectRequest(bucket_name, object_name));
  ASSERT_STATUS_OK(status);
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
    std::cerr << "Usage: " << cmd.substr(last_slash + 1) << " <bucket-name>\n";
    return 1;
  }

  google::cloud::storage::internal::flag_bucket_name = argv[1];

  return RUN_ALL_TESTS();
}
