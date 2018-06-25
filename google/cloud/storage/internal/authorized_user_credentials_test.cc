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
#include "google/cloud/storage/internal/nljson.h"
#include "google/cloud/storage/testing/mock_http_request.h"
#include <gmock/gmock.h>
#include <cstring>

namespace storage = google::cloud::storage;
using storage::internal::AuthorizedUserCredentials;
using storage::testing::MockHttpRequest;
using namespace ::testing;

class AuthorizedUserCredentialsTest : public ::testing::Test {
 protected:
  void SetUp() { MockHttpRequest::Clear(); }
  void TearDown() { MockHttpRequest::Clear(); }
};

/// @test Verify that we can create credentials from a JWT string.
TEST_F(AuthorizedUserCredentialsTest, Simple) {
  std::string jwt = R"""({
      "client_id": "a-client-id.example.com",
      "client_secret": "a-123456ABCDEF",
      "refresh_token": "1/THETOKEN",
      "type": "magic_type"
})""";

  auto handle =
      MockHttpRequest::Handle(storage::internal::GoogleOAuthRefreshEndpoint());
  EXPECT_CALL(*handle, PrepareRequest(An<std::string const&>()))
      .WillOnce(Invoke([](std::string const& payload) {
        auto npos = std::string::npos;
        EXPECT_NE(npos, payload.find("grant_type=refresh_token"));
        EXPECT_NE(npos, payload.find("client_id=a-client-id.example.com"));
        EXPECT_NE(npos, payload.find("client_secret=a-123456ABCDEF"));
        EXPECT_NE(npos, payload.find("refresh_token=1/THETOKEN"));
      }));
  EXPECT_CALL(*handle, MakeEscapedString(_))
      .WillRepeatedly(Invoke([](std::string const& x) {
        auto const size = x.size();
        auto copy = new char[size + 1];
        std::memcpy(copy, x.data(), x.size());
        copy[size] = '\0';
        return std::unique_ptr<char[]>(copy);
      }));

  std::string response = R"""({
    "token_type": "Type",
    "access_token": "access-token-value",
    "id_token": "id-token-value",
    "expires_in": 1234
})""";
  EXPECT_CALL(*handle, MakeRequest())
      .WillOnce(Return(storage::internal::HttpResponse{200, response, {}}));

  AuthorizedUserCredentials<MockHttpRequest> credentials(jwt);
  EXPECT_EQ("Authorization: Type access-token-value",
            credentials.AuthorizationHeader());
}

/// @test Verify that we can refresh service account credentials.
TEST_F(AuthorizedUserCredentialsTest, Refresh) {
  std::string jwt = R"""({
      "client_id": "a-client-id.example.com",
      "client_secret": "a-123456ABCDEF",
      "refresh_token": "1/THETOKEN",
      "type": "magic_type"
})""";

  auto handle =
      MockHttpRequest::Handle(storage::internal::GoogleOAuthRefreshEndpoint());
  EXPECT_CALL(*handle, PrepareRequest(An<std::string const&>())).Times(1);
  EXPECT_CALL(*handle, MakeEscapedString(_))
      .WillRepeatedly(Invoke([](std::string const& x) {
        auto const size = x.size();
        auto copy = new char[size + 1];
        std::memcpy(copy, x.data(), x.size());
        copy[size] = '\0';
        return std::unique_ptr<char[]>(copy);
      }));

  // Prepare two responses, the first one is used but becomes immediately
  // expired.
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
  EXPECT_CALL(*handle, MakeRequest())
      .WillOnce(Return(storage::internal::HttpResponse{200, r1, {}}))
      .WillOnce(Return(storage::internal::HttpResponse{200, r2, {}}));

  AuthorizedUserCredentials<MockHttpRequest> credentials(jwt);
  EXPECT_EQ("Authorization: Type access-token-r1",
            credentials.AuthorizationHeader());
  EXPECT_EQ("Authorization: Type access-token-r2",
            credentials.AuthorizationHeader());
  EXPECT_EQ("Authorization: Type access-token-r2",
            credentials.AuthorizationHeader());
}
