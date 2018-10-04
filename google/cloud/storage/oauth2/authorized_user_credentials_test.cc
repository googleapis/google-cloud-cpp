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
#include "google/cloud/internal/setenv.h"
#include "google/cloud/storage/internal/nljson.h"
#include "google/cloud/storage/oauth2/credential_constants.h"
#include "google/cloud/storage/testing/mock_http_request.h"
#include <gmock/gmock.h>
#include <cstring>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace oauth2 {
namespace {
using ::google::cloud::storage::internal::HttpResponse;
using ::google::cloud::storage::testing::MockHttpRequest;
using ::google::cloud::storage::testing::MockHttpRequestBuilder;
using ::testing::_;
using ::testing::An;
using ::testing::HasSubstr;
using ::testing::Invoke;
using ::testing::Return;
using ::testing::StrEq;

class AuthorizedUserCredentialsTest : public ::testing::Test {
 protected:
  void SetUp() override {
    MockHttpRequestBuilder::mock =
        std::make_shared<MockHttpRequestBuilder::Impl>();
  }
  void TearDown() override { MockHttpRequestBuilder::mock.reset(); }
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
      .WillOnce(Invoke([response](std::string const& payload) {
        EXPECT_THAT(payload, HasSubstr("grant_type=refresh_token"));
        EXPECT_THAT(payload, HasSubstr("client_id=a-client-id.example.com"));
        EXPECT_THAT(payload, HasSubstr("client_secret=a-123456ABCDEF"));
        EXPECT_THAT(payload, HasSubstr("refresh_token=1/THETOKEN"));
        return HttpResponse{200, response, {}};
      }));

  auto mock_builder = MockHttpRequestBuilder::mock;
  EXPECT_CALL(*mock_builder,
              Constructor(StrEq("https://oauth2.googleapis.com/token")))
      .Times(1);
  EXPECT_CALL(*mock_builder, BuildRequest()).WillOnce(Invoke([mock_request]() {
    MockHttpRequest result;
    result.mock = mock_request;
    return result;
  }));
  EXPECT_CALL(*mock_builder, MakeEscapedString(An<std::string const&>()))
      .WillRepeatedly(Invoke([](std::string const& s) {
        auto t = std::unique_ptr<char[]>(new char[s.size() + 1]);
        std::copy(s.begin(), s.end(), t.get());
        t[s.size()] = '\0';
        return t;
      }));

  std::string config = R"""({
      "client_id": "a-client-id.example.com",
      "client_secret": "a-123456ABCDEF",
      "refresh_token": "1/THETOKEN",
      "type": "magic_type"
})""";

  AuthorizedUserCredentials<MockHttpRequestBuilder> credentials(config, "test");
  EXPECT_EQ("Authorization: Type access-token-value",
            credentials.AuthorizationHeader());
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
  auto mock_builder = MockHttpRequestBuilder::mock;
  EXPECT_CALL(*mock_builder, BuildRequest()).WillOnce(Invoke([mock_request] {
    MockHttpRequest request;
    request.mock = mock_request;
    return request;
  }));
  EXPECT_CALL(*mock_builder, Constructor(GoogleOAuthRefreshEndpoint()))
      .Times(1);
  EXPECT_CALL(*mock_builder, MakeEscapedString(An<std::string const&>()))
      .WillRepeatedly(Invoke([](std::string const& s) {
        auto t = std::unique_ptr<char[]>(new char[s.size() + 1]);
        std::copy(s.begin(), s.end(), t.get());
        t[s.size()] = '\0';
        return t;
      }));

