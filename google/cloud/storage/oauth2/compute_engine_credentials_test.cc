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

#include "google/cloud/storage/oauth2/compute_engine_credentials.h"
#include "google/cloud/storage/internal/compute_engine_util.h"
#include "google/cloud/storage/internal/nljson.h"
#include "google/cloud/storage/oauth2/credential_constants.h"
#include "google/cloud/storage/testing/mock_fake_clock.h"
#include "google/cloud/storage/testing/mock_http_request.h"
#include "google/cloud/internal/setenv.h"
#include "google/cloud/testing_util/assert_ok.h"
#include <gmock/gmock.h>
#include <chrono>
#include <cstring>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace oauth2 {
namespace {

using ::google::cloud::storage::internal::GceMetadataHostname;
using ::google::cloud::storage::internal::HttpResponse;
using ::google::cloud::storage::testing::FakeClock;
using ::google::cloud::storage::testing::MockHttpRequest;
using ::google::cloud::storage::testing::MockHttpRequestBuilder;
using ::testing::_;
using ::testing::HasSubstr;
using ::testing::Invoke;
using ::testing::Return;
using ::testing::StrEq;
using ::testing::UnorderedElementsAre;

class ComputeEngineCredentialsTest : public ::testing::Test {
 protected:
  void SetUp() override {
    MockHttpRequestBuilder::mock_ =
        std::make_shared<MockHttpRequestBuilder::Impl>();
  }
  void TearDown() override { MockHttpRequestBuilder::mock_.reset(); }
};

/// @test Verify that we can create and refresh ComputeEngineCredentials.
TEST_F(ComputeEngineCredentialsTest,
       RefreshingSendsCorrectRequestBodyAndParsesResponse) {
  std::string alias = "default";
  std::string email = "foo@bar.baz";
  std::string hostname = GceMetadataHostname();
  std::string svc_acct_info_resp = R"""({
      "email": ")""" + email + R"""(",
      "scopes": ["scope1","scope2"]
  })""";
  std::string token_info_resp = R"""({
      "access_token": "mysupersecrettoken",
      "expires_in": 3600,
      "token_type": "tokentype"
  })""";

  auto first_mock_req_impl = std::make_shared<MockHttpRequest::Impl>();
  EXPECT_CALL(*first_mock_req_impl, MakeRequest(_))
      .WillOnce(Return(HttpResponse{200, svc_acct_info_resp, {}}));
  auto second_mock_req_impl = std::make_shared<MockHttpRequest::Impl>();
  EXPECT_CALL(*second_mock_req_impl, MakeRequest(_))
      .WillOnce(Return(HttpResponse{200, token_info_resp, {}}));

  auto mock_req_builder = MockHttpRequestBuilder::mock_;
  EXPECT_CALL(*mock_req_builder, BuildRequest())
      .WillOnce(Invoke([first_mock_req_impl] {
        MockHttpRequest mock_request;
        mock_request.mock = first_mock_req_impl;
        return mock_request;
      }))
      .WillOnce(Invoke([second_mock_req_impl] {
        MockHttpRequest mock_request;
        mock_request.mock = second_mock_req_impl;
        return mock_request;
      }));

  // Both requests add this header.
  EXPECT_CALL(*mock_req_builder, AddHeader(StrEq("metadata-flavor: Google")))
      .Times(2);
  EXPECT_CALL(
      *mock_req_builder,
      Constructor(StrEq(std::string("http://") + hostname +
                        "/computeMetadata/v1/instance/service-accounts/" +
                        email + "/token")))
      .Times(1);
  // Only the call to retrieve service account info sends this query param.
  EXPECT_CALL(*mock_req_builder,
              AddQueryParameter(StrEq("recursive"), StrEq("true")))
      .Times(1);
  EXPECT_CALL(
      *mock_req_builder,
      Constructor(StrEq(std::string("http://") + hostname +
                        "/computeMetadata/v1/instance/service-accounts/" +
                        alias + "/")))
      .Times(1);

  ComputeEngineCredentials<MockHttpRequestBuilder> credentials(alias);
  // Calls Refresh to obtain the access token for our authorization header.
  EXPECT_EQ("Authorization: tokentype mysupersecrettoken",
            credentials.AuthorizationHeader().value());
  // Make sure we obtain the scopes and email from the metadata server.
  EXPECT_EQ(email, credentials.service_account_email());
  EXPECT_THAT(credentials.scopes(), UnorderedElementsAre("scope1", "scope2"));
}

/// @test Parsing a refresh response with missing fields results in failure.
TEST_F(ComputeEngineCredentialsTest,
       ParseComputeEngineRefreshResponseMissingFields) {
  std::string token_info_resp = R"""({})""";
  // Does not have access_token.
  std::string token_info_resp2 = R"""({
      "expires_in": 3600,
      "token_type": "tokentype"
)""";

  FakeClock::reset_clock(1000);
  auto status = ParseComputeEngineRefreshResponse(
      HttpResponse{400, token_info_resp, {}}, FakeClock::now());
  EXPECT_FALSE(status);
  EXPECT_EQ(status.status().code(), StatusCode::kInvalidArgument);
  EXPECT_THAT(status.status().message(),
              ::testing::HasSubstr("Could not find all required fields"));

  status = ParseComputeEngineRefreshResponse(
      HttpResponse{400, token_info_resp2, {}}, FakeClock::now());
  EXPECT_FALSE(status);
  EXPECT_EQ(status.status().code(), StatusCode::kInvalidArgument);
  EXPECT_THAT(status.status().message(),
              ::testing::HasSubstr("Could not find all required fields"));
}

/// @test Parsing a refresh response yields a TemporaryToken.
TEST_F(ComputeEngineCredentialsTest, ParseComputeEngineRefreshResponse) {
  std::string token_info_resp = R"""({
      "access_token": "mysupersecrettoken",
      "expires_in": 3600,
      "token_type": "tokentype"
})""";

  auto expires_in = 3600;
  auto clock_value = 2000;
  FakeClock::reset_clock(clock_value);

  auto status = ParseComputeEngineRefreshResponse(
      HttpResponse{200, token_info_resp, {}}, FakeClock::now());
  EXPECT_STATUS_OK(status);
  EXPECT_EQ(status.status().code(), StatusCode::kOk);
  auto token = *status;
  EXPECT_EQ(
      std::chrono::time_point_cast<std::chrono::seconds>(token.expiration_time)
          .time_since_epoch()
          .count(),
      clock_value + expires_in);
  EXPECT_EQ(token.token, "Authorization: tokentype mysupersecrettoken");
}

/// @test Parsing a metadata server response with missing fields results in
/// failure.
TEST_F(ComputeEngineCredentialsTest, ParseMetadataServerResponseMissingFields) {
  std::string email = "foo@bar.baz";
  std::string svc_acct_info_resp = R"""({})""";
  std::string svc_acct_info_resp2 = R"""({
      "scopes": ["scope1","scope2"]
  })""";

  auto status =
      ParseMetadataServerResponse(HttpResponse{400, svc_acct_info_resp, {}});
  EXPECT_FALSE(status);
  EXPECT_EQ(status.status().code(), StatusCode::kInvalidArgument);
  EXPECT_THAT(status.status().message(),
              ::testing::HasSubstr("Could not find all required fields"));

  status =
      ParseMetadataServerResponse(HttpResponse{400, svc_acct_info_resp2, {}});
  EXPECT_FALSE(status);
  EXPECT_EQ(status.status().code(), StatusCode::kInvalidArgument);
  EXPECT_THAT(status.status().message(),
              ::testing::HasSubstr("Could not find all required fields"));
}

/// @test Parsing a metadata server response yields a ServiceAccountMetadata.
TEST_F(ComputeEngineCredentialsTest, ParseMetadataServerResponse) {
  std::string email = "foo@bar.baz";
  std::string svc_acct_info_resp = R"""({
      "email": ")""" + email + R"""(",
      "scopes": ["scope1","scope2"]
  })""";

  auto status =
      ParseMetadataServerResponse(HttpResponse{200, svc_acct_info_resp, {}});
  EXPECT_STATUS_OK(status);
  EXPECT_EQ(status.status().code(), StatusCode::kOk);
  auto metadata = *status;
  EXPECT_EQ(metadata.email, email);
  EXPECT_TRUE(metadata.scopes.count("scope1"));
  EXPECT_TRUE(metadata.scopes.count("scope2"));
}

/// @test Mock a failed refresh response during RetrieveServiceAccountInfo.
TEST_F(ComputeEngineCredentialsTest, FailedRetrieveServiceAccountInfo) {
  std::string alias = "default";
  std::string email = "foo@bar.baz";
  std::string hostname = GceMetadataHostname();

  // Missing fields.
  std::string svc_acct_info_resp = R"""({
      "scopes": ["scope1","scope2"]
  })""";

  auto first_mock_req_impl = std::make_shared<MockHttpRequest::Impl>();
  EXPECT_CALL(*first_mock_req_impl, MakeRequest(_))
      // Fail the first call to DoMetadataServerGetRequest immediately.
      .WillOnce(Return(Status(StatusCode::kAborted, "Fake Curl error")))
      // Likewise, except with a >= 300 status code.
      .WillOnce(Return(HttpResponse{400, "", {}}))
      // Parse with an invalid metadata response.
      .WillOnce(Return(HttpResponse{1, svc_acct_info_resp, {}}));

  auto mock_req_builder = MockHttpRequestBuilder::mock_;
  EXPECT_CALL(*mock_req_builder, BuildRequest())
      .Times(3)
      .WillRepeatedly(Invoke([first_mock_req_impl] {
        MockHttpRequest mock_request;
        mock_request.mock = first_mock_req_impl;
        return mock_request;
      }));

  EXPECT_CALL(*mock_req_builder, AddHeader(StrEq("metadata-flavor: Google")))
      .Times(3);
  EXPECT_CALL(*mock_req_builder,
              AddQueryParameter(StrEq("recursive"), StrEq("true")))
      .Times(3);
  EXPECT_CALL(
      *mock_req_builder,
      Constructor(StrEq(std::string("http://") + hostname +
                        "/computeMetadata/v1/instance/service-accounts/" +
                        alias + "/")))
      .Times(3);

  ComputeEngineCredentials<MockHttpRequestBuilder> credentials(alias);
  // Response 1
  auto status = credentials.AuthorizationHeader();
  EXPECT_FALSE(status) << "status=" << status.status();
  EXPECT_EQ(status.status().code(), StatusCode::kAborted);
  // Response 2
  status = credentials.AuthorizationHeader();
  EXPECT_FALSE(status) << "status=" << status.status();
  // Response 3
  status = credentials.AuthorizationHeader();
  EXPECT_FALSE(status) << "status=" << status.status();
  EXPECT_THAT(status.status().message(),
              ::testing::HasSubstr("Could not find all required fields"));
}

/// @test Mock a failed refresh response.
TEST_F(ComputeEngineCredentialsTest, FailedRefresh) {
  std::string alias = "default";
  std::string email = "foo@bar.baz";
  std::string hostname = GceMetadataHostname();
  std::string svc_acct_info_resp = R"""({
      "email": ")""" + email + R"""(",
      "scopes": ["scope1","scope2"]
  })""";
  // Missing fields.
  std::string token_info_resp = R"""({
      "expires_in": 3600,
      "token_type": "tokentype"
  })""";

  auto mock_req = std::make_shared<MockHttpRequest::Impl>();
  EXPECT_CALL(*mock_req, MakeRequest(_))
      // Fail the first call to RetrieveServiceAccountInfo immediately.
      .WillOnce(Return(Status(StatusCode::kAborted, "Fake Curl error")))
      // Make the call to RetrieveServiceAccountInfo return a good response,
      // but fail the token request immediately.
      .WillOnce(Return(HttpResponse{200, svc_acct_info_resp, {}}))
      .WillOnce(Return(Status(StatusCode::kAborted, "Fake Curl error")))
      // Likewise, except test with a >= 300 status code.
      .WillOnce(Return(HttpResponse{200, svc_acct_info_resp, {}}))
      .WillOnce(Return(HttpResponse{400, "", {}}))
      // Parse with an invalid token response.
      .WillOnce(Return(HttpResponse{200, svc_acct_info_resp, {}}))
      .WillOnce(Return(HttpResponse{1, token_info_resp, {}}));

  auto mock_req_builder = MockHttpRequestBuilder::mock_;
  auto mock_matcher = Invoke([mock_req] {
    MockHttpRequest mock_request;
    mock_request.mock = mock_req;
    return mock_request;
  });
  EXPECT_CALL(*mock_req_builder, BuildRequest())
      .Times(7)
      .WillRepeatedly(mock_matcher);

  // This is added for all 7 requests.
  EXPECT_CALL(*mock_req_builder, AddHeader(StrEq("metadata-flavor: Google")))
      .Times(7);
  // These requests happen after RetrieveServiceAccountInfo, so they only
  // occur 3 times.
  EXPECT_CALL(
      *mock_req_builder,
      Constructor(StrEq(std::string("http://") + hostname +
                        "/computeMetadata/v1/instance/service-accounts/" +
                        email + "/token")))
      .Times(3);
  // This is only set when not retrieving the token.
  EXPECT_CALL(*mock_req_builder,
              AddQueryParameter(StrEq("recursive"), StrEq("true")))
      .Times(4);
  // For the first expected failures, the alias is used until the metadata
  // request succeeds. Then the email is refreshed.
  EXPECT_CALL(
      *mock_req_builder,
      Constructor(StrEq(std::string("http://") + hostname +
                        "/computeMetadata/v1/instance/service-accounts/" +
                        alias + "/")))
      .Times(2);
  EXPECT_CALL(
      *mock_req_builder,
      Constructor(StrEq(std::string("http://") + hostname +
                        "/computeMetadata/v1/instance/service-accounts/" +
                        email + "/")))
      .Times(2);

  ComputeEngineCredentials<MockHttpRequestBuilder> credentials(alias);
  // Response 1
  auto status = credentials.AuthorizationHeader();
  EXPECT_FALSE(status) << "status=" << status.status();
  EXPECT_EQ(status.status().code(), StatusCode::kAborted);
  // Response 2
  status = credentials.AuthorizationHeader();
  EXPECT_FALSE(status) << "status=" << status.status();
  // Response 3
  status = credentials.AuthorizationHeader();
  EXPECT_FALSE(status) << "status=" << status.status();
  // Response 4
  status = credentials.AuthorizationHeader();
  EXPECT_FALSE(status) << "status=" << status.status();
  EXPECT_THAT(status.status().message(),
              ::testing::HasSubstr("Could not find all required fields"));
}

/// @test Verify that we can force a refresh of the service account email.
TEST_F(ComputeEngineCredentialsTest, AccountEmail) {
  std::string alias = "default";
  std::string email = "foo@bar.baz";
  std::string hostname = GceMetadataHostname();
  std::string svc_acct_info_resp = R"""({
      "email": ")""" + email + R"""(",
      "scopes": ["scope1","scope2"]
  })""";

  auto first_mock_req_impl = std::make_shared<MockHttpRequest::Impl>();
  EXPECT_CALL(*first_mock_req_impl, MakeRequest(_))
      .WillOnce(Return(HttpResponse{200, svc_acct_info_resp, {}}));

  auto mock_req_builder = MockHttpRequestBuilder::mock_;
  EXPECT_CALL(*mock_req_builder, BuildRequest())
      .WillOnce(Invoke([first_mock_req_impl] {
        MockHttpRequest mock_request;
        mock_request.mock = first_mock_req_impl;
        return mock_request;
      }));

  // Both requests add this header.
  EXPECT_CALL(*mock_req_builder, AddHeader(StrEq("metadata-flavor: Google")))
      .Times(1);
  // Only the call to retrieve service account info sends this query param.
  EXPECT_CALL(*mock_req_builder,
              AddQueryParameter(StrEq("recursive"), StrEq("true")))
      .Times(1);
  EXPECT_CALL(
      *mock_req_builder,
      Constructor(StrEq(std::string("http://") + hostname +
                        "/computeMetadata/v1/instance/service-accounts/" +
                        alias + "/")))
      .Times(1);

  ComputeEngineCredentials<MockHttpRequestBuilder> credentials(alias);
  EXPECT_EQ(credentials.service_account_email(), alias);
  auto refreshed_email = credentials.AccountEmail();
  EXPECT_EQ(email, refreshed_email);
  EXPECT_EQ(credentials.service_account_email(), refreshed_email);
}

}  // namespace
}  // namespace oauth2
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google
