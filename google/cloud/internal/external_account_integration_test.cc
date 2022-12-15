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

#include "google/cloud/common_options.h"
#include "google/cloud/credentials.h"
#include "google/cloud/internal/getenv.h"
#include "google/cloud/internal/rest_client.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>
#include <nlohmann/json.hpp>
#include <fstream>
#include <thread>

namespace google {
namespace cloud {
namespace oauth2_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::google::cloud::internal::GetEnv;

TEST(ExternalAccountIntegrationTest, UrlSourced) {
  auto bucket = GetEnv("GOOGLE_CLOUD_CPP_TEST_WIF_BUCKET");
  if (!bucket.has_value()) GTEST_SKIP();

  auto credentials = google::cloud::MakeGoogleDefaultCredentials(
      Options{}.set<TracingComponentsOption>({"auth", "http"}));
  auto client = rest_internal::MakeDefaultRestClient(
      "https://storage.googleapis.com/",
      Options{}.set<UnifiedCredentialsOption>(credentials));

  auto request = rest_internal::RestRequest("storage/v1/b/" + *bucket);
  // Anything involving HTTP requests may fail and needs a retry loop.
  auto now = std::chrono::system_clock::now();
  auto get_payload = [&](auto response) -> StatusOr<std::string> {
    if (!response) return std::move(response).status();
    if (!rest_internal::IsHttpSuccess(**response)) {
      return AsStatus(std::move(**response));
    }
    return rest_internal::ReadAll(std::move(**response).ExtractPayload());
  };
  auto payload = [&]() -> StatusOr<std::string> {
    Status last_status;
    auto delay = std::chrono::seconds(1);
    for (int i = 0; i != 5; ++i) {
      if (i != 0) std::this_thread::sleep_for(delay);
      now = std::chrono::system_clock::now();
      auto response = client->Get(request);
      auto payload = get_payload(std::move(response));
      if (payload) return payload;
      last_status = std::move(payload).status();
      delay *= 2;
    }
    return last_status;
  }();
  ASSERT_STATUS_OK(payload);
  auto metadata = nlohmann::json::parse(*payload, nullptr, false);
  ASSERT_TRUE(metadata.is_object()) << "type=" << metadata.type_name();
  EXPECT_EQ(metadata.value("kind", ""), "storage#bucket");
  EXPECT_EQ(metadata.value("id", ""), *bucket);
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace oauth2_internal
}  // namespace cloud
}  // namespace google
