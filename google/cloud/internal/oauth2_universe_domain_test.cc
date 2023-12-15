// Copyright 2024 Google LLC
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

#include "google/cloud/internal/oauth2_universe_domain.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>
#include <nlohmann/json.hpp>

namespace google {
namespace cloud {
namespace oauth2_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::google::cloud::testing_util::IsOkAndHolds;
using ::google::cloud::testing_util::StatusIs;
using ::testing::HasSubstr;

TEST(GetUniverseDomainFromCredentialsJson, NoUniverseDomainField) {
  std::string config = R"""({
      "client_id": "a-client-id.example.com",
      "client_secret": "a-123456ABCDEF",
      "refresh_token": "1/THETOKEN",
      "token_uri": "https://oauth2.googleapis.com/test_endpoint",
      "type": "magic_type"
})""";
  auto credentials = nlohmann::json::parse(config, nullptr, false);
  ASSERT_FALSE(credentials.is_discarded());
  auto universe_domain = GetUniverseDomainFromCredentialsJson(credentials);
  EXPECT_THAT(universe_domain, IsOkAndHolds(GoogleDefaultUniverseDomain()));
}

TEST(GetUniverseDomainFromCredentialsJson, UniverseDomainFieldNotEmpty) {
  std::string config = R"""({
      "client_id": "a-client-id.example.com",
      "client_secret": "a-123456ABCDEF",
      "refresh_token": "1/THETOKEN",
      "token_uri": "https://oauth2.googleapis.com/test_endpoint",
      "type": "magic_type",
      "universe_domain": "my-ud.net"
})""";
  auto credentials = nlohmann::json::parse(config, nullptr, false);
  ASSERT_FALSE(credentials.is_discarded());
  auto universe_domain = GetUniverseDomainFromCredentialsJson(credentials);
  EXPECT_THAT(universe_domain, IsOkAndHolds("my-ud.net"));
}

TEST(GetUniverseDomainFromCredentialsJson, UniverseDomainFieldEmpty) {
  std::string config = R"""({
      "client_id": "a-client-id.example.com",
      "client_secret": "a-123456ABCDEF",
      "refresh_token": "1/THETOKEN",
      "token_uri": "https://oauth2.googleapis.com/test_endpoint",
      "type": "magic_type",
      "universe_domain": ""
})""";
  auto credentials = nlohmann::json::parse(config, nullptr, false);
  ASSERT_FALSE(credentials.is_discarded());
  auto universe_domain = GetUniverseDomainFromCredentialsJson(credentials);
  EXPECT_THAT(
      universe_domain,
      StatusIs(
          StatusCode::kInvalidArgument,
          HasSubstr(
              "universe_domain field in credentials file cannot be empty")));
}

TEST(GetUniverseDomainFromCredentialsJson, UniverseDomainFieldIncorrectType) {
  std::string config = R"""({
      "client_id": "a-client-id.example.com",
      "client_secret": "a-123456ABCDEF",
      "refresh_token": "1/THETOKEN",
      "token_uri": "https://oauth2.googleapis.com/test_endpoint",
      "type": "magic_type",
      "universe_domain": true
})""";
  auto credentials = nlohmann::json::parse(config, nullptr, false);
  ASSERT_FALSE(credentials.is_discarded());
  auto universe_domain = GetUniverseDomainFromCredentialsJson(credentials);
  EXPECT_THAT(universe_domain,
              StatusIs(StatusCode::kInvalidArgument,
                       HasSubstr("Invalid type for universe_domain field in "
                                 "credentials; expected string")));
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace oauth2_internal
}  // namespace cloud
}  // namespace google
