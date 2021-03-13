// Copyright 2019 Google LLC
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
#include "google/cloud/storage/internal/curl_request_builder.h"
#include "google/cloud/storage/testing/storage_integration_test.h"
#include "google/cloud/internal/getenv.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>
#include <chrono>
#include <thread>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace {

class SignedUrlIntegrationTest
    : public google::cloud::storage::testing::StorageIntegrationTest {
 protected:
  void SetUp() override {
    bucket_name_ = google::cloud::internal::GetEnv(
                       "GOOGLE_CLOUD_CPP_STORAGE_TEST_BUCKET_NAME")
                       .value_or("");
    ASSERT_FALSE(bucket_name_.empty());
    service_account_ =
        google::cloud::internal::GetEnv(
            "GOOGLE_CLOUD_CPP_STORAGE_TEST_SIGNING_SERVICE_ACCOUNT")
            .value_or("");
    ASSERT_FALSE(service_account_.empty());
  }

  std::string bucket_name_;
  std::string service_account_;
};

StatusOr<internal::HttpResponse> RetryHttpRequest(
    internal::CurlRequest request, std::string const& payload = {}) {
  StatusOr<internal::HttpResponse> response;
  for (int i = 0; i != 3; ++i) {
    response = request.MakeRequest(payload);
    if (response) return response;
    std::this_thread::sleep_for(std::chrono::seconds(1));
  }
  return response;
};

TEST_F(SignedUrlIntegrationTest, CreateV2SignedUrlGet) {
  // The emulator does not implement signed URLs.
  if (UsingEmulator()) GTEST_SKIP();
  StatusOr<Client> client = MakeIntegrationTestClient();
  ASSERT_STATUS_OK(client);

  auto object_name = MakeRandomObjectName();

  std::string expected = LoremIpsum();

  // Create the object, but only if it does not exist already.
  auto meta = client->InsertObject(bucket_name_, object_name, expected,
                                   IfGenerationMatch(0));
  ASSERT_STATUS_OK(meta);

  StatusOr<std::string> signed_url = client->CreateV2SignedUrl(
      "GET", bucket_name_, object_name, SigningAccount(service_account_));
  ASSERT_STATUS_OK(signed_url);

  // Verify the signed URL can be used to download the object.
  internal::CurlRequestBuilder builder(
      *signed_url, storage::internal::GetDefaultCurlHandleFactory());

  auto response = RetryHttpRequest(builder.BuildRequest());
  ASSERT_STATUS_OK(response);
  EXPECT_EQ(200, response->status_code);

  EXPECT_EQ(expected, response->payload);

  auto deleted = client->DeleteObject(bucket_name_, object_name);
  ASSERT_STATUS_OK(deleted);
}

TEST_F(SignedUrlIntegrationTest, CreateV2SignedUrlPut) {
  // The emulator does not implement signed URLs.
  if (UsingEmulator()) GTEST_SKIP();
  StatusOr<Client> client = MakeIntegrationTestClient();
  ASSERT_STATUS_OK(client);

  auto object_name = MakeRandomObjectName();

  std::string expected = LoremIpsum();

  StatusOr<std::string> signed_url = client->CreateV2SignedUrl(
      "PUT", bucket_name_, object_name, SigningAccount(service_account_),
      ContentType("application/octet-stream"));
  ASSERT_STATUS_OK(signed_url);

  // Verify the signed URL can be used to download the object.
  internal::CurlRequestBuilder builder(
      *signed_url, storage::internal::GetDefaultCurlHandleFactory());
  builder.SetMethod("PUT");
  builder.AddHeader("content-type: application/octet-stream");

  auto response = RetryHttpRequest(builder.BuildRequest(), expected);
  ASSERT_STATUS_OK(response);
  EXPECT_EQ(200, response->status_code);

  auto stream = client->ReadObject(bucket_name_, object_name);
  std::string actual(std::istreambuf_iterator<char>{stream}, {});
  EXPECT_EQ(expected, actual);

  auto deleted = client->DeleteObject(bucket_name_, object_name);
  ASSERT_STATUS_OK(deleted);
}

TEST_F(SignedUrlIntegrationTest, CreateV4SignedUrlGet) {
  // The emulator does not implement signed URLs.
  if (UsingEmulator()) GTEST_SKIP();
  StatusOr<Client> client = MakeIntegrationTestClient();
  ASSERT_STATUS_OK(client);

  auto object_name = MakeRandomObjectName();

  std::string expected = LoremIpsum();

  // Create the object, but only if it does not exist already.
  auto meta = client->InsertObject(bucket_name_, object_name, expected,
                                   IfGenerationMatch(0));
  ASSERT_STATUS_OK(meta);

  StatusOr<std::string> signed_url = client->CreateV4SignedUrl(
      "GET", bucket_name_, object_name, SigningAccount(service_account_));
  ASSERT_STATUS_OK(signed_url);

  // Verify the signed URL can be used to download the object.
  internal::CurlRequestBuilder builder(
      *signed_url, storage::internal::GetDefaultCurlHandleFactory());

  auto response = RetryHttpRequest(builder.BuildRequest());
  ASSERT_STATUS_OK(response);
  EXPECT_EQ(200, response->status_code);

  EXPECT_EQ(expected, response->payload);

  auto deleted = client->DeleteObject(bucket_name_, object_name);
  ASSERT_STATUS_OK(deleted);
}

TEST_F(SignedUrlIntegrationTest, CreateV4SignedUrlPut) {
  // The emulator does not implement signed URLs.
  if (UsingEmulator()) GTEST_SKIP();
  StatusOr<Client> client = MakeIntegrationTestClient();
  ASSERT_STATUS_OK(client);

  auto object_name = MakeRandomObjectName();

  std::string expected = LoremIpsum();

  StatusOr<std::string> signed_url = client->CreateV4SignedUrl(
      "PUT", bucket_name_, object_name, SigningAccount(service_account_));
  ASSERT_STATUS_OK(signed_url);

  // Verify the signed URL can be used to download the object.
  internal::CurlRequestBuilder builder(
      *signed_url, storage::internal::GetDefaultCurlHandleFactory());
  builder.SetMethod("PUT");

  auto response = RetryHttpRequest(builder.BuildRequest(), expected);
  ASSERT_STATUS_OK(response);
  EXPECT_EQ(200, response->status_code);

  auto stream = client->ReadObject(bucket_name_, object_name);
  std::string actual(std::istreambuf_iterator<char>{stream}, {});
  EXPECT_EQ(expected, actual);

  auto deleted = client->DeleteObject(bucket_name_, object_name);
  ASSERT_STATUS_OK(deleted);
}

}  // namespace
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google