  std::string config = R"""({
      "client_id": "a-client-id.example.com",
      "client_secret": "a-123456ABCDEF",
      "refresh_token": "1/THETOKEN",
      "type": "magic_type"
})""";
  AuthorizedUserCredentials<MockHttpRequestBuilder> credentials(config, "test");
  EXPECT_EQ("Authorization: Type access-token-r1",
            credentials.AuthorizationHeader());
  EXPECT_EQ("Authorization: Type access-token-r2",
            credentials.AuthorizationHeader());
  EXPECT_EQ("Authorization: Type access-token-r2",
            credentials.AuthorizationHeader());
}

/// @test Verify that invalid contents result in a readable error.
TEST_F(AuthorizedUserCredentialsTest, InvalidContents) {
  std::string config = R"""( not-a-valid-json-string })""";

#if GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
  auto mock_builder = MockHttpRequestBuilder::mock;
  EXPECT_CALL(*mock_builder, Constructor(GoogleOAuthRefreshEndpoint()))
      .Times(1);
  EXPECT_THROW(
      try {
        AuthorizedUserCredentials<MockHttpRequestBuilder> credentials(
            config, "test-as-a-source");
      } catch (std::invalid_argument const& ex) {
        EXPECT_THAT(ex.what(), HasSubstr("Invalid AuthorizedUserCredentials"));
        EXPECT_THAT(ex.what(), HasSubstr("test-as-a-source"));
        throw;
      },
      std::invalid_argument);
#else
  EXPECT_DEATH_IF_SUPPORTED(AuthorizedUserCredentials<MockHttpRequestBuilder>(
                                config, "test-as-a-source"),
                            "exceptions are disabled");
#endif  // GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
}

/// @test Verify that missing fields result in a readable error.
TEST_F(AuthorizedUserCredentialsTest, MissingContents) {
  std::string config = R"""({
      "client_secret": "a-123456ABCDEF",
      "refresh_token": "1/THETOKEN",
      "type": "magic_type"
})""";

#if GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
  auto mock_builder = MockHttpRequestBuilder::mock;
  EXPECT_CALL(*mock_builder, Constructor(GoogleOAuthRefreshEndpoint()))
      .Times(1);
  EXPECT_THROW(
      try {
        AuthorizedUserCredentials<MockHttpRequestBuilder> credentials(
            config, "test-as-a-source");
      } catch (std::invalid_argument const& ex) {
        EXPECT_THAT(ex.what(), HasSubstr("the client_id field is missing"));
        EXPECT_THAT(ex.what(), HasSubstr("test-as-a-source"));
        throw;
      },
      std::invalid_argument);
#else
  EXPECT_DEATH_IF_SUPPORTED(AuthorizedUserCredentials<MockHttpRequestBuilder>(
                                config, "test-as-a-source"),
                            "exceptions are disabled");
#endif  // GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
}

/// @test Verify that parsing works in the easy case.
TEST_F(AuthorizedUserCredentialsTest, ParseSimple) {
  std::string contents = R"""({
      "client_id": "a-client-id.example.com",
      "client_secret": "a-123456ABCDEF",
      "refresh_token": "1/THETOKEN",
      "type": "magic_type"
})""";

  auto actual = ParseAuthorizedUserCredentials(contents, "test-data");
  EXPECT_EQ("a-client-id.example.com", actual.client_id);
  EXPECT_EQ("a-123456ABCDEF", actual.client_secret);
  EXPECT_EQ("1/THETOKEN", actual.refresh_token);
}
/// @test Verify that invalid contents result in a readable error.
TEST_F(AuthorizedUserCredentialsTest, ParseInvalid) {
  std::string contents = R"""( not-a-valid-json-string )""";

#if GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
  EXPECT_THROW(
      try {
        ParseAuthorizedUserCredentials(contents, "test-data");
      } catch (std::invalid_argument const& ex) {
        EXPECT_THAT(ex.what(), HasSubstr("Invalid AuthorizedUserCredentials"));
        EXPECT_THAT(ex.what(), HasSubstr("test-data"));
        throw;
      },
      std::invalid_argument);
#else
  EXPECT_DEATH_IF_SUPPORTED(
      ParseAuthorizedUserCredentials(contents, "test-data"),
      "exceptions are disabled");
#endif  // GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
}

/// @test Parsing a service account JSON string should detect empty fields.
TEST_F(AuthorizedUserCredentialsTest, ParseEmptyField) {
  std::string contents = R"""({
      "client_id": "a-client-id.example.com",
      "client_secret": "a-123456ABCDEF",
      "refresh_token": "1/THETOKEN",
      "type": "magic_type"
})""";

  for (auto const& field : {"client_id", "client_secret", "refresh_token"}) {
    internal::nl::json json = internal::nl::json::parse(contents);
    json[field] = "";
#if GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
    EXPECT_THROW(
        try {
          ParseAuthorizedUserCredentials(json.dump(), "test-data");
        } catch (std::invalid_argument const& ex) {
          EXPECT_THAT(ex.what(), HasSubstr(field));
          EXPECT_THAT(ex.what(), HasSubstr(" field is empty"));
          EXPECT_THAT(ex.what(), HasSubstr("test-data"));
          throw;
        },
        std::invalid_argument)
        << "field=" << field;
#else
    EXPECT_DEATH_IF_SUPPORTED(
        ParseAuthorizedUserCredentials(json.dump(), "test-data"),
        "exceptions are disabled")
        << "field=" << field;
#endif  // GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
  }
}

/// @test Parsing a service account JSON string should detect missing fields.
TEST_F(AuthorizedUserCredentialsTest, ParseMissingField) {
  std::string contents = R"""({
      "client_id": "a-client-id.example.com",
      "client_secret": "a-123456ABCDEF",
      "refresh_token": "1/THETOKEN",
      "type": "magic_type"
})""";

  for (auto const& field : {"client_id", "client_secret", "refresh_token"}) {
    internal::nl::json json = internal::nl::json::parse(contents);
    json.erase(field);
#if GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
    EXPECT_THROW(
        try {
          ParseAuthorizedUserCredentials(json.dump(), "test-data");
        } catch (std::invalid_argument const& ex) {
          EXPECT_THAT(ex.what(), HasSubstr(field));
          EXPECT_THAT(ex.what(), HasSubstr(" field is missing"));
          EXPECT_THAT(ex.what(), HasSubstr("test-data"));
          throw;
        },
        std::invalid_argument)
        << "field=" << field;
#else
    EXPECT_DEATH_IF_SUPPORTED(
        ParseAuthorizedUserCredentials(json.dump(), "test-data"),
        "exceptions are disabled")
        << "field=" << field;
#endif  // GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
  }
}

}  // namespace
}  // namespace oauth2
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google
