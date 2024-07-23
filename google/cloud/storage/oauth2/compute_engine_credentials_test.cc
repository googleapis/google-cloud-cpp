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

#include "google/cloud/storage/oauth2/compute_engine_credentials.h"
#include "google/cloud/storage/internal/compute_engine_util.h"
#include "google/cloud/storage/oauth2/credential_constants.h"
#include "google/cloud/storage/testing/mock_http_request.h"
#include "google/cloud/testing_util/mock_fake_clock.h"
#include "google/cloud/testing_util/mock_http_payload.h"
#include "google/cloud/testing_util/mock_rest_client.h"
#include "google/cloud/testing_util/mock_rest_response.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>
#include <nlohmann/json.hpp>
#include <chrono>
#include <cstring>
#include <memory>
#include <string>
#include <utility>

namespace google {
namespace cloud {
namespace storage {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace oauth2 {

// Define a helper to test the specialization.
struct ComputeEngineCredentialsTester {
  static StatusOr<std::string> Header(
      ComputeEngineCredentials<>& tested,
      std::chrono::system_clock::time_point tp) {
    return tested.AuthorizationHeaderForTesting(tp);
  }

  static ComputeEngineCredentials<> MakeComputeEngineCredentials(
      std::string service_account_email,
      oauth2_internal::HttpClientFactory factory) {
    return ComputeEngineCredentials<>(std::move(service_account_email),
                                      std::move(factory));
  }
};

namespace {

using ::google::cloud::storage::internal::GceMetadataHostname;
using ::google::cloud::storage::internal::HttpResponse;
using ::google::cloud::storage::testing::MockHttpRequest;
using ::google::cloud::storage::testing::MockHttpRequestBuilder;
using ::google::cloud::testing_util::FakeClock;
using ::google::cloud::testing_util::IsOk;
using ::google::cloud::testing_util::IsOkAndHolds;
using ::google::cloud::testing_util::MakeMockHttpPayloadSuccess;
using ::google::cloud::testing_util::MockRestClient;
using ::google::cloud::testing_util::MockRestResponse;
using ::google::cloud::testing_util::StatusIs;
using ::testing::_;
using ::testing::ByMove;
using ::testing::HasSubstr;
using ::testing::IsEmpty;
using ::testing::Not;
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
  EXPECT_CALL(*first_mock_req_impl, MakeRequest)
      .WillOnce(Return(HttpResponse{200, svc_acct_info_resp, {}}));
  auto second_mock_req_impl = std::make_shared<MockHttpRequest::Impl>();
  EXPECT_CALL(*second_mock_req_impl, MakeRequest)
      .WillOnce(Return(HttpResponse{200, token_info_resp, {}}));

  auto mock_req_builder = MockHttpRequestBuilder::mock_;
  EXPECT_CALL(*mock_req_builder, BuildRequest())
      .WillOnce([first_mock_req_impl] {
        MockHttpRequest mock_request;
        mock_request.mock = first_mock_req_impl;
        return mock_request;
      })
      .WillOnce([second_mock_req_impl] {
        MockHttpRequest mock_request;
        mock_request.mock = second_mock_req_impl;
        return mock_request;
      });

