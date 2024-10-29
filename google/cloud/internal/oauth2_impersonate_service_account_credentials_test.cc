// Copyright 2021 Google LLC
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

#include "google/cloud/internal/oauth2_impersonate_service_account_credentials.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>
#include <nlohmann/json.hpp>
#include <memory>

namespace google {
namespace cloud {
namespace oauth2_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::google::cloud::AccessToken;
using ::google::cloud::AccessTokenLifetimeOption;
using ::google::cloud::testing_util::IsOk;
using ::google::cloud::testing_util::IsOkAndHolds;
using ::google::cloud::testing_util::StatusIs;
using ::std::chrono::minutes;
using ::std::chrono::seconds;
using ::testing::AllOf;
using ::testing::ElementsAre;
using ::testing::HasSubstr;
using ::testing::Optional;
using ::testing::Return;

auto constexpr kFullValidConfig = R"""({
  "service_account_impersonation_url": "https://iamcredentials.googleapis.com/v1/projects/-/serviceAccounts/sa3@developer.gserviceaccount.com:generateAccessToken",
  "delegates": [
    "sa1@developer.gserviceaccount.com",
    "sa2@developer.gserviceaccount.com"
  ],
  "quota_project_id": "my-project",
  "source_credentials": {
    "type": "authorized_user"
  },
  "type": "impersonated_service_account"
})""";

TEST(ParseImpersonatedServiceAccountCredentials, Success) {
  auto actual =
      ParseImpersonatedServiceAccountCredentials(kFullValidConfig, "test-data");
  ASSERT_STATUS_OK(actual);
  EXPECT_EQ(actual->service_account, "sa3@developer.gserviceaccount.com");
  EXPECT_THAT(actual->delegates,
              ElementsAre("sa1@developer.gserviceaccount.com",
                          "sa2@developer.gserviceaccount.com"));
  EXPECT_THAT(actual->quota_project_id, Optional<std::string>("my-project"));
  EXPECT_THAT(actual->source_credentials,
              AllOf(HasSubstr("type"), HasSubstr("authorized_user")));
}

TEST(ParseImpersonatedServiceAccountCredentials, MissingRequiredFieldsError) {
  for (auto const& required_field :
       {"service_account_impersonation_url", "source_credentials"}) {
    auto json = nlohmann::json::parse(kFullValidConfig);
    json.erase(required_field);
    auto actual = ParseImpersonatedServiceAccountCredentials(json.dump(), "");
    EXPECT_THAT(actual, StatusIs(StatusCode::kInvalidArgument,
                                 AllOf(HasSubstr("Missing"),
                                       HasSubstr(required_field))));
  }
}

TEST(ParseImpersonatedServiceAccountCredentials, MissingOptionalFieldsIsOk) {
  for (auto const& optional_field : {"delegates", "quota_project_id"}) {
    auto json = nlohmann::json::parse(kFullValidConfig);
    json.erase(optional_field);
    auto actual = ParseImpersonatedServiceAccountCredentials(json.dump(), "");
    EXPECT_STATUS_OK(actual);
  }
}

TEST(ParseImpersonatedServiceAccountCredentials, MalformedServiceAccountPath) {
  auto json = nlohmann::json::parse(kFullValidConfig);
  json["service_account_impersonation_url"] = "not-a-valid-url-path";
  auto actual = ParseImpersonatedServiceAccountCredentials(json.dump(), "");
  EXPECT_THAT(actual,
              StatusIs(StatusCode::kInvalidArgument,
                       AllOf(HasSubstr("Malformed"),
                             HasSubstr("service_account_impersonation_url"),
                             HasSubstr("contents"))));
}

TEST(ParseImpersonatedServiceAccountCredentials, MalformedJsonFields) {
  for (auto const& non_int_field :
       {"service_account_impersonation_url", "delegates", "quota_project_id",
        "source_credentials"}) {
    auto json = nlohmann::json::parse(kFullValidConfig);
    json[non_int_field] = 0;  // Provide an unexpected JSON type
    auto actual = ParseImpersonatedServiceAccountCredentials(json.dump(), "");
    EXPECT_THAT(actual, StatusIs(StatusCode::kInvalidArgument,
                                 AllOf(HasSubstr("Malformed"),
                                       HasSubstr(non_int_field))));
  }
}

class MockMinimalIamCredentialsRest : public MinimalIamCredentialsRest {
 public:
  MOCK_METHOD(StatusOr<google::cloud::AccessToken>, GenerateAccessToken,
              (GenerateAccessTokenRequest const&), (override));
  MOCK_METHOD(StatusOr<std::string>, universe_domain, (Options const& options),
              (override, const));
};

TEST(ImpersonateServiceAccountCredentialsTest, Basic) {
  auto const now = std::chrono::system_clock::now();

  auto constexpr kExpectedUniverseDomain = "my-ud.net";
  StatusOr<std::string> universe_domain{kExpectedUniverseDomain};
  auto mock = std::make_shared<MockMinimalIamCredentialsRest>();
  EXPECT_CALL(*mock, GenerateAccessToken)
      .WillOnce(
          Return(make_status_or(AccessToken{"token1", now + minutes(30)})))
      .WillOnce(Return(Status(StatusCode::kPermissionDenied, "")));
  EXPECT_CALL(*mock, universe_domain).WillOnce(Return(universe_domain));

  auto config = google::cloud::internal::ImpersonateServiceAccountConfig(
      google::cloud::MakeGoogleDefaultCredentials(),
      "test-only-invalid@test.invalid",
      Options{}.set<AccessTokenLifetimeOption>(std::chrono::minutes(15)));

  ImpersonateServiceAccountCredentials under_test(config, mock);

  auto universe_domain_result = under_test.universe_domain({});
  EXPECT_THAT(universe_domain_result, IsOkAndHolds(kExpectedUniverseDomain));

  auto token = under_test.GetToken(now + minutes(1));
  ASSERT_THAT(token, IsOk());
  EXPECT_THAT(token->token, "token1");

  token = under_test.GetToken(now + minutes(45));
  ASSERT_THAT(token, StatusIs(StatusCode::kPermissionDenied));
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace oauth2_internal
}  // namespace cloud
}  // namespace google
