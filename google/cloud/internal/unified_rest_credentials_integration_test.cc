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
#include "google/cloud/internal/curl_options.h"
#include "google/cloud/internal/getenv.h"
#include "google/cloud/internal/oauth2_google_credentials.h"
#include "google/cloud/internal/rest_client.h"
#include "google/cloud/internal/setenv.h"
#include "google/cloud/log.h"
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
using ::google::cloud::testing_util::StatusIs;

class UnifiedRestCredentialsIntegrationTest : public ::testing::Test {
 protected:
  void SetUp() override {
    key_file_ = internal::GetEnv("GOOGLE_CLOUD_CPP_REST_TEST_KEY_FILE_JSON");
  }

  void TearDown() override {
    internal::UnsetEnv("GOOGLE_APPLICATION_CREDENTIALS");
  }

  static StatusOr<std::unique_ptr<RestResponse>> RetryRestRequest(
      std::function<StatusOr<std::unique_ptr<RestResponse>>()> const& request,
      StatusCode expected_status = StatusCode::kOk) {
    auto delay = std::chrono::seconds(1);
    StatusOr<std::unique_ptr<RestResponse>> response;
    for (auto i = 0; i != 3; ++i) {
      response = request();
      if (response.status().code() == expected_status) return response;
      std::this_thread::sleep_for(delay);
      delay *= 2;
    }
    return response;
  }

  void MakeRestRpcCall(StatusCode expected_status) {
    std::string bigquery_endpoint = "https://bigquery.googleapis.com";
    auto client = GetPooledRestClient(bigquery_endpoint, options_);
    RestRequest request;
    request.SetPath("bigquery/v2/projects/bigquery-public-data/datasets");
    request.AddQueryParameter({"maxResults", "10"});
    auto response = RetryRestRequest([&] { return client->Get(request); });
    ASSERT_THAT(response, IsOk());
    auto response_payload = std::move(**response).ExtractPayload();
    auto response_json = ReadAll(std::move(response_payload));
    ASSERT_THAT(response_json, StatusIs(expected_status));
    if (response_json.ok()) {
      auto parsed_response =
          nlohmann::json::parse(*response_json, nullptr, false);
      ASSERT_TRUE(parsed_response.is_object());
      auto kind = parsed_response.find("kind");
      ASSERT_NE(kind, parsed_response.end());
      EXPECT_EQ(std::string(kind.value()), "bigquery#datasetList");
    }
  }

  absl::optional<std::string> key_file_;
  Options options_;
};

TEST_F(UnifiedRestCredentialsIntegrationTest, InsecureCredentials) {
  options_.set<UnifiedCredentialsOption>(MakeInsecureCredentials());
  std::string bigquery_endpoint = "https://bigquery.googleapis.com";
  auto client = GetPooledRestClient(bigquery_endpoint, options_);
  RestRequest request;
  request.SetPath("bigquery/v2/projects/bigquery-public-data/datasets");
  request.AddQueryParameter({"maxResults", "10"});
  auto response = RetryRestRequest([&] { return client->Get(request); });
  EXPECT_THAT(response, StatusIs(StatusCode::kOk));
  auto response_payload = std::move(**response).ExtractPayload();
  auto response_json = ReadAll(std::move(response_payload));
  ASSERT_THAT(response_json, StatusIs(StatusCode::kUnauthenticated));
}

TEST_F(UnifiedRestCredentialsIntegrationTest, GoogleDefaultCredentials) {
  options_.set<UnifiedCredentialsOption>(MakeGoogleDefaultCredentials());
  MakeRestRpcCall(StatusCode::kOk);
}

TEST_F(UnifiedRestCredentialsIntegrationTest, AccessTokenCredentials) {
  ASSERT_TRUE(key_file_);
  internal::SetEnv("GOOGLE_APPLICATION_CREDENTIALS", *key_file_);
  auto default_creds = oauth2_internal::GoogleDefaultCredentials();
  auto header = default_creds.value()->AuthorizationHeader();
  // header is in format pair("Authorization", "Bearer <token>");
  std::pair<std::string, std::string> bearer_token =
      absl::StrSplit(header->second, absl::ByChar(' '));
  auto expiration = std::chrono::system_clock::now() + std::chrono::hours(1);
  options_.set<UnifiedCredentialsOption>(
      MakeAccessTokenCredentials(bearer_token.second, expiration));
  MakeRestRpcCall(StatusCode::kOk);
}

TEST_F(UnifiedRestCredentialsIntegrationTest,
       ImpersonateServiceAccountCredentials) {
  ASSERT_TRUE(key_file_);
  internal::SetEnv("GOOGLE_APPLICATION_CREDENTIALS", *key_file_);
  auto service_account =
      internal::GetEnv("GOOGLE_CLOUD_CPP_REST_TEST_SIGNING_SERVICE_ACCOUNT");
  ASSERT_TRUE(service_account.has_value());
  options_.set<UnifiedCredentialsOption>(
      MakeImpersonateServiceAccountCredentials(MakeGoogleDefaultCredentials(),
                                               *service_account));
  MakeRestRpcCall(StatusCode::kOk);
}

TEST_F(UnifiedRestCredentialsIntegrationTest, ServiceAccountCredentials) {
  ASSERT_TRUE(key_file_);
  std::ifstream is(*key_file_);
  auto contents = std::string{std::istreambuf_iterator<char>{is}, {}};
  options_.set<UnifiedCredentialsOption>(
      MakeServiceAccountCredentials(contents));
  MakeRestRpcCall(StatusCode::kOk);
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace rest_internal
}  // namespace cloud
}  // namespace google
