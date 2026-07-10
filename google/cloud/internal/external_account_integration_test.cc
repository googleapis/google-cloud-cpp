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

#include "google/cloud/backoff_policy.h"
#include "google/cloud/common_options.h"
#include "google/cloud/credentials.h"
#include "google/cloud/internal/getenv.h"
#include "google/cloud/internal/rest_client.h"
#include "google/cloud/internal/unified_rest_credentials.h"
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
using ::google::cloud::testing_util::IsOkAndHolds;

auto constexpr kEndpointThatUsesRAB = "storage.googleapis.com";

MATCHER_P(NonEmptyHttpHeaderNameIs, header_name, "has non-empty header named") {
  return header_name == arg.name() && !arg.EmptyValues();
}

StatusOr<std::unique_ptr<rest_internal::RestResponse>> RetryRestRequest(
    std::function<
        StatusOr<std::unique_ptr<rest_internal::RestResponse>>()> const&
        request) {
  auto backoff = google::cloud::ExponentialBackoffPolicy(
      std::chrono::seconds(1), std::chrono::minutes(5), 2.0);
  StatusOr<std::unique_ptr<rest_internal::RestResponse>> response;
  for (auto i = 0; i != 10; ++i) {
    response = request();
    if (response.ok()) return response;
    std::this_thread::sleep_for(backoff.OnCompletion());
  }
  return response;
}

void HandleResponse(std::unique_ptr<rest_internal::RestResponse> response,
                    std::string const& expected_kind) {
  auto response_payload = std::move(*response).ExtractPayload();
  auto payload = rest_internal::ReadAll(std::move(response_payload));
  ASSERT_STATUS_OK(payload);
  auto parsed = nlohmann::json::parse(*payload, nullptr, false);
  ASSERT_TRUE(parsed.is_object()) << "parsed=" << parsed;
  ASSERT_TRUE(parsed.contains("kind")) << "parsed=" << parsed;
  EXPECT_EQ(parsed.value("kind", ""), expected_kind);
}

void MakeStorageRpcCall(Options options) {
  std::string endpoint = "https://storage.googleapis.com";
  auto client =
      rest_internal::MakePooledRestClient(endpoint, std::move(options));
  rest_internal::RestRequest request;
  request.SetPath("storage/v1/b/gcp-public-data-landsat");
  auto response = RetryRestRequest([&] {
    rest_internal::RestContext context;
    return client->Get(context, request);
  });
  ASSERT_STATUS_OK(response);
  HandleResponse(*std::move(response), "storage#bucket");
}

std::string GetExternalAccountCredentialsContents() {
  for (auto const& var :
       {"GOOGLE_CLOUD_CPP_REST_TEST_EXTERNAL_ACCOUNT_KEY_FILE",
        "GOOGLE_APPLICATION_CREDENTIALS"}) {
    auto path = internal::GetEnv(var);
    if (!path.has_value() || path->empty()) continue;
    std::ifstream is(*path);
    auto contents = std::string{std::istreambuf_iterator<char>{is}, {}};
    if (contents.empty()) continue;
    auto parsed = nlohmann::json::parse(contents, nullptr, false);
    if (parsed.is_object() && parsed.value("type", "") == "external_account") {
      return contents;
    }
  }
  return {};
}

TEST(ExternalAccountIntegrationTest, UrlSourced) {
  auto bucket = GetEnv("GOOGLE_CLOUD_CPP_TEST_WIF_BUCKET");
  if (!bucket.has_value()) GTEST_SKIP();

  auto credentials = google::cloud::MakeGoogleDefaultCredentials(
      Options{}.set<LoggingComponentsOption>({"auth", "http"}));
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
      rest_internal::RestContext context;
      auto response = client->Get(context, request);
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

TEST(ExternalAccountIntegrationTest, ExternalAccountCredentials) {
  auto contents = GetExternalAccountCredentialsContents();
  if (contents.empty()) GTEST_SKIP();

  ASSERT_NO_FATAL_FAILURE(
      MakeStorageRpcCall(Options{}.set<UnifiedCredentialsOption>(
          MakeExternalAccountCredentials(contents))));
}

TEST(ExternalAccountIntegrationTest, RABExternalAccountCredentials) {
#ifdef GOOGLE_CLOUD_CPP_TESTING_ENABLE_RAB
  auto contents = GetExternalAccountCredentialsContents();
  if (contents.empty()) GTEST_SKIP();

  auto creds = MakeExternalAccountCredentials(contents);
  auto creds_rest = rest_internal::MapCredentials(*creds);

  auto headers = creds_rest->AuthenticationHeaders(
      std::chrono::system_clock::now(), kEndpointThatUsesRAB);
  EXPECT_THAT(headers,
              IsOkAndHolds(::testing::Contains(
                  NonEmptyHttpHeaderNameIs(std::string{"authorization"}))));

  // x-allowed-locations header is fetched asynchronously.
  for (auto delay : {2, 3, 5}) {
    std::this_thread::sleep_for(std::chrono::seconds(delay));
    headers = creds_rest->AuthenticationHeaders(
        std::chrono::system_clock::now(), kEndpointThatUsesRAB);
    if (headers.ok() && headers->size() > 1) break;
  }

  EXPECT_THAT(headers,
              IsOkAndHolds(::testing::Contains(NonEmptyHttpHeaderNameIs(
                  std::string{"x-allowed-locations"}))));
#else
  GTEST_SKIP();
#endif
}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace oauth2_internal
}  // namespace cloud
}  // namespace google
