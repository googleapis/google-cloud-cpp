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

#include "google/cloud/internal/oauth2_compute_engine_credentials.h"
#include "google/cloud/internal/absl_str_cat_quiet.h"
#include "google/cloud/internal/compute_engine_util.h"
#include "google/cloud/testing_util/mock_http_payload.h"
#include "google/cloud/testing_util/mock_rest_client.h"
#include "google/cloud/testing_util/mock_rest_response.h"
#include "google/cloud/testing_util/status_matchers.h"
#include "absl/memory/memory.h"
#include <gmock/gmock.h>
#include <chrono>

namespace google {
namespace cloud {
namespace oauth2_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::google::cloud::rest_internal::HttpStatusCode;
using ::google::cloud::rest_internal::RestRequest;
using ::google::cloud::rest_internal::RestResponse;
using ::google::cloud::testing_util::IsOk;
using ::google::cloud::testing_util::MakeMockHttpPayloadSuccess;
using ::google::cloud::testing_util::MockRestClient;
using ::google::cloud::testing_util::MockRestResponse;
using ::google::cloud::testing_util::StatusIs;
using ::testing::AllOf;
using ::testing::ByMove;
using ::testing::Contains;
using ::testing::HasSubstr;
using ::testing::Not;
using ::testing::Pair;
using ::testing::Property;
using ::testing::Return;
using ::testing::UnorderedElementsAre;
using ::testing::UnorderedElementsAreArray;

using MockHttpClientFactory =
    ::testing::MockFunction<std::unique_ptr<rest_internal::RestClient>(
        Options const&)>;

auto expect_service_config = [](std::string const& account) {
  auto const expected_path = absl::StrCat(
      internal::GceMetadataScheme(), "://", internal::GceMetadataHostname(),
      "/computeMetadata/v1/instance/service-accounts/", account, "/");
  return AllOf(
      Property(&RestRequest::path, expected_path),
      Property(&RestRequest::headers,
               Contains(Pair("metadata-flavor", Contains("Google")))),
      Property(&RestRequest::parameters, Contains(Pair("recursive", "true"))));
};

auto expect_token = [](std::string const& account) {
  auto const expected_path = absl::StrCat(
      internal::GceMetadataScheme(), "://", internal::GceMetadataHostname(),
      "/computeMetadata/v1/instance/service-accounts/", account, "/token");
  return AllOf(Property(&RestRequest::path, expected_path),
               Property(&RestRequest::headers,
                        Contains(Pair("metadata-flavor", Contains("Google")))),
               Property(&RestRequest::parameters,
                        Not(Contains(Pair("recursive", "true")))));
};

/// @test Verify that we can create and refresh ComputeEngineCredentials.
TEST(ComputeEngineCredentialsTest,
     RefreshingSendsCorrectRequestBodyAndParsesResponse) {
  auto const alias = std::string{"default"};
  auto const email = std::string{"foo@bar.baz"};
  auto const svc_acct_info_resp = std::string{R"""({
      "email": ")""" + email + R"""(",
      "scopes": ["scope1","scope2"]
  })"""};
  auto const token_info_resp = std::string{R"""({
      "access_token": "mysupersecrettoken",
      "expires_in": 3600,
      "token_type": "tokentype"
  })"""};

  auto mock_metadata_client = [&]() {
    auto response = absl::make_unique<MockRestResponse>();
    EXPECT_CALL(*response, StatusCode)
        .WillRepeatedly(Return(HttpStatusCode::kOk));
    EXPECT_CALL(std::move(*response), ExtractPayload)
        .WillOnce(
            Return(ByMove(MakeMockHttpPayloadSuccess(svc_acct_info_resp))));

    auto mock = absl::make_unique<MockRestClient>();
    EXPECT_CALL(*mock, Get(expect_service_config(alias)))
        .WillOnce(
            Return(ByMove(std::unique_ptr<RestResponse>(std::move(response)))));
    return mock;
  }();

  auto mock_token_client = [&]() {
    auto response = absl::make_unique<MockRestResponse>();
    EXPECT_CALL(*response, StatusCode)
        .WillRepeatedly(Return(HttpStatusCode::kOk));
    EXPECT_CALL(std::move(*response), ExtractPayload)
        .WillOnce(Return(ByMove(MakeMockHttpPayloadSuccess(token_info_resp))));

    auto mock = absl::make_unique<MockRestClient>();
    EXPECT_CALL(*mock, Get(expect_token(email)))
        .WillOnce(
            Return(ByMove(std::unique_ptr<RestResponse>(std::move(response)))));
    return mock;
  }();

  MockHttpClientFactory mock_http_client_factory;
  EXPECT_CALL(mock_http_client_factory, Call)
      .WillOnce(Return(ByMove(std::move(mock_metadata_client))))
      .WillOnce(Return(ByMove(std::move(mock_token_client))));
  ComputeEngineCredentials credentials(
      alias, Options{}, mock_http_client_factory.AsStdFunction());
  // Calls Refresh to obtain the access token for our authorization header.
  auto const now = std::chrono::system_clock::now();
  auto const expected_token = internal::AccessToken{
      "mysupersecrettoken", now + std::chrono::seconds(3600)};
  EXPECT_EQ(expected_token, credentials.GetToken(now).value());
  // Make sure we obtain the scopes and email from the metadata server.
  EXPECT_EQ(email, credentials.service_account_email());
  EXPECT_THAT(credentials.scopes(), UnorderedElementsAre("scope1", "scope2"));
}

/// @test Parsing a refresh response with missing fields results in failure.
TEST(ComputeEngineCredentialsTest,
     ParseComputeEngineRefreshResponseMissingFields) {
  std::string token_info_resp = R"""({})""";
  // Does not have access_token.
  std::string token_info_resp2 = R"""({
      "expires_in": 3600,
      "token_type": "tokentype"
)""";

  auto mock_response1 = absl::make_unique<MockRestResponse>();
  EXPECT_CALL(*mock_response1, StatusCode)
      .WillRepeatedly(Return(HttpStatusCode::kBadRequest));
  EXPECT_CALL(std::move(*mock_response1), ExtractPayload)
      .WillOnce(Return(ByMove(MakeMockHttpPayloadSuccess(token_info_resp))));

  auto mock_response2 = absl::make_unique<MockRestResponse>();
  EXPECT_CALL(*mock_response2, StatusCode)
      .WillRepeatedly(Return(HttpStatusCode::kBadRequest));
  EXPECT_CALL(std::move(*mock_response2), ExtractPayload)
      .WillOnce(Return(ByMove(MakeMockHttpPayloadSuccess(token_info_resp2))));

  auto status = ParseComputeEngineRefreshResponse(
      *mock_response1, std::chrono::system_clock::from_time_t(1000));
  EXPECT_THAT(status,
              StatusIs(StatusCode::kInvalidArgument,
                       HasSubstr("Could not find all required fields")));

  status = ParseComputeEngineRefreshResponse(
      *mock_response2, std::chrono::system_clock::from_time_t(1000));
  EXPECT_THAT(status,
              StatusIs(StatusCode::kInvalidArgument,
                       HasSubstr("Could not find all required fields")));
}

/// @test Parsing a refresh response yields an access token.
TEST(ComputeEngineCredentialsTest, ParseComputeEngineRefreshResponse) {
  auto const token_info_resp = std::string{R"""({
      "access_token": "mysupersecrettoken",
      "expires_in": 3600,
      "token_type": "tokentype"})"""};

  auto mock_http_payload = MakeMockHttpPayloadSuccess(token_info_resp);

  auto mock_response = absl::make_unique<MockRestResponse>();
  EXPECT_CALL(*mock_response, StatusCode)
      .WillRepeatedly(Return(HttpStatusCode::kBadRequest));
  EXPECT_CALL(std::move(*mock_response), ExtractPayload)
      .WillOnce(Return(ByMove(std::move(mock_http_payload))));

  auto const now = std::chrono::system_clock::now();
  auto const expires_in = std::chrono::seconds(3600);

  auto status = ParseComputeEngineRefreshResponse(*mock_response, now);
  EXPECT_STATUS_OK(status);
  auto token = *status;
  EXPECT_EQ(token.expiration, now + expires_in);
  EXPECT_EQ(token.token, "mysupersecrettoken");
}

/// @test Parsing a metadata server response yields a ServiceAccountMetadata.
TEST(ComputeEngineCredentialsTest, ParseMetadataServerResponse) {
  struct TestCase {
    std::string payload;
    ServiceAccountMetadata expected;
  } cases[] = {
      {R"js({"email": "foo@bar.baz", "scopes": ["scope1", "scope2"]})js",
       ServiceAccountMetadata{{"scope1", "scope2"}, "foo@bar.baz"}},
      {R"js({"email": "foo@bar.baz", "scopes": "scope1\nscope2\n"})js",
       ServiceAccountMetadata{{"scope1", "scope2"}, "foo@bar.baz"}},
      // Ignore invalid formats
      {R"js({"email": ["1", "2"], "scopes": ["scope1", "scope2"]})js",
       ServiceAccountMetadata{{"scope1", "scope2"}, ""}},
      {R"js({"email": "foo@bar", "scopes": {"foo": "bar"}})js",
       ServiceAccountMetadata{{}, "foo@bar"}},
      // Ignore missing fields
      {R"js({"scopes": ["scope1", "scope2"]})js",
       ServiceAccountMetadata{{"scope1", "scope2"}, ""}},
      {R"js({"email": "foo@bar.baz"})js",
       ServiceAccountMetadata{{}, "foo@bar.baz"}},
      {R"js({})js", ServiceAccountMetadata{{}, ""}},
  };

  for (auto const& test : cases) {
    SCOPED_TRACE("testing with " + test.payload);

    auto mock_response = absl::make_unique<MockRestResponse>();
    EXPECT_CALL(*mock_response, StatusCode)
        .WillRepeatedly(Return(HttpStatusCode::kOk));
    EXPECT_CALL(std::move(*mock_response), ExtractPayload)
        .WillOnce(Return(ByMove(MakeMockHttpPayloadSuccess(test.payload))));

    auto status = ParseMetadataServerResponse(*mock_response);
    ASSERT_STATUS_OK(status);
    auto metadata = *status;
    EXPECT_EQ(metadata.email, test.expected.email);
    EXPECT_THAT(metadata.scopes,
                UnorderedElementsAreArray(test.expected.scopes));
  }
}

/// @test Mock a failed refresh response during RetrieveServiceAccountInfo.
TEST(ComputeEngineCredentialsTest, FailedRetrieveServiceAccountInfo) {
  auto const alias = std::string{"default"};
  auto const email = std::string{"foo@bar.baz"};
  auto const token_info_resp = std::string{R"""({
      "access_token": "mysupersecrettoken",
      "expires_in": 3600,
      "token_type": "tokentype"})"""};

  auto mock_metadata_client_get_error = [&]() {
    auto mock = absl::make_unique<MockRestClient>();
    EXPECT_CALL(*mock, Get(expect_service_config(alias)))
        .WillOnce(Return(Status{StatusCode::kAborted, "Fake Curl error"}));
    return mock;
  }();

  auto mock_metadata_client_response_error = [&]() {
    auto mock = absl::make_unique<MockRestClient>();
    EXPECT_CALL(*mock, Get(expect_service_config(alias))).WillOnce([] {
      auto response = absl::make_unique<MockRestResponse>();
      EXPECT_CALL(*response, StatusCode)
          .WillRepeatedly(Return(HttpStatusCode::kBadRequest));
      return std::unique_ptr<RestResponse>(std::move(response));
    });
    return mock;
  }();

  MockHttpClientFactory mock_http_client_factory;
  EXPECT_CALL(mock_http_client_factory, Call)
      .WillOnce(Return(ByMove(std::move(mock_metadata_client_get_error))))
      .WillOnce(Return(ByMove(std::move(mock_metadata_client_response_error))));

  ComputeEngineCredentials credentials(
      alias, Options{}, mock_http_client_factory.AsStdFunction());
  // Response 1
  auto actual = credentials.AccountEmail();
  EXPECT_EQ(actual, alias);
  // Response 2
  actual = credentials.AccountEmail();
  EXPECT_EQ(actual, alias);
}

/// @test Mock a failed refresh response.
TEST(ComputeEngineCredentialsTest, FailedRefresh) {
  auto const alias = std::string{"default"};
  auto const email = std::string{"foo@bar.baz"};
  auto const svc_acct_info_resp = std::string{R"""({
      "email": "foo@bar.baz",
      "scopes": ["scope1","scope2"]
  })"""};
  // Note this response is missing a field.
  auto const token_info_resp = std::string{R"""({
      "expires_in": 3600,
      "token_type": "tokentype"
  })"""};

  // Fail the first call to RetrieveServiceAccountInfo immediately.
  auto metadata_aborted = [&]() {
    auto client = absl::make_unique<MockRestClient>();
    EXPECT_CALL(*client, Get(expect_service_config(alias)))
        .WillOnce([&](RestRequest const&) {
          return Status{StatusCode::kAborted, "Fake Curl error / info", {}};
        });
    return client;
  }();
  // Then fail the token request immediately.
  auto token_aborted = [&]() {
    auto client = absl::make_unique<MockRestClient>();
    EXPECT_CALL(*client, Get(expect_token(alias)))
        .WillOnce([&](RestRequest const&) {
          return Status{StatusCode::kAborted, "Fake Curl error / token", {}};
        });
    return client;
  }();
  // Since the service config request failed, it will be attempted again. This
  // time have it succeed.
  auto metadata_success = [&]() {
    auto response = absl::make_unique<MockRestResponse>();
    EXPECT_CALL(*response, StatusCode)
        .WillRepeatedly(Return(HttpStatusCode::kOk));
    EXPECT_CALL(std::move(*response), ExtractPayload).WillOnce([&]() {
      return MakeMockHttpPayloadSuccess(svc_acct_info_resp);
    });

    auto client = absl::make_unique<MockRestClient>();
    EXPECT_CALL(*client, Get(expect_service_config(alias)))
        .WillOnce(
            Return(ByMove(std::unique_ptr<RestResponse>(std::move(response)))));
    return client;
  }();
  // Make the token request fail. Now with a bad HTTP error code.
  auto token_bad_http = [&]() {
    auto response = absl::make_unique<MockRestResponse>();
    EXPECT_CALL(*response, StatusCode)
        .WillRepeatedly(Return(HttpStatusCode::kBadRequest));
    EXPECT_CALL(std::move(*response), ExtractPayload).WillOnce([&] {
      return MakeMockHttpPayloadSuccess(std::string{});
    });

    auto client = absl::make_unique<MockRestClient>();
    EXPECT_CALL(*client, Get(expect_token(email)))
        .WillOnce(
            Return(ByMove(std::unique_ptr<RestResponse>(std::move(response)))));
    return client;
  }();
  // And fail again, now with an incomplete response.
  auto token_incomplete = [&]() {
    auto response = absl::make_unique<MockRestResponse>();
    EXPECT_CALL(*response, StatusCode)
        .WillRepeatedly(Return(HttpStatusCode::kOk));
    EXPECT_CALL(std::move(*response), ExtractPayload).WillOnce([&] {
      return MakeMockHttpPayloadSuccess(token_info_resp);
    });

    auto client = absl::make_unique<MockRestClient>();
    EXPECT_CALL(*client, Get(expect_token(email)))
        .WillOnce(
            Return(ByMove(std::unique_ptr<RestResponse>(std::move(response)))));
    return client;
  }();

  MockHttpClientFactory client_factory;
  EXPECT_CALL(client_factory, Call)
      .WillOnce(Return(ByMove(std::move(metadata_aborted))))
      .WillOnce(Return(ByMove(std::move(token_aborted))))
      .WillOnce(Return(ByMove(std::move(metadata_success))))
      .WillOnce(Return(ByMove(std::move(token_bad_http))))
      .WillOnce(Return(ByMove(std::move(token_incomplete))));
  ComputeEngineCredentials credentials(alias, Options{},
                                       client_factory.AsStdFunction());
  auto const now = std::chrono::system_clock::now();
  auto status = credentials.GetToken(now);
  EXPECT_THAT(status, StatusIs(StatusCode::kAborted,
                               HasSubstr("Fake Curl error / token")));
  status = credentials.GetToken(now);
  EXPECT_THAT(status, Not(IsOk()));
  status = credentials.GetToken(now);
  EXPECT_THAT(status,
              StatusIs(Not(StatusCode::kOk),
                       HasSubstr("Could not find all required fields")));
}

/// @test Verify that we can force a refresh of the service account email.
TEST(ComputeEngineCredentialsTest, AccountEmail) {
  auto const alias = std::string{"default"};
  auto const email = std::string{"foo@bar.baz"};
  auto const svc_acct_info_resp = std::string{R"""({
      "email": ")""" + email + R"""(",
      "scopes": ["scope1","scope2"]
  })"""};

  auto client = absl::make_unique<MockRestClient>();
  EXPECT_CALL(*client, Get(expect_service_config(alias)))
      .WillOnce([&](RestRequest const&) {
        auto response = absl::make_unique<MockRestResponse>();
        EXPECT_CALL(*response, StatusCode)
            .WillRepeatedly(Return(HttpStatusCode::kOk));
        EXPECT_CALL(std::move(*response), ExtractPayload).WillOnce([&] {
          return MakeMockHttpPayloadSuccess(svc_acct_info_resp);
        });
        return std::unique_ptr<RestResponse>(std::move(response));
      });

  MockHttpClientFactory client_factory;
  EXPECT_CALL(client_factory, Call).WillOnce(Return(ByMove(std::move(client))));
  ComputeEngineCredentials credentials(alias, Options{},
                                       client_factory.AsStdFunction());
  EXPECT_EQ(credentials.service_account_email(), alias);
  auto refreshed_email = credentials.AccountEmail();
  EXPECT_EQ(email, refreshed_email);
  EXPECT_EQ(credentials.service_account_email(), refreshed_email);
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace oauth2_internal
}  // namespace cloud
}  // namespace google
