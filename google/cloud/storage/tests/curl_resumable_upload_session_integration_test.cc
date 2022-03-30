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

#include "google/cloud/storage/internal/curl_client.h"
#include "google/cloud/storage/testing/storage_integration_test.h"
#include "google/cloud/internal/getenv.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace storage {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {
namespace {

class CurlResumableUploadIntegrationTest
    : public google::cloud::storage::testing::StorageIntegrationTest {
 protected:
  void SetUp() override {
    bucket_name_ = google::cloud::internal::GetEnv(
                       "GOOGLE_CLOUD_CPP_STORAGE_TEST_BUCKET_NAME")
                       .value_or("");
    ASSERT_FALSE(bucket_name_.empty());
    project_id_ =
        google::cloud::internal::GetEnv("GOOGLE_CLOUD_PROJECT").value_or("");
    ASSERT_FALSE(project_id_.empty());
  }

  std::string bucket_name_;
  std::string project_id_;
};

TEST_F(CurlResumableUploadIntegrationTest, Simple) {
  auto client_options = ClientOptions::CreateDefaultClientOptions();
  ASSERT_STATUS_OK(client_options);
  auto client = CurlClient::Create(*client_options);
  auto object_name = MakeRandomObjectName();

  ResumableUploadRequest request(bucket_name_, object_name);
  request.set_multiple_options(IfGenerationMatch(0));

  auto create = client->CreateResumableSession(request);
  ASSERT_STATUS_OK(create);
  auto session = std::move(create->session);

  std::string const contents = LoremIpsum();
  StatusOr<ResumableUploadResponse> response =
      session->UploadFinalChunk({{contents}}, contents.size(), HashValues{});

  ASSERT_STATUS_OK(response);
  EXPECT_TRUE(response->payload.has_value());
  auto metadata = *response->payload;
  ScheduleForDelete(metadata);
  EXPECT_EQ(object_name, metadata.name());
  EXPECT_EQ(bucket_name_, metadata.bucket());
  EXPECT_EQ(contents.size(), metadata.size());
}

TEST_F(CurlResumableUploadIntegrationTest, WithReset) {
  auto client_options = ClientOptions::CreateDefaultClientOptions();
  ASSERT_STATUS_OK(client_options);
  auto client = CurlClient::Create(*client_options);
  auto object_name = MakeRandomObjectName();

  ResumableUploadRequest request(bucket_name_, object_name);
  request.set_multiple_options(IfGenerationMatch(0));

  auto create = client->CreateResumableSession(request);
  ASSERT_STATUS_OK(create);
  auto session = std::move(create->session);

  std::string const contents(UploadChunkRequest::kChunkSizeQuantum, '0');
  StatusOr<ResumableUploadResponse> response =
      session->UploadChunk({{contents}});
  ASSERT_STATUS_OK(response.status());

  response = session->ResetSession();
  ASSERT_STATUS_OK(response);

  response = session->UploadFinalChunk({{contents}}, 2 * contents.size(),
                                       HashValues{});
  ASSERT_STATUS_OK(response);

  EXPECT_TRUE(response->payload.has_value());
  auto metadata = *response->payload;
  ScheduleForDelete(metadata);
  EXPECT_EQ(object_name, metadata.name());
  EXPECT_EQ(bucket_name_, metadata.bucket());
  EXPECT_EQ(2 * contents.size(), metadata.size());
}

TEST_F(CurlResumableUploadIntegrationTest, Restore) {
  auto client_options = ClientOptions::CreateDefaultClientOptions();
  ASSERT_STATUS_OK(client_options);
  auto client = CurlClient::Create(*client_options);
  auto object_name = MakeRandomObjectName();

  ResumableUploadRequest request(bucket_name_, object_name);
  request.set_multiple_options(IfGenerationMatch(0));

  auto old_create = client->CreateResumableSession(request);
  ASSERT_STATUS_OK(old_create);
  auto old_session = std::move(old_create->session);

  std::string const contents(UploadChunkRequest::kChunkSizeQuantum, '0');

  StatusOr<ResumableUploadResponse> response =
      old_session->UploadChunk({{contents}});
  ASSERT_STATUS_OK(response.status());

  auto restore =
      client->FullyRestoreResumableSession(request, old_session->session_id());
  ASSERT_STATUS_OK(restore);
  auto session = std::move(restore->session);

  old_session.reset();

  response = session->UploadChunk({{contents}});
  ASSERT_STATUS_OK(response);

  response = session->UploadFinalChunk({{contents}}, 3 * contents.size(),
                                       HashValues{});
  ASSERT_STATUS_OK(response);

  EXPECT_TRUE(response->payload.has_value());
  auto metadata = *response->payload;
  ScheduleForDelete(metadata);
  EXPECT_EQ(object_name, metadata.name());
  EXPECT_EQ(bucket_name_, metadata.bucket());
  EXPECT_EQ(3 * contents.size(), metadata.size());
}

TEST_F(CurlResumableUploadIntegrationTest, EmptyTrailer) {
  auto client_options = ClientOptions::CreateDefaultClientOptions();
  ASSERT_STATUS_OK(client_options);
  auto client = CurlClient::Create(*client_options);
  auto object_name = MakeRandomObjectName();

  ResumableUploadRequest request(bucket_name_, object_name);
  request.set_multiple_options(IfGenerationMatch(0));

  auto create = client->CreateResumableSession(request);
  ASSERT_STATUS_OK(create);
  auto session = std::move(create->session);

  std::string const contents(UploadChunkRequest::kChunkSizeQuantum, '0');
  // Send 2 chunks sized to be round quantums.
  StatusOr<ResumableUploadResponse> response =
      session->UploadChunk({{contents}});
  ASSERT_STATUS_OK(response.status());
  response = session->UploadChunk({{contents}});
  ASSERT_STATUS_OK(response.status());

  // Consider a streaming upload where the application flushes before closing
  // the stream *and* the flush sends all the data remaining in the stream.
  // This can happen naturally when the upload is a round multiple of the
  // upload quantum. In this case the stream is terminated by sending an empty
  // chunk at the end, with the size of the previous chunks as an indication
  // of "done".
  response = session->UploadFinalChunk({}, 2 * contents.size(), HashValues{});
  ASSERT_STATUS_OK(response.status());

  EXPECT_TRUE(response->payload.has_value());
  auto metadata = *response->payload;
  ScheduleForDelete(metadata);
  EXPECT_EQ(object_name, metadata.name());
  EXPECT_EQ(bucket_name_, metadata.bucket());
  EXPECT_EQ(2 * contents.size(), metadata.size());
}

TEST_F(CurlResumableUploadIntegrationTest, Empty) {
  auto client_options = ClientOptions::CreateDefaultClientOptions();
  ASSERT_STATUS_OK(client_options);
  auto client = CurlClient::Create(*client_options);
  auto object_name = MakeRandomObjectName();

  ResumableUploadRequest request(bucket_name_, object_name);
  request.set_multiple_options(IfGenerationMatch(0));

  auto create = client->CreateResumableSession(request);
  ASSERT_STATUS_OK(create);
  auto session = std::move(create->session);

  auto response = session->UploadFinalChunk({}, 0, HashValues{});
  ASSERT_STATUS_OK(response.status());

  EXPECT_TRUE(response->payload.has_value());
  auto metadata = *response->payload;
  ScheduleForDelete(metadata);
  EXPECT_EQ(object_name, metadata.name());
  EXPECT_EQ(bucket_name_, metadata.bucket());
  EXPECT_EQ(0, metadata.size());
}

/**
 * @test Verify that resetting sessions with query parameters works.
 *
 * UserProject parameter is not tested because it is hard to set up. The hope is
 * that if it stops to work, other parameters do too.
 */
TEST_F(CurlResumableUploadIntegrationTest, ResetWithParameters) {
  auto client_options = ClientOptions::CreateDefaultClientOptions();
  ASSERT_STATUS_OK(client_options);
  auto raw_client =
      CurlClient::Create(client_options->set_project_id(project_id_));
  auto client = ClientImplDetails::CreateClient(raw_client);
  auto object_name = MakeRandomObjectName();
  auto generator = google::cloud::internal::DefaultPRNG(std::random_device{}());
  auto bucket_name = MakeRandomBucketName();
  auto const csek = CreateKeyFromGenerator(generator);

  auto res = client.CreateBucket(bucket_name, BucketMetadata{});
  ASSERT_STATUS_OK(res);
  ScheduleForDelete(*res);
  res = client.PatchBucket(bucket_name, BucketMetadataPatchBuilder().SetBilling(
                                            BucketBilling{true}));
  EXPECT_STATUS_OK(res);
  EXPECT_TRUE(res->has_billing());
  EXPECT_EQ(res->billing_as_optional().value_or(BucketBilling{true}),
            BucketBilling{true});

  ResumableUploadRequest request(bucket_name, object_name);
  request.set_multiple_options(IfGenerationMatch(0),
                               QuotaUser("test-quota-user"), Fields("name"),
                               UserProject(project_id_), EncryptionKey(csek));

  auto create = raw_client->CreateResumableSession(request);
  ASSERT_STATUS_OK(create);
  auto session = std::move(create->session);

  std::string const contents(UploadChunkRequest::kChunkSizeQuantum, '0');
  StatusOr<ResumableUploadResponse> response =
      session->UploadChunk({{contents}});
  ASSERT_STATUS_OK(response);

  response = session->ResetSession();
  ASSERT_STATUS_OK(response);

  response = session->UploadFinalChunk({{contents}}, 2 * contents.size(),
                                       HashValues{});
  ASSERT_STATUS_OK(response);
  ASSERT_TRUE(response->payload.has_value());
  auto metadata = *response->payload;
  ScheduleForDelete(metadata);

  EXPECT_EQ(object_name, metadata.name());
  // These are an effect of Fields("name")
  EXPECT_EQ("", metadata.bucket());
  EXPECT_EQ(0, metadata.size());

  auto stream = client.ReadObject(
      bucket_name, object_name, UserProject(project_id_), EncryptionKey(csek));
  std::string actual_contents(std::istreambuf_iterator<char>{stream}, {});
  ASSERT_STATUS_OK(stream.status());
  EXPECT_EQ(2 * contents.size(), actual_contents.size());
  EXPECT_TRUE(std::all_of(actual_contents.begin(), actual_contents.end(),
                          [](char c) { return c == '0'; }));
}

}  // namespace
}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google
