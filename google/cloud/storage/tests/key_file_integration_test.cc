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

class KeyFileIntegrationTest
    : public google::cloud::storage::testing::StorageIntegrationTest,
      public ::testing::WithParamInterface<std::string> {
 protected:
  void SetUp() override {
    // The emulator does not implement signed URLs.
    if (UsingEmulator()) GTEST_SKIP();

    std::string const key_file_envvar = GetParam();

    bucket_name_ = google::cloud::internal::GetEnv(
                       "GOOGLE_CLOUD_CPP_STORAGE_TEST_BUCKET_NAME")
                       .value_or("");
    ASSERT_FALSE(bucket_name_.empty());
    key_filename_ =
        google::cloud::internal::GetEnv(key_file_envvar.c_str()).value_or("");
    ASSERT_FALSE(key_filename_.empty())
        << " expected non-empty value for ${" << key_file_envvar << "}";
    service_account_ =
        google::cloud::internal::GetEnv(
            "GOOGLE_CLOUD_CPP_STORAGE_TEST_SIGNING_SERVICE_ACCOUNT")
            .value_or("");
    ASSERT_FALSE(service_account_.empty());
  }

  std::string bucket_name_;
  std::string key_filename_;
  std::string service_account_;
};

StatusOr<internal::HttpResponse> RetryHttpRequest(
    internal::CurlRequest request) {
  StatusOr<internal::HttpResponse> response;
  for (int i = 0; i != 3; ++i) {
    response = request.MakeRequest({});
    if (response) return response;
    std::this_thread::sleep_for(std::chrono::seconds(1));
  }
  return response;
};

TEST_P(KeyFileIntegrationTest, ObjectWriteSignAndReadDefaultAccount) {
  auto credentials =
      oauth2::CreateServiceAccountCredentialsFromFilePath(key_filename_);
  ASSERT_STATUS_OK(credentials);

  Client client(Options{}.set<Oauth2CredentialsOption>(*credentials));

  auto object_name = MakeRandomObjectName();
  std::string expected = LoremIpsum();

  // Create the object, but only if it does not exist already.
  auto meta = client.InsertObject(bucket_name_, object_name, expected,
                                  IfGenerationMatch(0));
  ASSERT_STATUS_OK(meta);

  StatusOr<std::string> signed_url =
      client.CreateV4SignedUrl("GET", bucket_name_, object_name);
  ASSERT_STATUS_OK(signed_url);

  // Verify the signed URL can be used to download the object.
  internal::CurlRequestBuilder builder(
      *signed_url, storage::internal::GetDefaultCurlHandleFactory());

  auto response = RetryHttpRequest(builder.BuildRequest());
  ASSERT_STATUS_OK(response);
  EXPECT_EQ(200, response->status_code);

  EXPECT_EQ(expected, response->payload);

  auto deleted = client.DeleteObject(bucket_name_, object_name);
  ASSERT_STATUS_OK(deleted);
}

TEST_P(KeyFileIntegrationTest, ObjectWriteSignAndReadExplicitAccount) {
  auto credentials =
      oauth2::CreateServiceAccountCredentialsFromFilePath(key_filename_);
  ASSERT_STATUS_OK(credentials);

  Client client(Options{}.set<Oauth2CredentialsOption>(*credentials));

  auto object_name = MakeRandomObjectName();
  std::string expected = LoremIpsum();

  // Create the object, but only if it does not exist already.
  auto meta = client.InsertObject(bucket_name_, object_name, expected,
                                  IfGenerationMatch(0));
  ASSERT_STATUS_OK(meta);

  StatusOr<std::string> signed_url = client.CreateV4SignedUrl(
      "GET", bucket_name_, object_name, SigningAccount(service_account_));
  ASSERT_STATUS_OK(signed_url);

  // Verify the signed URL can be used to download the object.
  internal::CurlRequestBuilder builder(
      *signed_url, storage::internal::GetDefaultCurlHandleFactory());

  auto response = RetryHttpRequest(builder.BuildRequest());
  ASSERT_STATUS_OK(response);
  EXPECT_EQ(200, response->status_code);

  EXPECT_EQ(expected, response->payload);

  auto deleted = client.DeleteObject(bucket_name_, object_name);
  ASSERT_STATUS_OK(deleted);
}

INSTANTIATE_TEST_SUITE_P(
    KeyFileJsonTest, KeyFileIntegrationTest,
    ::testing::Values("GOOGLE_CLOUD_CPP_STORAGE_TEST_KEY_FILE_JSON"));
INSTANTIATE_TEST_SUITE_P(
    KeyFileP12Test, KeyFileIntegrationTest,
    ::testing::Values("GOOGLE_CLOUD_CPP_STORAGE_TEST_KEY_FILE_P12"));

}  // namespace
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google