  // Both requests add this header.
  EXPECT_CALL(*mock_req_builder, AddHeader(StrEq("metadata-flavor: Google")))
      .Times(2);
  EXPECT_CALL(
      *mock_req_builder,
      Constructor(StrEq(std::string("http://") + hostname +
                        "/computeMetadata/v1/instance/service-accounts/" +
                        email + "/token"),
                  _, _))
      .Times(1);
  // Only the call to retrieve service account info sends this query param.
  EXPECT_CALL(*mock_req_builder,
              AddQueryParameter(StrEq("recursive"), StrEq("true")))
      .Times(1);
  EXPECT_CALL(
      *mock_req_builder,
      Constructor(
          StrEq(std::string("http://") + hostname +
                "/computeMetadata/v1/instance/service-accounts/" + alias + "/"),
          _, _))
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
TEST_F(ComputeEngineCredentialsTest, ParseComputeEngineRefreshResponseInvalid) {
  std::string token_info_resp = R"""({})""";
  // Does not have access_token.
  std::string token_info_resp2 = R"""({
      "expires_in": 3600,
      "token_type": "tokentype"
)""";

  FakeClock::reset_clock(1000);
  auto status = ParseComputeEngineRefreshResponse(
      HttpResponse{400, token_info_resp, {}}, FakeClock::now());
  EXPECT_THAT(status,
              StatusIs(StatusCode::kInvalidArgument,
                       HasSubstr("Could not find all required fields")));

  status = ParseComputeEngineRefreshResponse(
      HttpResponse{400, token_info_resp2, {}}, FakeClock::now());
  EXPECT_THAT(status,
              StatusIs(StatusCode::kInvalidArgument,
                       HasSubstr("Could not find all required fields")));

  EXPECT_THAT(ParseComputeEngineRefreshResponse(
                  HttpResponse{400, R"js("valid-json-but-not-object")js", {}},
                  FakeClock::now()),
              StatusIs(StatusCode::kInvalidArgument,
                       HasSubstr("Could not find all required fields")));
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
TEST_F(ComputeEngineCredentialsTest, ParseMetadataServerResponseInvalid) {
  std::string svc_acct_info_resp1 = R"""({})""";
  std::string svc_acct_info_resp2 = R"""({"scopes": ["scope1","scope2"]})""";
  std::string svc_acct_info_resp3 = R"""({"email": "foo@bar"})""";
  std::string svc_acct_info_resp4 =
      R"""({"email": "foo@bar", "scopes": "scope1\nscope2\n"})""";

  auto status =
      ParseMetadataServerResponse(HttpResponse{200, svc_acct_info_resp1, {}});
  EXPECT_THAT(status, IsOk());
  EXPECT_THAT(status->email, IsEmpty());
  EXPECT_THAT(status->scopes, IsEmpty());

  status =
      ParseMetadataServerResponse(HttpResponse{400, svc_acct_info_resp2, {}});
  EXPECT_THAT(status, IsOk());
  EXPECT_THAT(status->email, IsEmpty());
  EXPECT_THAT(status->scopes, UnorderedElementsAre("scope1", "scope2"));

  status =
      ParseMetadataServerResponse(HttpResponse{400, svc_acct_info_resp3, {}});
  EXPECT_THAT(status, IsOk());
  EXPECT_THAT(status->email, "foo@bar");
  EXPECT_THAT(status->scopes, IsEmpty());

  status =
      ParseMetadataServerResponse(HttpResponse{400, svc_acct_info_resp4, {}});
  EXPECT_THAT(status, IsOk());
  EXPECT_THAT(status->email, "foo@bar");
  EXPECT_THAT(status->scopes, UnorderedElementsAre("scope1", "scope2"));

  status = ParseMetadataServerResponse(
      HttpResponse{400, R"js("valid-json-but-not-an-object")js", {}});
  EXPECT_THAT(status, IsOk());
  EXPECT_THAT(status->email, IsEmpty());
  EXPECT_THAT(status->scopes, IsEmpty());
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
  auto metadata = *status;
  EXPECT_EQ(metadata.email, email);
  EXPECT_TRUE(metadata.scopes.count("scope1"));
  EXPECT_TRUE(metadata.scopes.count("scope2"));
}

/// @test Mock a failed refresh response during RetrieveServiceAccountInfo.
TEST_F(ComputeEngineCredentialsTest, FailedRetrieveServiceAccountInfo) {
  std::string alias = "default";
  std::string hostname = GceMetadataHostname();

  auto first_mock_req_impl = std::make_shared<MockHttpRequest::Impl>();
  EXPECT_CALL(*first_mock_req_impl, MakeRequest)
      // Fail the first call to DoMetadataServerGetRequest immediately.
      .WillOnce(Return(Status(StatusCode::kAborted, "Fake Curl error")))
      // Likewise, except with a >= 300 status code.
      .WillOnce(Return(HttpResponse{400, "", {}}));

  auto mock_req_builder = MockHttpRequestBuilder::mock_;
  EXPECT_CALL(*mock_req_builder, BuildRequest())
      .Times(2)
      .WillRepeatedly([first_mock_req_impl] {
        MockHttpRequest mock_request;
        mock_request.mock = first_mock_req_impl;
        return mock_request;
      });

  EXPECT_CALL(*mock_req_builder, AddHeader(StrEq("metadata-flavor: Google")))
      .Times(2);
  EXPECT_CALL(*mock_req_builder,
              AddQueryParameter(StrEq("recursive"), StrEq("true")))
      .Times(2);
  EXPECT_CALL(
      *mock_req_builder,
      Constructor(
          StrEq(std::string("http://") + hostname +
                "/computeMetadata/v1/instance/service-accounts/" + alias + "/"),
          _, _))
      .Times(2);

  ComputeEngineCredentials<MockHttpRequestBuilder> credentials(alias);
  // Response 1
  auto status = credentials.AuthorizationHeader();
  EXPECT_THAT(status, Not(IsOk()));
  // Response 2
  status = credentials.AuthorizationHeader();
  EXPECT_THAT(status, Not(IsOk()));
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
  EXPECT_CALL(*mock_req, MakeRequest)
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
  auto mock_matcher = [mock_req] {
    MockHttpRequest mock_request;
    mock_request.mock = mock_req;
    return mock_request;
  };
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
                        email + "/token"),
                  _, _))
      .Times(3);
  // This is only set when not retrieving the token.
  EXPECT_CALL(*mock_req_builder,
              AddQueryParameter(StrEq("recursive"), StrEq("true")))
      .Times(4);
  // For the first expected failures, the alias is used until the metadata
  // request succeeds. Then the email is refreshed.
  EXPECT_CALL(
      *mock_req_builder,
      Constructor(
          StrEq(std::string("http://") + hostname +
                "/computeMetadata/v1/instance/service-accounts/" + alias + "/"),
          _, _))
      .Times(2);
  EXPECT_CALL(
      *mock_req_builder,
      Constructor(
          StrEq(std::string("http://") + hostname +
                "/computeMetadata/v1/instance/service-accounts/" + email + "/"),
          _, _))
      .Times(2);

