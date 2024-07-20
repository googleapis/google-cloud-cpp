// Copyright 2019 Google LLC
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
#include "google/cloud/storage/testing/retry_http_request.h"
#include "google/cloud/storage/testing/storage_integration_test.h"
#include "google/cloud/internal/getenv.h"
#include "google/cloud/internal/rest_request.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace storage {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::google::cloud::storage::testing::RetryHttpGet;
using ::google::cloud::testing_util::IsOkAndHolds;

constexpr auto kJsonEnvVar = "GOOGLE_CLOUD_CPP_STORAGE_TEST_KEY_FILE_JSON";
constexpr auto kP12EnvVar = "GOOGLE_CLOUD_CPP_STORAGE_TEST_KEY_FILE_P12";

class KeyFileIntegrationTest
    : public google::cloud::storage::testing::StorageIntegrationTest,
      public ::testing::WithParamInterface<std::string> {
 protected:
  void SetUp() override {
    // The emulator does not implement signed URLs.
    if (UsingEmulator()) GTEST_SKIP();

    std::string const env = GetParam();
    key_filename_ = google::cloud::internal::GetEnv(env.c_str()).value_or("");
    // The p12 test is only run on certain platforms, so if it's not set, skip
    // the test rather than failing.
    if (key_filename_.empty() && env == kP12EnvVar) {
      GTEST_SKIP() << "Skipping because ${" << env << "} is not set";
    }
    ASSERT_FALSE(key_filename_.empty())
        << "Expected non-empty value for ${" << env << "}";

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
  std::string key_filename_;
  std::string service_account_;
};

TEST_P(KeyFileIntegrationTest, ObjectWriteSignAndReadDefaultAccount) {
  if (UsingGrpc()) GTEST_SKIP();

  auto credentials =
      oauth2::CreateServiceAccountCredentialsFromFilePath(key_filename_);
  ASSERT_STATUS_OK(credentials);

  auto client = MakeIntegrationTestClient(
      Options{}.set<Oauth2CredentialsOption>(*credentials));
  auto object_name = MakeRandomObjectName();
  std::string expected = LoremIpsum();

  // Create the object, but only if it does not exist already.
  auto meta = client.InsertObject(bucket_name_, object_name, expected,
                                  IfGenerationMatch(0));
  ASSERT_STATUS_OK(meta);
  ScheduleForDelete(*meta);

  StatusOr<std::string> signed_url =
      client.CreateV4SignedUrl("GET", bucket_name_, object_name);
  ASSERT_STATUS_OK(signed_url);

  // Verify the signed URL can be used to download the object.
  auto response =
      RetryHttpGet(*signed_url, [] { return rest_internal::RestRequest(); });
  EXPECT_THAT(response, IsOkAndHolds(expected));
}

TEST_P(KeyFileIntegrationTest, ObjectWriteSignAndReadExplicitAccount) {
  if (UsingGrpc()) GTEST_SKIP();

  auto credentials =
      oauth2::CreateServiceAccountCredentialsFromFilePath(key_filename_);
  ASSERT_STATUS_OK(credentials);

  auto client = MakeIntegrationTestClient(
      Options{}.set<Oauth2CredentialsOption>(*credentials));
  auto object_name = MakeRandomObjectName();
  std::string expected = LoremIpsum();

  // Create the object, but only if it does not exist already.
  auto meta = client.InsertObject(bucket_name_, object_name, expected,
                                  IfGenerationMatch(0));
  ASSERT_STATUS_OK(meta);
  ScheduleForDelete(*meta);

  StatusOr<std::string> signed_url = client.CreateV4SignedUrl(
      "GET", bucket_name_, object_name, SigningAccount(service_account_));
  ASSERT_STATUS_OK(signed_url);

  // Verify the signed URL can be used to download the object.
  auto response =
      RetryHttpGet(*signed_url, [] { return rest_internal::RestRequest(); });
  EXPECT_THAT(response, IsOkAndHolds(expected));
}

INSTANTIATE_TEST_SUITE_P(KeyFileJsonTest, KeyFileIntegrationTest,
                         ::testing::Values(kJsonEnvVar));
INSTANTIATE_TEST_SUITE_P(KeyFileP12Test, KeyFileIntegrationTest,
                         ::testing::Values(kP12EnvVar));

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google
