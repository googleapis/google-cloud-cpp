// Copyright 2018 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "google/cloud/storage/oauth2/authorized_user_credentials.h"
#include "google/cloud/storage/oauth2/credential_constants.h"
#include "google/cloud/storage/testing/mock_fake_clock.h"
#include "google/cloud/storage/testing/mock_http_request.h"
#include "google/cloud/internal/setenv.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>
#include <nlohmann/json.hpp>
#include <cstring>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace oauth2 {
namespace {

using ::google::cloud::storage::internal::HttpResponse;
using ::google::cloud::storage::testing::FakeClock;
using ::google::cloud::storage::testing::MockHttpRequest;
using ::google::cloud::storage::testing::MockHttpRequestBuilder;
using ::google::cloud::testing_util::IsOk;
using ::google::cloud::testing_util::StatusIs;
using ::testing::_;
using ::testing::AllOf;
using ::testing::An;
using ::testing::HasSubstr;
using ::testing::Not;
using ::testing::Return;
using ::testing::StrEq;

class AuthorizedUserCredentialsTest : public ::testing::Test {
 protected:
  void SetUp() override {
    MockHttpRequestBuilder::mock_ =
        std::make_shared<MockHttpRequestBuilder::Impl>();
  }
  void TearDown() override { MockHttpRequestBuilder::mock_.reset(); }
};

/// @test Verify that we can create credentials from a JWT string.
TEST_F(AuthorizedUserCredentialsTest, Simple) {
  std::string response = R"""({
    "token_type": "Type",
    "access_token": "access-token-value",
    "id_token": "id-token-value",
    "expires_in": 1234
})""";
  auto mock_request = std::make_shared<MockHttpRequest::Impl>();
  EXPECT_CALL(*mock_request, MakeRequest(_))
      .WillOnce([response](std::string const& payload) {
        EXPECT_THAT(payload, HasSubstr("grant_type=refresh_token"));
        EXPECT_THAT(payload, HasSubstr("client_id=a-client-id.example.com"));
        EXPECT_THAT(payload, HasSubstr("client_secret=a-123456ABCDEF"));
        EXPECT_THAT(payload, HasSubstr("refresh_token=1/THETOKEN"));
        return HttpResponse{200, response, {}};
      });

  auto mock_builder = MockHttpRequestBuilder::mock_;
  EXPECT_CALL(*mock_builder,
              Constructor(StrEq("https://oauth2.googleapis.com/token")))
      .Times(1);
  EXPECT_CALL(*mock_builder, BuildRequest()).WillOnce([mock_request]() {
    MockHttpRequest result;
    result.mock = mock_request;
    return result;
  });
  EXPECT_CALL(*mock_builder, MakeEscapedString(An<std::string const&>()))
      .WillRepeatedly([](std::string const& s) {
        auto t = std::unique_ptr<char[]>(new char[s.size() + 1]);
        std::copy(s.begin(), s.end(), t.get());
        t[s.size()] = '\0';
        return t;
      });

  std::string config = R"""({
      "client_id": "a-client-id.example.com",
      "client_secret": "a-123456ABCDEF",
      "refresh_token": "1/THETOKEN",
      "type": "magic_type"
})""";

  auto info = ParseAuthorizedUserCredentials(config, "test");
  ASSERT_STATUS_OK(info);
  AuthorizedUserCredentials<MockHttpRequestBuilder> credentials(*info);
  EXPECT_EQ("Authorization: Type access-token-value",
            credentials.AuthorizationHeader().value());
}

/// @test Verify that we can refresh service account credentials.
TEST_F(AuthorizedUserCredentialsTest, Refresh) {
  // Prepare two responses, the first one is used but becomes immediately
  // expired, resulting in another refresh next time the caller tries to get
  // an authorization header.
  std::string r1 = R"""({
    "token_type": "Type",
    "access_token": "access-token-r1",
    "id_token": "id-token-value",
    "expires_in": 0
})""";
  std::string r2 = R"""({
    "token_type": "Type",
    "access_token": "access-token-r2",
    "id_token": "id-token-value",
    "expires_in": 1000
})""";
  auto mock_request = std::make_shared<MockHttpRequest::Impl>();
  EXPECT_CALL(*mock_request, MakeRequest(_))
      .WillOnce(Return(HttpResponse{200, r1, {}}))
      .WillOnce(Return(HttpResponse{200, r2, {}}));

  // Now setup the builder to return those responses.
  auto mock_builder = MockHttpRequestBuilder::mock_;
  EXPECT_CALL(*mock_builder, BuildRequest()).WillOnce([mock_request] {
    MockHttpRequest request;
    request.mock = mock_request;
    return request;
  });
  EXPECT_CALL(*mock_builder, Constructor(GoogleOAuthRefreshEndpoint()))
      .Times(1);
  EXPECT_CALL(*mock_builder, MakeEscapedString(An<std::string const&>()))
      .WillRepeatedly([](std::string const& s) {
        auto t = std::unique_ptr<char[]>(new char[s.size() + 1]);
        std::copy(s.begin(), s.end(), t.get());
        t[s.size()] = '\0';
        return t;
      });

  std::string config = R"""({
      "client_id": "a-client-id.example.com",
      "client_secret": "a-123456ABCDEF",
      "refresh_token": "1/THETOKEN",
      "type": "magic_type"
})""";
  auto info = ParseAuthorizedUserCredentials(config, "test");
  ASSERT_STATUS_OK(info);
  AuthorizedUserCredentials<MockHttpRequestBuilder> credentials(*info);
  EXPECT_EQ("Authorization: Type access-token-r1",
            credentials.AuthorizationHeader().value());
  EXPECT_EQ("Authorization: Type access-token-r2",
            credentials.AuthorizationHeader().value());
  EXPECT_EQ("Authorization: Type access-token-r2",
            credentials.AuthorizationHeader().value());
}

/// @test Mock a failed refresh response.
TEST_F(AuthorizedUserCredentialsTest, FailedRefresh) {
  auto mock_request = std::make_shared<MockHttpRequest::Impl>();
  EXPECT_CALL(*mock_request, MakeRequest(_))
      .WillOnce(Return(Status(StatusCode::kAborted, "Fake Curl error")))
      .WillOnce(Return(HttpResponse{400, "", {}}));

  // Now setup the builder to return those responses.
  auto mock_builder = MockHttpRequestBuilder::mock_;
  EXPECT_CALL(*mock_builder, BuildRequest()).WillOnce([mock_request] {
    MockHttpRequest request;
    request.mock = mock_request;
    return request;
  });
  EXPECT_CALL(*mock_builder, Constructor(GoogleOAuthRefreshEndpoint()))
      .Times(1);
  EXPECT_CALL(*mock_builder, MakeEscapedString(An<std::string const&>()))
      .WillRepeatedly([](std::string const& s) {
        auto t = std::unique_ptr<char[]>(new char[s.size() + 1]);
        std::copy(s.begin(), s.end(), t.get());
        t[s.size()] = '\0';
        return t;
      });

  std::string config = R"""({
      "client_id": "a-client-id.example.com",
      "client_secret": "a-123456ABCDEF",
      "refresh_token": "1/THETOKEN",
      "type": "magic_type"
})""";
  auto info = ParseAuthorizedUserCredentials(config, "test");
  ASSERT_STATUS_OK(info);
  AuthorizedUserCredentials<MockHttpRequestBuilder> credentials(*info);
  // Response 1
  auto status = credentials.AuthorizationHeader();
  EXPECT_THAT(status, StatusIs(StatusCode::kAborted));
  // Response 2
  status = credentials.AuthorizationHeader();
  EXPECT_THAT(status, Not(IsOk()));
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
    "id_token": "id-token-value",
    "expires_in": 1000
})""";

  FakeClock::reset_clock(1000);
  auto status = ParseAuthorizedUserRefreshResponse(HttpResponse{400, r1, {}},
                                                   FakeClock::now());
  EXPECT_THAT(status,
              StatusIs(StatusCode::kInvalidArgument,
                       HasSubstr("Could not find all required fields")));

  status = ParseAuthorizedUserRefreshResponse(HttpResponse{400, r2, {}},
                                              FakeClock::now());
  EXPECT_THAT(status,
              StatusIs(StatusCode::kInvalidArgument,
                       HasSubstr("Could not find all required fields")));
}

/// @test Parsing a refresh response yields a TemporaryToken.
TEST_F(AuthorizedUserCredentialsTest, ParseAuthorizedUserRefreshResponse) {
  std::string r1 = R"""({
    "token_type": "Type",
    "access_token": "access-token-r1",
    "id_token": "id-token-value",
    "expires_in": 1000
})""";

  auto expires_in = 1000;
  FakeClock::reset_clock(2000);
  auto status = ParseAuthorizedUserRefreshResponse(HttpResponse{200, r1, {}},
                                                   FakeClock::now());
  EXPECT_STATUS_OK(status);
  auto token = *status;
  EXPECT_EQ(
      std::chrono::time_point_cast<std::chrono::seconds>(token.expiration_time)
          .time_since_epoch()
          .count(),
      FakeClock::now_value_ + expires_in);
  EXPECT_EQ(token.token, "Authorization: Type access-token-r1");
}

}  // namespace
}  // namespace oauth2
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google
