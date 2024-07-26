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

#include "google/cloud/storage/oauth2/google_credentials.h"
#include "google/cloud/storage/testing/retry_http_request.h"
#include "google/cloud/storage/testing/storage_integration_test.h"
#include "google/cloud/internal/getenv.h"
#include "google/cloud/internal/rest_request.h"
#include "google/cloud/testing_util/status_matchers.h"
#include "absl/strings/str_split.h"
#include <gmock/gmock.h>
#include <nlohmann/json.hpp>
#include <chrono>
#include <set>
#include <string>
#include <utility>

namespace google {
namespace cloud {
namespace storage {
namespace {

using ::google::cloud::storage::testing::RetryHttpGet;

class ServiceAccountCredentialsTest
    : public google::cloud::storage::testing::StorageIntegrationTest {};

/// @test verify ServiceAccountCredentials create access tokens usable with
/// https://www.googleapis.com/userinfo/v2/me
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

  auto constexpr kUrl = "https://www.googleapis.com/userinfo/v2/me";
  auto factory = [c = *credentials]() {
    auto authorization = c->AuthorizationHeader();
    if (!authorization) return rest_internal::RestRequest();
    std::pair<std::string, std::string> p =
        absl::StrSplit(*authorization, absl::MaxSplits(": ", 1));
    return rest_internal::RestRequest().AddHeader(std::move(p));
  };

  auto response = RetryHttpGet(kUrl, factory);
  ASSERT_STATUS_OK(response);

  auto parsed = nlohmann::json::parse(*response, nullptr, false);
  ASSERT_TRUE(parsed.is_object()) << "payload=" << *response;
  ASSERT_TRUE(parsed.contains("email")) << "payload=" << *response;
}

}  // namespace
}  // namespace storage
}  // namespace cloud
}  // namespace google
