// Copyright 2018 Google LLC
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

#include "google/cloud/internal/oauth2_authorized_user_credentials.h"
#include "google/cloud/internal/oauth2_credential_constants.h"
#include "google/cloud/internal/oauth2_universe_domain.h"
#include "google/cloud/testing_util/chrono_output.h"
#include "google/cloud/testing_util/mock_http_payload.h"
#include "google/cloud/testing_util/mock_rest_client.h"
#include "google/cloud/testing_util/mock_rest_response.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>
#include <nlohmann/json.hpp>

namespace google {
namespace cloud {
namespace oauth2_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::google::cloud::rest_internal::RestRequest;
using ::google::cloud::rest_internal::RestResponse;
using ::google::cloud::testing_util::MakeMockHttpPayloadSuccess;
using ::google::cloud::testing_util::MockRestClient;
using ::google::cloud::testing_util::MockRestResponse;
using ::google::cloud::testing_util::StatusIs;
using ::testing::_;
using ::testing::AllOf;
using ::testing::ByMove;
using ::testing::HasSubstr;
using ::testing::IsSupersetOf;
using ::testing::MatcherCast;
using ::testing::Not;
using ::testing::Pair;
using ::testing::Property;
using ::testing::Return;

using MockHttpClientFactory =
    ::testing::MockFunction<std::unique_ptr<rest_internal::RestClient>(
        Options const&)>;

class AuthorizedUserCredentialsTest : public ::testing::Test {
 protected:
  void SetUp() override {
    mock_rest_client_ = std::make_unique<MockRestClient>();
  }
  std::unique_ptr<MockRestClient> mock_rest_client_;
};

/// @test Verify that we can create credentials from a JWT string.
TEST_F(AuthorizedUserCredentialsTest, Simple) {
  std::string response_payload = R"""({
    "token_type": "Type",
    "access_token": "access-token-value",
    "expires_in": 1234
})""";

  auto client = [&]() {
    auto response = std::make_unique<MockRestResponse>();
    EXPECT_CALL(*response, StatusCode)
        .WillRepeatedly(Return(rest_internal::HttpStatusCode::kOk));
    EXPECT_CALL(std::move(*response), ExtractPayload)
        .WillOnce(Return(ByMove(MakeMockHttpPayloadSuccess(response_payload))));

    auto client = std::make_unique<MockRestClient>();
    using FormDataType = std::vector<std::pair<std::string, std::string>>;
    auto expected_request =
        Property(&RestRequest::path, GoogleOAuthRefreshEndpoint());
    auto expected_form_data = MatcherCast<FormDataType const&>(IsSupersetOf({
        Pair("grant_type", "refresh_token"),
        Pair("client_id", "a-client-id.example.com"),
        Pair("client_secret", "a-123456ABCDEF"),
        Pair("refresh_token", "1/THETOKEN"),
    }));
    EXPECT_CALL(*client, Post(_, expected_request, expected_form_data))
        .WillOnce(
            Return(ByMove(std::unique_ptr<RestResponse>(std::move(response)))));
    return client;
  }();

  std::string config = R"""({
      "client_id": "a-client-id.example.com",
      "client_secret": "a-123456ABCDEF",
      "refresh_token": "1/THETOKEN",
      "type": "magic_type"
})""";

  auto info = ParseAuthorizedUserCredentials(config, "test");
  ASSERT_STATUS_OK(info);

  MockHttpClientFactory client_factory;
  EXPECT_CALL(client_factory, Call).WillOnce(Return(ByMove(std::move(client))));
  AuthorizedUserCredentials credentials(*info, {},
                                        client_factory.AsStdFunction());
  auto token = credentials.GetToken(std::chrono::system_clock::now());
  ASSERT_STATUS_OK(token);
  EXPECT_EQ(token->token, "access-token-value");
}

/// @test Verify that parsing an authorized user account JSON string works.
TEST_F(AuthorizedUserCredentialsTest, ParseSimple) {
  std::string config = R"""({
      "client_id": "a-client-id.example.com",
      "client_secret": "a-123456ABCDEF",
      "refresh_token": "1/THETOKEN",
      "token_uri": "https://oauth2.googleapis.com/test_endpoint",
      "type": "magic_type"
})""";

  auto actual =
      ParseAuthorizedUserCredentials(config, "test-data", "unused-uri");
  ASSERT_STATUS_OK(actual);
  EXPECT_EQ("a-client-id.example.com", actual->client_id);
  EXPECT_EQ("a-123456ABCDEF", actual->client_secret);
  EXPECT_EQ("1/THETOKEN", actual->refresh_token);
  EXPECT_EQ("https://oauth2.googleapis.com/test_endpoint", actual->token_uri);
  EXPECT_EQ(actual->universe_domain, GoogleDefaultUniverseDomain());
}

/// @test Verify that parsing an authorized user account JSON string with a
/// non-empty universe_domain works.
TEST_F(AuthorizedUserCredentialsTest, ParseSimpleWithUniverseDomain) {
  std::string config = R"""({
      "client_id": "a-client-id.example.com",
      "client_secret": "a-123456ABCDEF",
      "refresh_token": "1/THETOKEN",
      "token_uri": "https://oauth2.googleapis.com/test_endpoint",
      "type": "magic_type",
      "universe_domain": "my-ud.net"
})""";

  auto actual =
      ParseAuthorizedUserCredentials(config, "test-data", "unused-uri");
  ASSERT_STATUS_OK(actual);
  EXPECT_EQ("a-client-id.example.com", actual->client_id);
  EXPECT_EQ("a-123456ABCDEF", actual->client_secret);
  EXPECT_EQ("1/THETOKEN", actual->refresh_token);
  EXPECT_EQ("https://oauth2.googleapis.com/test_endpoint", actual->token_uri);
  EXPECT_EQ(actual->universe_domain, "my-ud.net");
}

/// @test Verify that parsing an authorized user account JSON string with a
/// non-empty universe_domain works.
TEST_F(AuthorizedUserCredentialsTest, ParseSimpleWithEmptyUniverseDomain) {
  std::string config = R"""({
      "client_id": "a-client-id.example.com",
      "client_secret": "a-123456ABCDEF",
      "refresh_token": "1/THETOKEN",
      "token_uri": "https://oauth2.googleapis.com/test_endpoint",
      "type": "magic_type",
      "universe_domain": ""
})""";

  auto actual =
      ParseAuthorizedUserCredentials(config, "test-data", "unused-uri");
  EXPECT_THAT(
      actual,
      StatusIs(
          StatusCode::kInvalidArgument,
          HasSubstr(
              "universe_domain field in credentials file cannot be empty")));
}

/// @test Verify that parsing an authorized user account JSON string works.
TEST_F(AuthorizedUserCredentialsTest, ParseUsesExplicitDefaultTokenUri) {
  // No token_uri attribute here, so the default passed below should be used.
  std::string config = R"""({
      "client_id": "a-client-id.example.com",
      "client_secret": "a-123456ABCDEF",
      "refresh_token": "1/THETOKEN",
      "type": "magic_type"
})""";

  auto actual = ParseAuthorizedUserCredentials(
      config, "test-data", "https://oauth2.googleapis.com/test_endpoint");
  ASSERT_STATUS_OK(actual);
  EXPECT_EQ("a-client-id.example.com", actual->client_id);
  EXPECT_EQ("a-123456ABCDEF", actual->client_secret);
  EXPECT_EQ("1/THETOKEN", actual->refresh_token);
  EXPECT_EQ("https://oauth2.googleapis.com/test_endpoint", actual->token_uri);
}

/// @test Verify that parsing an authorized user account JSON string works.
TEST_F(AuthorizedUserCredentialsTest, ParseUsesImplicitDefaultTokenUri) {
  // No token_uri attribute here.
  std::string config = R"""({
      "client_id": "a-client-id.example.com",
      "client_secret": "a-123456ABCDEF",
      "refresh_token": "1/THETOKEN",
      "type": "magic_type"
})""";

  // No token_uri passed in here, either.
  auto actual = ParseAuthorizedUserCredentials(config, "test-data");
  ASSERT_STATUS_OK(actual);
  EXPECT_EQ("a-client-id.example.com", actual->client_id);
  EXPECT_EQ("a-123456ABCDEF", actual->client_secret);
  EXPECT_EQ("1/THETOKEN", actual->refresh_token);
  EXPECT_EQ(std::string(GoogleOAuthRefreshEndpoint()), actual->token_uri);
}

/// @test Verify that invalid contents result in a readable error.
TEST_F(AuthorizedUserCredentialsTest, ParseInvalidContentsFails) {
  std::string config = R"""( not-a-valid-json-string })""";

  auto info = ParseAuthorizedUserCredentials(config, "test-as-a-source");
  EXPECT_THAT(info,
              StatusIs(Not(StatusCode::kOk),
                       AllOf(HasSubstr("Invalid AuthorizedUserCredentials"),
                             HasSubstr("test-as-a-source"))));
}

/// @test Parsing a service account JSON string should detect empty fields.
TEST_F(AuthorizedUserCredentialsTest, ParseEmptyFieldFails) {
  std::string contents = R"""({
      "client_id": "a-client-id.example.com",
      "client_secret": "a-123456ABCDEF",
      "refresh_token": "1/THETOKEN",
      "type": "magic_type"
})""";

  for (auto const& field : {"client_id", "client_secret", "refresh_token"}) {
    auto json = nlohmann::json::parse(contents);
    json[field] = "";
    auto info = ParseAuthorizedUserCredentials(json.dump(), "test-data");
    EXPECT_THAT(info,
                StatusIs(Not(StatusCode::kOk),
                         AllOf(HasSubstr(field), HasSubstr(" field is empty"),
                               HasSubstr("test-data"))));
  }
}

/// @test Parsing a service account JSON string should detect missing fields.
TEST_F(AuthorizedUserCredentialsTest, ParseMissingFieldFails) {
  std::string contents = R"""({
      "client_id": "a-client-id.example.com",
      "client_secret": "a-123456ABCDEF",
      "refresh_token": "1/THETOKEN",
      "type": "magic_type"
})""";

  for (auto const& field : {"client_id", "client_secret", "refresh_token"}) {
    auto json = nlohmann::json::parse(contents);
    json.erase(field);
    auto info = ParseAuthorizedUserCredentials(json.dump(), "test-data");
    EXPECT_THAT(info,
                StatusIs(Not(StatusCode::kOk),
                         AllOf(HasSubstr(field), HasSubstr(" field is missing"),
                               HasSubstr("test-data"))));
  }
}

/// @test Parsing a refresh response with missing fields results in failure.
TEST_F(AuthorizedUserCredentialsTest,
       ParseAuthorizedUserRefreshResponseMissingFields) {
  std::string r1 = R"""({})""";
  // Does not have access_token.
  std::string r2 = R"""({
    "token_type": "Type",
    "expires_in": 1000
})""";

  auto mock_response1 = std::make_unique<MockRestResponse>();
  EXPECT_CALL(*mock_response1, StatusCode)
      .WillRepeatedly(Return(rest_internal::HttpStatusCode::kBadRequest));
  EXPECT_CALL(std::move(*mock_response1), ExtractPayload).WillOnce([&] {
    return MakeMockHttpPayloadSuccess(r1);
  });

  auto mock_response2 = std::make_unique<MockRestResponse>();
  //  EXPECT_CALL(*mock_response2, Headers).WillOnce(Return(headers_));
  EXPECT_CALL(*mock_response2, StatusCode)
      .WillRepeatedly(Return(rest_internal::HttpStatusCode::kBadRequest));
  EXPECT_CALL(std::move(*mock_response2), ExtractPayload).WillOnce([&] {
    return MakeMockHttpPayloadSuccess(r2);
  });

  auto status = ParseAuthorizedUserRefreshResponse(
      *mock_response1, std::chrono::system_clock::from_time_t(1000));
  EXPECT_THAT(status,
              StatusIs(StatusCode::kInvalidArgument,
                       HasSubstr("Could not find all required fields")));

  status = ParseAuthorizedUserRefreshResponse(*mock_response2,
                                              std::chrono::system_clock::now());
  EXPECT_THAT(status,
              StatusIs(StatusCode::kInvalidArgument,
                       HasSubstr("Could not find all required fields")));
}

/// @test Parsing a refresh response yields an access token.
TEST_F(AuthorizedUserCredentialsTest, ParseAuthorizedUserRefreshResponse) {
  std::string r1 = R"""({
    "token_type": "Type",
    "access_token": "access-token-r1",
    "expires_in": 1000
})""";

  auto mock_response = std::make_unique<MockRestResponse>();
  EXPECT_CALL(*mock_response, StatusCode)
      .WillRepeatedly(Return(rest_internal::HttpStatusCode::kOk));
  EXPECT_CALL(std::move(*mock_response), ExtractPayload).WillOnce([&] {
    return MakeMockHttpPayloadSuccess(r1);
  });

  auto const now = std::chrono::system_clock::now();
  auto const expires_in = std::chrono::seconds(1000);
  auto status = ParseAuthorizedUserRefreshResponse(*mock_response, now);
  EXPECT_STATUS_OK(status);
  auto token = *status;
  EXPECT_EQ(token.expiration, now + expires_in);
  EXPECT_EQ(token.token, "access-token-r1");
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace oauth2_internal
}  // namespace cloud
}  // namespace google
