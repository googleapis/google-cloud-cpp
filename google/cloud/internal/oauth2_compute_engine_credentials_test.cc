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

using ::google::cloud::internal::GceMetadataHostname;
using ::google::cloud::rest_internal::HttpPayload;
using ::google::cloud::rest_internal::HttpStatusCode;
using ::google::cloud::rest_internal::RestRequest;
using ::google::cloud::rest_internal::RestResponse;
using ::google::cloud::testing_util::IsOk;
using ::google::cloud::testing_util::MockHttpPayload;
using ::google::cloud::testing_util::MockRestClient;
using ::google::cloud::testing_util::MockRestResponse;
using ::google::cloud::testing_util::StatusIs;
using ::testing::ByMove;
using ::testing::Contains;
using ::testing::Eq;
using ::testing::HasSubstr;
using ::testing::Not;
using ::testing::Return;
using ::testing::UnorderedElementsAre;
using ::testing::UnorderedElementsAreArray;

class ComputeEngineCredentialsTest : public ::testing::Test {
 protected:
  void SetUp() override {
    mock_rest_client_ = absl::make_unique<MockRestClient>();
  }
  std::unique_ptr<MockRestClient> mock_rest_client_;
  Options options_;
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

  auto mock_response1 = absl::make_unique<MockRestResponse>();
  EXPECT_CALL(*mock_response1, StatusCode)
      .WillRepeatedly(Return(HttpStatusCode::kOk));
  EXPECT_CALL(std::move(*mock_response1), ExtractPayload).WillOnce([&] {
    auto mock_http_payload = absl::make_unique<MockHttpPayload>();
    EXPECT_CALL(*mock_http_payload, Read)
        .WillOnce([svc_acct_info_resp](absl::Span<char> buffer) {
          std::copy(svc_acct_info_resp.begin(), svc_acct_info_resp.end(),
                    buffer.begin());
          return svc_acct_info_resp.size();
        })
        .WillOnce([](absl::Span<char>) { return 0; });
    return std::unique_ptr<HttpPayload>(std::move(mock_http_payload));
  });

  auto mock_response2 = absl::make_unique<MockRestResponse>();
  EXPECT_CALL(*mock_response2, StatusCode)
      .WillRepeatedly(Return(HttpStatusCode::kOk));
  EXPECT_CALL(std::move(*mock_response2), ExtractPayload).WillOnce([&] {
    auto mock_http_payload = absl::make_unique<MockHttpPayload>();
    EXPECT_CALL(*mock_http_payload, Read)
        .WillOnce([token_info_resp](absl::Span<char> buffer) {
          std::copy(token_info_resp.begin(), token_info_resp.end(),
                    buffer.begin());
          return token_info_resp.size();
        })
        .WillOnce([](absl::Span<char>) { return 0; });
    return std::unique_ptr<HttpPayload>(std::move(mock_http_payload));
  });

  EXPECT_CALL(*mock_rest_client_, Get)
      .WillOnce([&](RestRequest const& request) {
        EXPECT_THAT(request.GetHeader("metadata-flavor"), Contains("Google"));
        EXPECT_THAT(request.GetQueryParameter("recursive"), Contains("true"));
        EXPECT_THAT(
            request.path(),
            Eq(std::string("computeMetadata/v1/instance/service-accounts/") +
               alias + "/"));
        return std::unique_ptr<RestResponse>(std::move(mock_response1));
      })
      .WillOnce([&](RestRequest const& request) {
        EXPECT_THAT(request.GetHeader("metadata-flavor"), Contains("Google"));
        EXPECT_THAT(
            request.path(),
            Eq(std::string("computeMetadata/v1/instance/service-accounts/") +
               email + "/token"));
        return std::unique_ptr<RestResponse>(std::move(mock_response2));
      });

