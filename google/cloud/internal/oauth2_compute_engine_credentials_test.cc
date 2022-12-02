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

auto expect_service_config = [](std::string const& account) {
  auto const expected_path = absl::StrCat(
      "computeMetadata/v1/instance/service-accounts/", account, "/");
  return AllOf(
      Property(&RestRequest::path, expected_path),
      Property(&RestRequest::headers,
               Contains(Pair("metadata-flavor", Contains("Google")))),
      Property(&RestRequest::parameters, Contains(Pair("recursive", "true"))));
};

auto expect_token = [](std::string const& account) {
  auto const expected_path = absl::StrCat(
      "computeMetadata/v1/instance/service-accounts/", account, "/token");
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

  auto mock_response1 = absl::make_unique<MockRestResponse>();
  EXPECT_CALL(*mock_response1, StatusCode)
      .WillRepeatedly(Return(HttpStatusCode::kOk));
  EXPECT_CALL(std::move(*mock_response1), ExtractPayload).WillOnce([&] {
    return MakeMockHttpPayloadSuccess(svc_acct_info_resp);
  });

  auto mock_response2 = absl::make_unique<MockRestResponse>();
  EXPECT_CALL(*mock_response2, StatusCode)
      .WillRepeatedly(Return(HttpStatusCode::kOk));
  EXPECT_CALL(std::move(*mock_response2), ExtractPayload).WillOnce([&] {
    return MakeMockHttpPayloadSuccess(token_info_resp);
  });

  auto mock_rest_client = absl::make_unique<MockRestClient>();
  EXPECT_CALL(*mock_rest_client, Get(expect_service_config(alias)))
      .WillOnce([&](RestRequest const&) {
        return std::unique_ptr<RestResponse>(std::move(mock_response1));
      });
  EXPECT_CALL(*mock_rest_client, Get(expect_token(email)))
      .WillOnce([&](RestRequest const&) {
        return std::unique_ptr<RestResponse>(std::move(mock_response2));
      });

  ComputeEngineCredentials credentials(alias, Options{},
                                       std::move(mock_rest_client));
  // Calls Refresh to obtain the access token for our authorization header.
  EXPECT_EQ(std::make_pair(std::string{"Authorization"},
                           std::string{"tokentype mysupersecrettoken"}),
            credentials.AuthorizationHeader().value());
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

  auto expires_in = 3600;
  auto clock_value = 2000;

  auto status = ParseComputeEngineRefreshResponse(
      *mock_response, std::chrono::system_clock::from_time_t(clock_value));
  EXPECT_STATUS_OK(status);
  auto token = *status;
  EXPECT_EQ(std::chrono::time_point_cast<std::chrono::seconds>(token.expiration)
                .time_since_epoch()
                .count(),
            clock_value + expires_in);
  EXPECT_EQ(token.token, "tokentype mysupersecrettoken");
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

  ::testing::InSequence sequence;
  auto mock_rest_client = absl::make_unique<MockRestClient>();
  EXPECT_CALL(*mock_rest_client, Get(expect_service_config(alias)))
      .WillOnce([] {
        return Status{StatusCode::kAborted, "Fake Curl error"};
      });
  EXPECT_CALL(*mock_rest_client, Get(expect_service_config(alias)))
      .WillOnce([&] {
        auto response = absl::make_unique<MockRestResponse>();
        EXPECT_CALL(*response, StatusCode)
            .WillRepeatedly(Return(HttpStatusCode::kBadRequest));
        return std::unique_ptr<oauth2_internal::RestResponse>(
            std::move(response));
      });

  ComputeEngineCredentials credentials(alias, Options{},
                                       std::move(mock_rest_client));
  // Response 1
  auto actual = credentials.AccountEmail();
  EXPECT_EQ(actual, alias);
  // Response 2
  actual = credentials.AccountEmail();
  EXPECT_THAT(actual, alias);
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

  // Create the mock functions before the `::testing::InSequence`
  auto svc_acct_info_success = [&](RestRequest const&) {
    auto response = absl::make_unique<MockRestResponse>();
    EXPECT_CALL(*response, StatusCode)
        .WillRepeatedly(Return(HttpStatusCode::kOk));
    EXPECT_CALL(std::move(*response), ExtractPayload).WillOnce([&] {
      return MakeMockHttpPayloadSuccess(svc_acct_info_resp);
    });
    return std::unique_ptr<RestResponse>(std::move(response));
  };
  auto token_http_error = [&](RestRequest const&) {
    auto response = absl::make_unique<MockRestResponse>();
    EXPECT_CALL(*response, StatusCode)
        .WillRepeatedly(Return(HttpStatusCode::kBadRequest));
    EXPECT_CALL(std::move(*response), ExtractPayload).WillOnce([&] {
      return MakeMockHttpPayloadSuccess(std::string{});
    });

    return std::unique_ptr<RestResponse>(std::move(response));
  };
  auto token_incomplete = [&](RestRequest const&) {
    auto response = absl::make_unique<MockRestResponse>();
    EXPECT_CALL(*response, StatusCode)
        .WillRepeatedly(Return(HttpStatusCode::kOk));
    EXPECT_CALL(std::move(*response), ExtractPayload).WillOnce([&] {
      return MakeMockHttpPayloadSuccess(token_info_resp);
    });
    return std::unique_ptr<RestResponse>(std::move(response));
  };

  auto mock_rest_client = absl::make_unique<MockRestClient>();
  {
    // We need a new scope because the nested calls can (and should) execute
    // in a different order.
    ::testing::InSequence sequence;
    // Fail the first call to RetrieveServiceAccountInfo immediately.
    EXPECT_CALL(*mock_rest_client, Get(expect_service_config(alias)))
        .WillOnce([&](RestRequest const&) {
          return Status{StatusCode::kAborted, "Fake Curl error / info", {}};
        });
    // Then fail the token request immediately.
    EXPECT_CALL(*mock_rest_client, Get(expect_token(alias)))
        .WillOnce([&](RestRequest const&) {
          return Status{StatusCode::kAborted, "Fake Curl error / token", {}};
        });
    // Since the service config request failed, it will be attempted again. This
    // time have it succeed.
    EXPECT_CALL(*mock_rest_client, Get(expect_service_config(alias)))
        .WillOnce(svc_acct_info_success);
    // Make the token request fail. Now with a bad HTTP error code.
    EXPECT_CALL(*mock_rest_client, Get(expect_token(email)))
        .WillOnce(token_http_error);
    // And fail again, now with an incomplete response.
    EXPECT_CALL(*mock_rest_client, Get(expect_token(email)))
        .WillOnce(token_incomplete);
  }
  ComputeEngineCredentials credentials(alias, Options{},
                                       std::move(mock_rest_client));
  auto status = credentials.AuthorizationHeader();
  EXPECT_THAT(status, StatusIs(StatusCode::kAborted,
                               HasSubstr("Fake Curl error / token")));
  status = credentials.AuthorizationHeader();
  EXPECT_THAT(status, Not(IsOk()));
  status = credentials.AuthorizationHeader();
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

  auto mock_rest_client = absl::make_unique<MockRestClient>();
  EXPECT_CALL(*mock_rest_client, Get(expect_service_config(alias)))
      .WillOnce([&](RestRequest const&) {
        auto response = absl::make_unique<MockRestResponse>();
        EXPECT_CALL(*response, StatusCode)
            .WillRepeatedly(Return(HttpStatusCode::kOk));
        EXPECT_CALL(std::move(*response), ExtractPayload).WillOnce([&] {
          return MakeMockHttpPayloadSuccess(svc_acct_info_resp);
        });
        return std::unique_ptr<RestResponse>(std::move(response));
      });

  ComputeEngineCredentials credentials(alias, Options{},
                                       std::move(mock_rest_client));
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
