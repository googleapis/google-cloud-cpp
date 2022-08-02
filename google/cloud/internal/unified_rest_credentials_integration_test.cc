// Copyright 2022 Google LLC
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

#include "google/cloud/credentials.h"
#include "google/cloud/internal/getenv.h"
#include "google/cloud/internal/oauth2_google_credentials.h"
#include "google/cloud/internal/oauth2_minimal_iam_credentials_rest.h"
#include "google/cloud/internal/rest_client.h"
#include "google/cloud/log.h"
#include "google/cloud/testing_util/scoped_environment.h"
#include "google/cloud/testing_util/status_matchers.h"
#include "absl/strings/str_split.h"
#include <gmock/gmock.h>
#include <nlohmann/json.hpp>
#include <fstream>

namespace google {
namespace cloud {
namespace rest_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::google::cloud::testing_util::IsOk;
using ::google::cloud::testing_util::ScopedEnvironment;

StatusOr<std::unique_ptr<RestResponse>> RetryRestRequest(
    std::function<StatusOr<std::unique_ptr<RestResponse>>()> const& request) {
  auto delay = std::chrono::seconds(1);
  StatusOr<std::unique_ptr<RestResponse>> response;
  for (auto i = 0; i != 3; ++i) {
    response = request();
    if (response.ok()) return response;
    std::this_thread::sleep_for(delay);
    delay *= 2;
  }
  return response;
}

// BigQuery is a common REST API.
void MakeBigQueryRpcCall(Options options) {
  std::string bigquery_endpoint = "https://bigquery.googleapis.com";
  auto client = MakePooledRestClient(bigquery_endpoint, std::move(options));
  RestRequest request;
  request.SetPath("bigquery/v2/projects/bigquery-public-data/datasets");
  request.AddQueryParameter({"maxResults", "10"});
  auto response = RetryRestRequest([&] { return client->Get(request); });
  ASSERT_STATUS_OK(response);

  auto response_payload = std::move(**response).ExtractPayload();
  auto payload = ReadAll(std::move(response_payload));
  ASSERT_STATUS_OK(payload);
  auto parsed = nlohmann::json::parse(*payload, nullptr, false);
  ASSERT_TRUE(parsed.is_object());
  ASSERT_TRUE(parsed.contains("kind"));
  EXPECT_EQ(parsed.value("kind", ""), "bigquery#datasetList");
}

// Storage has a fully public bucket which we can use to test insecure
// credentials.
void MakeStorageRpcCall(Options options) {
  std::string endpoint = "https://storage.googleapis.com";
  auto client = MakePooledRestClient(endpoint, std::move(options));
  RestRequest request;
  request.SetPath("storage/v1/b/gcp-public-data-landsat");
  auto response = RetryRestRequest([&] { return client->Get(request); });
  ASSERT_STATUS_OK(response);

  auto response_payload = std::move(**response).ExtractPayload();
  auto payload = ReadAll(std::move(response_payload));
  ASSERT_STATUS_OK(payload);
  auto parsed = nlohmann::json::parse(*payload, nullptr, false);
  ASSERT_TRUE(parsed.is_object());
  ASSERT_TRUE(parsed.contains("kind"));
  EXPECT_EQ(parsed.value("kind", ""), "storage#bucket");
}

TEST(UnifiedRestCredentialsIntegrationTest, InsecureCredentials) {
  ASSERT_NO_FATAL_FAILURE(MakeStorageRpcCall(
      Options{}.set<UnifiedCredentialsOption>(MakeInsecureCredentials())));
}

TEST(UnifiedRestCredentialsIntegrationTest, GoogleDefaultCredentials) {
  ASSERT_NO_FATAL_FAILURE(MakeStorageRpcCall(
      Options{}.set<UnifiedCredentialsOption>(MakeGoogleDefaultCredentials())));
}

TEST(UnifiedRestCredentialsIntegrationTest, AccessTokenCredentials) {
  auto env = internal::GetEnv("GOOGLE_CLOUD_CPP_REST_TEST_KEY_FILE_JSON");
  ASSERT_TRUE(env.has_value());
  auto service_account =
      internal::GetEnv("GOOGLE_CLOUD_CPP_REST_TEST_SIGNING_SERVICE_ACCOUNT");
  ASSERT_TRUE(service_account.has_value());
  // Use the IAM service to create an access token, and then make a request
  // using that access token.
  std::string key_file = std::move(*env);
  ScopedEnvironment google_app_creds_override_env_var(
      "GOOGLE_APPLICATION_CREDENTIALS", key_file);
  auto default_creds = oauth2_internal::GoogleDefaultCredentials();
  ASSERT_THAT(default_creds, IsOk());
  auto iam_creds =
      oauth2_internal::MakeMinimalIamCredentialsRestStub(*default_creds);
  oauth2_internal::GenerateAccessTokenRequest request;
  request.lifetime = std::chrono::hours(1);
  request.service_account = std::move(*service_account);
  request.scopes.emplace_back("https://www.googleapis.com/auth/cloud-platform");
  auto token = iam_creds->GenerateAccessToken(request);
  ASSERT_STATUS_OK(token);

  ASSERT_NO_FATAL_FAILURE(
      MakeBigQueryRpcCall(Options{}.set<UnifiedCredentialsOption>(
          MakeAccessTokenCredentials(token->token, token->expiration))));
}

TEST(UnifiedRestCredentialsIntegrationTest,
     ImpersonateServiceAccountCredentials) {
  auto env = internal::GetEnv("GOOGLE_CLOUD_CPP_REST_TEST_KEY_FILE_JSON");
  ASSERT_TRUE(env.has_value());
  std::string key_file = std::move(*env);
  ScopedEnvironment google_app_creds_override_env_var(
      "GOOGLE_APPLICATION_CREDENTIALS", key_file);
  auto service_account =
      internal::GetEnv("GOOGLE_CLOUD_CPP_REST_TEST_SIGNING_SERVICE_ACCOUNT");
  ASSERT_TRUE(service_account.has_value());

  ASSERT_NO_FATAL_FAILURE(
      MakeBigQueryRpcCall(Options{}.set<UnifiedCredentialsOption>(
          MakeImpersonateServiceAccountCredentials(
              MakeGoogleDefaultCredentials(), *service_account))));
}

TEST(UnifiedRestCredentialsIntegrationTest, ServiceAccountCredentials) {
  ScopedEnvironment self_signed_jwt(
      "GOOGLE_CLOUD_CPP_EXPERIMENTAL_DISABLE_SELF_SIGNED_JWT", "1");

  auto env = internal::GetEnv("GOOGLE_CLOUD_CPP_REST_TEST_KEY_FILE_JSON");
  ASSERT_TRUE(env.has_value());
  std::string key_file = std::move(*env);
  std::ifstream is(key_file);
  auto contents = std::string{std::istreambuf_iterator<char>{is}, {}};

  ASSERT_NO_FATAL_FAILURE(
      MakeBigQueryRpcCall(Options{}.set<UnifiedCredentialsOption>(
          MakeServiceAccountCredentials(contents))));
}

TEST(UnifiedRestCredentialsIntegrationTest, StorageGoogleDefaultCredentials) {
  ASSERT_NO_FATAL_FAILURE(MakeStorageRpcCall(
      Options{}.set<UnifiedCredentialsOption>(MakeGoogleDefaultCredentials())));
}

TEST(UnifiedRestCredentialsIntegrationTest, StorageServiceAccount) {
  ScopedEnvironment self_signed_jwt(
      "GOOGLE_CLOUD_CPP_EXPERIMENTAL_DISABLE_SELF_SIGNED_JWT", "1");

  auto project = internal::GetEnv("GOOGLE_CLOUD_PROJECT");
  ASSERT_TRUE(project.has_value());
  auto env = internal::GetEnv("GOOGLE_CLOUD_CPP_REST_TEST_KEY_FILE_JSON");
  ASSERT_TRUE(env.has_value());
  std::string key_file = std::move(*env);
  std::ifstream is(key_file);
  auto contents = std::string{std::istreambuf_iterator<char>{is}, {}};

  ASSERT_NO_FATAL_FAILURE(
      MakeStorageRpcCall(Options{}.set<UnifiedCredentialsOption>(
          MakeServiceAccountCredentials(contents))));
}

TEST(UnifiedRestCredentialsIntegrationTest, BigQuerySelfSignedJWT) {
  ScopedEnvironment self_signed_jwt(
      "GOOGLE_CLOUD_CPP_EXPERIMENTAL_DISABLE_SELF_SIGNED_JWT", absl::nullopt);

  auto env = internal::GetEnv("GOOGLE_CLOUD_CPP_REST_TEST_KEY_FILE_JSON");
  ASSERT_TRUE(env.has_value());
  std::ifstream is(*env);
  auto contents = std::string{std::istreambuf_iterator<char>{is}, {}};
  ASSERT_NO_FATAL_FAILURE(
      MakeBigQueryRpcCall(Options{}.set<UnifiedCredentialsOption>(
          MakeServiceAccountCredentials(contents))));
}

TEST(UnifiedRestCredentialsIntegrationTest, StorageSelfSignedJWT) {
  ScopedEnvironment self_signed_jwt(
      "GOOGLE_CLOUD_CPP_EXPERIMENTAL_DISABLE_SELF_SIGNED_JWT", absl::nullopt);

  auto env = internal::GetEnv("GOOGLE_CLOUD_CPP_REST_TEST_KEY_FILE_JSON");
  ASSERT_TRUE(env.has_value());
  std::ifstream is(*env);
  auto contents = std::string{std::istreambuf_iterator<char>{is}, {}};
  ASSERT_NO_FATAL_FAILURE(
      MakeStorageRpcCall(Options{}.set<UnifiedCredentialsOption>(
          MakeServiceAccountCredentials(contents))));
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace rest_internal
}  // namespace cloud
}  // namespace google
