// Copyright 2023 Google LLC
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

#include "google/cloud/storage/internal/curl_request_builder.h"
#include "google/cloud/storage/oauth2/google_credentials.h"
#include "google/cloud/storage/testing/storage_integration_test.h"
#include "google/cloud/internal/getenv.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>
#include <nlohmann/json.hpp>
#include <chrono>
#include <functional>
#include <thread>

namespace google {
namespace cloud {
namespace storage {
namespace {

class ServiceAccountCredentialsTest
    : public google::cloud::storage::testing::StorageIntegrationTest {};

StatusOr<internal::HttpResponse> RetryHttpRequest(
    std::function<internal::CurlRequest()> const& factory) {
  StatusOr<internal::HttpResponse> response;
  for (int i = 0; i != 3; ++i) {
    response = factory().MakeRequest({});
    if (response) return response;
    std::this_thread::sleep_for(std::chrono::seconds(1));
  }
  return response;
}

TEST_F(ServiceAccountCredentialsTest, UserInfoOAuth2) {
  auto filename = google::cloud::internal::GetEnv(
      "GOOGLE_CLOUD_CPP_STORAGE_TEST_KEY_FILE_JSON");
  if (UsingEmulator() || !filename) GTEST_SKIP();

  auto credentials = oauth2::CreateServiceAccountCredentialsFromFilePath(
      *filename, /*scopes=*/
      std::set<std::string>({"https://www.googleapis.com/auth/userinfo.email",
                             "https://www.googleapis.com/auth/cloud-platform"}),
      /*subject=*/absl::nullopt);
  ASSERT_STATUS_OK(credentials);

  auto factory = [c = *credentials]() {
    auto constexpr kUrl = "https://www.googleapis.com/userinfo/v2/me";
    internal::CurlRequestBuilder builder(
        kUrl, storage::internal::GetDefaultCurlHandleFactory());
    auto authorization = c->AuthorizationHeader();
    if (authorization) builder.AddHeader(*authorization);
    return std::move(builder).BuildRequest();
  };

  auto response = RetryHttpRequest(factory);
  ASSERT_STATUS_OK(response);
  EXPECT_EQ(200, response->status_code);

  auto parsed = nlohmann::json::parse(response->payload, nullptr, false);
  ASSERT_TRUE(parsed.is_object()) << "payload=" << response->payload;
  ASSERT_TRUE(parsed.contains("email")) << "payload=" << response->payload;
}

}  // namespace
}  // namespace storage
}  // namespace cloud
}  // namespace google
