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

#include "google/cloud/storage/internal/authorized_user_credentials.h"
#include "google/cloud/internal/setenv.h"
#include "google/cloud/storage/internal/credential_constants.h"
#include "google/cloud/storage/internal/nljson.h"
#include "google/cloud/storage/testing/mock_http_request.h"
#include <gmock/gmock.h>
#include <cstring>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace internal {
namespace {
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
  EXPECT_CALL(*mock_request, MakeRequest())
      .WillOnce(Return(HttpResponse{200, response, {}}));

  auto mock_builder = MockHttpRequestBuilder::mock;
  EXPECT_CALL(*mock_builder,
              Constructor(StrEq("https://accounts.google.com/o/oauth2/token")))
      .Times(1);
  EXPECT_CALL(*mock_builder, BuildRequest(_))
      .WillOnce(Invoke([mock_request](std::string payload) {
        EXPECT_THAT(payload, HasSubstr("grant_type=refresh_token"));
        EXPECT_THAT(payload, HasSubstr("client_id=a-client-id.example.com"));
        EXPECT_THAT(payload, HasSubstr("client_secret=a-123456ABCDEF"));
        EXPECT_THAT(payload, HasSubstr("refresh_token=1/THETOKEN"));
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

  AuthorizedUserCredentials<MockHttpRequestBuilder> credentials(config);
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
  EXPECT_CALL(*mock_request, MakeRequest())
      .WillOnce(Return(HttpResponse{200, r1, {}}))
      .WillOnce(Return(HttpResponse{200, r2, {}}));

  // Now setup the builder to return those responses.
  auto mock_builder = MockHttpRequestBuilder::mock;
  EXPECT_CALL(*mock_builder, BuildRequest(_))
      .WillOnce(Invoke([mock_request](std::string unused) {
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
  AuthorizedUserCredentials<MockHttpRequestBuilder> credentials(config);
  EXPECT_EQ("Authorization: Type access-token-r1",
            credentials.AuthorizationHeader());
  EXPECT_EQ("Authorization: Type access-token-r2",
            credentials.AuthorizationHeader());
  EXPECT_EQ("Authorization: Type access-token-r2",
            credentials.AuthorizationHeader());
}

}  // namespace
}  // namespace internal
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google