  ComputeEngineCredentials credentials(alias, options_,
                                       std::move(mock_rest_client_));
  // Calls Refresh to obtain the access token for our authorization header.
  EXPECT_EQ(std::make_pair(std::string{"Authorization"},
                           std::string{"tokentype mysupersecrettoken"}),
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

  auto mock_http_payload1 = absl::make_unique<MockHttpPayload>();
  EXPECT_CALL(*mock_http_payload1, Read)
      .WillOnce([token_info_resp](absl::Span<char> buffer) {
        std::copy(token_info_resp.begin(), token_info_resp.end(),
                  buffer.begin());
        return token_info_resp.size();
      })
      .WillOnce([](absl::Span<char>) { return 0; });

  auto mock_http_payload2 = absl::make_unique<MockHttpPayload>();
  EXPECT_CALL(*mock_http_payload2, Read)
      .WillOnce([token_info_resp2](absl::Span<char> buffer) {
        std::copy(token_info_resp2.begin(), token_info_resp2.end(),
                  buffer.begin());
        return token_info_resp2.size();
      })
      .WillOnce([](absl::Span<char>) { return 0; });

  auto mock_response1 = absl::make_unique<MockRestResponse>();
  EXPECT_CALL(*mock_response1, StatusCode)
      .WillRepeatedly(Return(HttpStatusCode::kBadRequest));
  EXPECT_CALL(std::move(*mock_response1), ExtractPayload)
      .WillOnce(Return(ByMove(std::move(mock_http_payload1))));

  auto mock_response2 = absl::make_unique<MockRestResponse>();
  EXPECT_CALL(*mock_response2, StatusCode)
      .WillRepeatedly(Return(HttpStatusCode::kBadRequest));
  EXPECT_CALL(std::move(*mock_response2), ExtractPayload)
      .WillOnce(Return(ByMove(std::move(mock_http_payload2))));

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
TEST_F(ComputeEngineCredentialsTest, ParseComputeEngineRefreshResponse) {
  std::string token_info_resp = R"""({
      "access_token": "mysupersecrettoken",
      "expires_in": 3600,
      "token_type": "tokentype"
})""";

  auto mock_http_payload = absl::make_unique<MockHttpPayload>();
  EXPECT_CALL(*mock_http_payload, Read)
      .WillOnce([token_info_resp](absl::Span<char> buffer) {
        std::copy(token_info_resp.begin(), token_info_resp.end(),
                  buffer.begin());
        return token_info_resp.size();
      })
      .WillOnce([](absl::Span<char>) { return 0; });

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
TEST_F(ComputeEngineCredentialsTest, ParseMetadataServerResponse) {
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
    auto mock_http_payload = absl::make_unique<MockHttpPayload>();
    EXPECT_CALL(*mock_http_payload, Read)
        .WillOnce([test](absl::Span<char> buffer) {
          std::copy(test.payload.begin(), test.payload.end(), buffer.begin());
          return test.payload.size();
        })
        .WillOnce([](absl::Span<char>) { return 0; });

    auto mock_response = absl::make_unique<MockRestResponse>();
    EXPECT_CALL(*mock_response, StatusCode)
        .WillRepeatedly(Return(HttpStatusCode::kOk));
    EXPECT_CALL(std::move(*mock_response), ExtractPayload)
        .WillOnce(Return(ByMove(std::move(mock_http_payload))));

    auto status = ParseMetadataServerResponse(*mock_response);
    ASSERT_STATUS_OK(status);
    auto metadata = *status;
    EXPECT_EQ(metadata.email, test.expected.email);
    EXPECT_THAT(metadata.scopes,
                UnorderedElementsAreArray(test.expected.scopes));
  }
}

/// @test Mock a failed refresh response during RetrieveServiceAccountInfo.
TEST_F(ComputeEngineCredentialsTest, FailedRetrieveServiceAccountInfo) {
  std::string alias = "default";
  std::string email = "foo@bar.baz";
  std::string hostname = GceMetadataHostname();

  auto mock_response2 = absl::make_unique<MockRestResponse>();
  EXPECT_CALL(*mock_response2, StatusCode)
      .WillRepeatedly(Return(HttpStatusCode::kBadRequest));
  EXPECT_CALL(std::move(*mock_response2), ExtractPayload).WillOnce([&] {
    auto mock_http_payload = absl::make_unique<MockHttpPayload>();
    EXPECT_CALL(*mock_http_payload, Read).WillOnce([](absl::Span<char>) {
      return 0;
    });
    return std::unique_ptr<HttpPayload>(std::move(mock_http_payload));
  });

  EXPECT_CALL(*mock_rest_client_, Get)
      .WillOnce([&](RestRequest const& request) {
        EXPECT_THAT(request.GetHeader("metadata-flavor"), Contains("Google"));
        EXPECT_THAT(request.GetQueryParameter("recursive"), Contains("true"));
        EXPECT_THAT(
            request.path(),
            Eq(std::string("computeMetadata/v1/instance/service-accounts/") +
               alias + "/"));
        return Status{StatusCode::kAborted, "Fake Curl error", {}};
      })
      .WillOnce([&](RestRequest const& request) {
        EXPECT_THAT(request.GetHeader("metadata-flavor"), Contains("Google"));
        EXPECT_THAT(request.GetQueryParameter("recursive"), Contains("true"));
        EXPECT_THAT(
            request.path(),
            Eq(std::string("computeMetadata/v1/instance/service-accounts/") +
               alias + "/"));
        return std::unique_ptr<RestResponse>(std::move(mock_response2));
      });

  ComputeEngineCredentials credentials(alias, options_,
                                       std::move(mock_rest_client_));
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

  auto mock_response2 = absl::make_unique<MockRestResponse>();
  EXPECT_CALL(*mock_response2, StatusCode)
      .WillRepeatedly(Return(HttpStatusCode::kOk));
  EXPECT_CALL(std::move(*mock_response2), ExtractPayload).WillOnce([&] {
    auto mock_http_payload = absl::make_unique<MockHttpPayload>();
    EXPECT_CALL(*mock_http_payload, Read)
        .WillOnce([svc_acct_info_resp](absl::Span<char> buffer) {
          std::copy(svc_acct_info_resp.begin(), svc_acct_info_resp.end(),
                    buffer.begin());
          return svc_acct_info_resp.size();
        })
        .WillOnce([](absl::Span<char>) { return 0; });
    return std::unique_ptr<HttpPayload>(std::move(mock_http_payload));
  });

  auto mock_response4 = absl::make_unique<MockRestResponse>();
  EXPECT_CALL(*mock_response4, StatusCode)
      .WillRepeatedly(Return(HttpStatusCode::kOk));
  EXPECT_CALL(std::move(*mock_response4), ExtractPayload).WillOnce([&] {
    auto mock_http_payload = absl::make_unique<MockHttpPayload>();
    EXPECT_CALL(*mock_http_payload, Read)
        .WillOnce([svc_acct_info_resp](absl::Span<char> buffer) {
          std::copy(svc_acct_info_resp.begin(), svc_acct_info_resp.end(),
                    buffer.begin());
          return svc_acct_info_resp.size();
        })
        .WillOnce([](absl::Span<char>) { return 0; });
    return std::unique_ptr<HttpPayload>(std::move(mock_http_payload));
  });

  auto mock_response5 = absl::make_unique<MockRestResponse>();
  EXPECT_CALL(*mock_response5, StatusCode)
      .WillRepeatedly(Return(HttpStatusCode::kBadRequest));
  EXPECT_CALL(std::move(*mock_response5), ExtractPayload).WillOnce([&] {
    auto mock_http_payload = absl::make_unique<MockHttpPayload>();
    EXPECT_CALL(*mock_http_payload, Read).WillOnce([](absl::Span<char>) {
      return 0;
    });
    return std::unique_ptr<HttpPayload>(std::move(mock_http_payload));
  });

  auto mock_response6 = absl::make_unique<MockRestResponse>();
  EXPECT_CALL(*mock_response6, StatusCode)
      .WillRepeatedly(Return(HttpStatusCode::kOk));
  EXPECT_CALL(std::move(*mock_response6), ExtractPayload).WillOnce([&] {
    auto mock_http_payload = absl::make_unique<MockHttpPayload>();
    EXPECT_CALL(*mock_http_payload, Read)
        .WillOnce([svc_acct_info_resp](absl::Span<char> buffer) {
          std::copy(svc_acct_info_resp.begin(), svc_acct_info_resp.end(),
                    buffer.begin());
          return svc_acct_info_resp.size();
        })
        .WillOnce([](absl::Span<char>) { return 0; });
    return std::unique_ptr<HttpPayload>(std::move(mock_http_payload));
  });

  auto mock_response7 = absl::make_unique<MockRestResponse>();
  EXPECT_CALL(*mock_response7, StatusCode)
      .WillRepeatedly(Return(HttpStatusCode::kOk));
  EXPECT_CALL(std::move(*mock_response7), ExtractPayload).WillOnce([&] {
    auto mock_http_payload = absl::make_unique<MockHttpPayload>();
    EXPECT_CALL(*mock_http_payload, Read)
        .WillOnce([token_info_resp](absl::Span<char> buffer) {
          std::copy(token_info_resp.begin(), token_info_resp.end(),
                    buffer.begin());
          return token_info_resp.size();
        })
        .WillOnce([](absl::Span<char>) { return 0; });
    return std::unique_ptr<HttpPayload>(std::move(mock_http_payload));
  });

  // Fail the first call to RetrieveServiceAccountInfo immediately.
  EXPECT_CALL(*mock_rest_client_, Get)
      .WillOnce([&](RestRequest const& request) {
        EXPECT_THAT(request.GetHeader("metadata-flavor"), Contains("Google"));
        EXPECT_THAT(request.GetQueryParameter("recursive"), Contains("true"));
        // For the first expected failures, the alias is used until the metadata
        // request succeeds.
        EXPECT_THAT(
            request.path(),
            Eq(std::string("computeMetadata/v1/instance/service-accounts/") +
               alias + "/"));
        return Status{StatusCode::kAborted, "Fake Curl error", {}};
      })
      // Make the call to RetrieveServiceAccountInfo return a good response,
      .WillOnce([&](RestRequest const& request) {
        EXPECT_THAT(request.GetHeader("metadata-flavor"), Contains("Google"));
        EXPECT_THAT(request.GetQueryParameter("recursive"), Contains("true"));
        // For the first expected failures, the alias is used until the metadata
        // request succeeds.
        EXPECT_THAT(
            request.path(),
            Eq(std::string("computeMetadata/v1/instance/service-accounts/") +
               alias + "/"));
        return std::unique_ptr<RestResponse>(std::move(mock_response2));
      })
      // but fail the token request immediately.
      .WillOnce([&](RestRequest const& request) {
        EXPECT_THAT(request.GetHeader("metadata-flavor"), Contains("Google"));
        EXPECT_THAT(
            request.path(),
            Eq(std::string("computeMetadata/v1/instance/service-accounts/") +
               email + "/token"));
        return Status{StatusCode::kAborted, "Fake Curl error", {}};
      })
      // Make the call to RetrieveServiceAccountInfo return a good response,
      .WillOnce([&](RestRequest const& request) {
        EXPECT_THAT(request.GetHeader("metadata-flavor"), Contains("Google"));
        EXPECT_THAT(request.GetQueryParameter("recursive"), Contains("true"));
        // Now that the first request has succeeded and the metadata has been
        // retrieved, the the email is used for refresh.
        EXPECT_THAT(
            request.path(),
            Eq(std::string("computeMetadata/v1/instance/service-accounts/") +
               email + "/"));
        return std::unique_ptr<RestResponse>(std::move(mock_response4));
      })
      // but fail the token request with a bad HTTP error code.
      .WillOnce([&](RestRequest const& request) {
        EXPECT_THAT(request.GetHeader("metadata-flavor"), Contains("Google"));
        EXPECT_THAT(
            request.path(),
            Eq(std::string("computeMetadata/v1/instance/service-accounts/") +
               email + "/token"));
        return std::unique_ptr<RestResponse>(std::move(mock_response5));
      })
      // Make the call to RetrieveServiceAccountInfo return a good response,
      .WillOnce([&](RestRequest const& request) {
        EXPECT_THAT(request.GetHeader("metadata-flavor"), Contains("Google"));
        EXPECT_THAT(request.GetQueryParameter("recursive"), Contains("true"));
        // Now that the first request has succeeded and the metadata has been
        // retrieved, the the email is used for refresh.
        EXPECT_THAT(
            request.path(),
            Eq(std::string("computeMetadata/v1/instance/service-accounts/") +
               email + "/"));
        return std::unique_ptr<RestResponse>(std::move(mock_response6));
      })
      // but, parse with an invalid token response.
      .WillOnce([&](RestRequest const& request) {
        EXPECT_THAT(request.GetHeader("metadata-flavor"), Contains("Google"));
        EXPECT_THAT(
            request.path(),
            Eq(std::string("computeMetadata/v1/instance/service-accounts/") +
               email + "/token"));
        return std::unique_ptr<RestResponse>(std::move(mock_response7));
      });

  ComputeEngineCredentials credentials(alias, options_,
                                       std::move(mock_rest_client_));
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

  auto mock_response1 = absl::make_unique<MockRestResponse>();
  EXPECT_CALL(*mock_response1, StatusCode)
      .WillRepeatedly(Return(HttpStatusCode::kOk));
  EXPECT_CALL(std::move(*mock_response1), ExtractPayload).WillOnce([&] {
    auto mock_http_payload = absl::make_unique<MockHttpPayload>();
    EXPECT_CALL(*mock_http_payload, Read)
        .WillOnce([svc_acct_info_resp](absl::Span<char> buffer) {
          std::copy(svc_acct_info_resp.begin(), svc_acct_info_resp.end(),
                    buffer.begin());
          return svc_acct_info_resp.size();
        })
        .WillOnce([](absl::Span<char>) { return 0; });
    return std::unique_ptr<HttpPayload>(std::move(mock_http_payload));
  });

  EXPECT_CALL(*mock_rest_client_, Get)
      .WillOnce([&](RestRequest const& request) {
        EXPECT_THAT(request.GetHeader("metadata-flavor"), Contains("Google"));
        EXPECT_THAT(request.GetQueryParameter("recursive"), Contains("true"));
        EXPECT_THAT(
            request.path(),
            Eq(std::string("computeMetadata/v1/instance/service-accounts/") +
               alias + "/"));
        return std::unique_ptr<RestResponse>(std::move(mock_response1));
      });

  ComputeEngineCredentials credentials(alias, options_,
                                       std::move(mock_rest_client_));
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