  ComputeEngineCredentials<MockHttpRequestBuilder> credentials(alias);
  // Response 1
  auto status = credentials.AuthorizationHeader();
  EXPECT_THAT(status, StatusIs(StatusCode::kAborted));
  // Response 2
  status = credentials.AuthorizationHeader();
  EXPECT_THAT(status, Not(IsOk()));
  // Response 3
  status = credentials.AuthorizationHeader();
  EXPECT_THAT(status, Not(IsOk()));
  // Response 4
  status = credentials.AuthorizationHeader();
  EXPECT_THAT(status,
              StatusIs(Not(StatusCode::kOk),
                       HasSubstr("Could not find all required fields")));
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
  EXPECT_CALL(*first_mock_req_impl, MakeRequest)
      .WillOnce(Return(HttpResponse{200, svc_acct_info_resp, {}}));

  auto mock_req_builder = MockHttpRequestBuilder::mock_;
  EXPECT_CALL(*mock_req_builder, BuildRequest())
      .WillOnce([first_mock_req_impl] {
        MockHttpRequest mock_request;
        mock_request.mock = first_mock_req_impl;
        return mock_request;
      });

  // Both requests add this header.
  EXPECT_CALL(*mock_req_builder, AddHeader(StrEq("metadata-flavor: Google")))
      .Times(1);
  // Only the call to retrieve service account info sends this query param.
  EXPECT_CALL(*mock_req_builder,
              AddQueryParameter(StrEq("recursive"), StrEq("true")))
      .Times(1);
  EXPECT_CALL(
      *mock_req_builder,
      Constructor(
          StrEq(std::string("http://") + hostname +
                "/computeMetadata/v1/instance/service-accounts/" + alias + "/"),
          _, _))
      .Times(1);

  ComputeEngineCredentials<MockHttpRequestBuilder> credentials(alias);
  EXPECT_EQ(credentials.service_account_email(), alias);
  auto refreshed_email = credentials.AccountEmail();
  EXPECT_EQ(email, refreshed_email);
  EXPECT_EQ(credentials.service_account_email(), refreshed_email);
}

TEST_F(ComputeEngineCredentialsTest, Caching) {
  // We need to mock the Compute Engine Metadata Service or this would be an
  // integration test that only runs on GCE.
  auto make_mock_client = [](std::string const& payload) {
    auto response = std::make_unique<MockRestResponse>();
    EXPECT_CALL(*response, StatusCode)
        .WillRepeatedly(
            Return(google::cloud::rest_internal::HttpStatusCode::kOk));
    EXPECT_CALL(std::move(*response), ExtractPayload)
        .WillOnce(Return(ByMove(MakeMockHttpPayloadSuccess(payload))));
    auto mock = std::make_unique<MockRestClient>();
    EXPECT_CALL(*mock, Get)
        .WillOnce(Return(ByMove(std::unique_ptr<rest_internal::RestResponse>(
            std::move(response)))));
    return std::unique_ptr<rest_internal::RestClient>(std::move(mock));
  };

  // We expect a call to the metadata service to get the service account
  // metadata. Then one call to get the token (which is cached), and finally a
  // call the refresh the token.
  auto constexpr kPayload0 =
      R"""({"email": "test-only-email", "scopes": ["s1", "s2]})""";
  auto constexpr kPayload1 = R"""({
            "access_token": "test-token-1",
            "expires_in": 3600,
            "token_type": "tokentype"
        })""";
  auto constexpr kPayload2 = R"""({
            "access_token": "test-token-2",
            "expires_in": 3600,
            "token_type": "tokentype"
        })""";

  using MockHttpClientFactory =
      ::testing::MockFunction<std::unique_ptr<rest_internal::RestClient>(
          Options const&)>;
  MockHttpClientFactory mock_factory;
  EXPECT_CALL(mock_factory, Call)
      .WillOnce(Return(ByMove(make_mock_client(kPayload0))))
      .WillOnce(Return(ByMove(make_mock_client(kPayload1))))
      .WillOnce(Return(ByMove(make_mock_client(kPayload2))));

  auto tested = ComputeEngineCredentialsTester::MakeComputeEngineCredentials(
      "test-only", mock_factory.AsStdFunction());
  auto const tp = std::chrono::system_clock::now();
  auto initial = ComputeEngineCredentialsTester::Header(tested, tp);
  ASSERT_STATUS_OK(initial);

  auto cached = ComputeEngineCredentialsTester::Header(
      tested, tp + std::chrono::seconds(30));
  EXPECT_THAT(cached, IsOkAndHolds(*initial));

  cached = ComputeEngineCredentialsTester::Header(
      tested, tp + std::chrono::seconds(300));
  EXPECT_THAT(cached, IsOkAndHolds(*initial));

  auto uncached = ComputeEngineCredentialsTester::Header(
      tested, tp + std::chrono::hours(2));
  ASSERT_STATUS_OK(uncached);
  EXPECT_NE(*initial, *uncached);
}

}  // namespace
}  // namespace oauth2
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google
