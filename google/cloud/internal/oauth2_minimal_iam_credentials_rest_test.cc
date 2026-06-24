// Copyright 2022 Google LLC
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

#include "google/cloud/internal/oauth2_minimal_iam_credentials_rest.h"
#include "google/cloud/internal/http_payload.h"
#include "google/cloud/internal/make_status.h"
#include "google/cloud/internal/rest_request.h"
#include "google/cloud/internal/rest_response.h"
#include "google/cloud/testing_util/chrono_output.h"
#include "google/cloud/testing_util/mock_http_payload.h"
#include "google/cloud/testing_util/mock_rest_client.h"
#include "google/cloud/testing_util/mock_rest_response.h"
#include "google/cloud/testing_util/status_matchers.h"
#include "absl/strings/str_cat.h"
#include <gmock/gmock.h>
#include <memory>

namespace google {
namespace cloud {
namespace oauth2_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::google::cloud::rest_internal::RestContext;
using ::google::cloud::rest_internal::RestRequest;
using ::google::cloud::rest_internal::RestResponse;
using ::google::cloud::testing_util::IsOk;
using ::google::cloud::testing_util::IsOkAndHolds;
using ::google::cloud::testing_util::MakeMockHttpPayloadSuccess;
using ::google::cloud::testing_util::MockRestClient;
using ::google::cloud::testing_util::MockRestResponse;
using ::google::cloud::testing_util::StatusIs;
using ::testing::_;
using ::testing::A;
using ::testing::ByMove;
using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::HasSubstr;
using ::testing::IsSupersetOf;
using ::testing::Pair;
using ::testing::Return;

class MockCredentials : public google::cloud::oauth2_internal::Credentials {
 public:
  MOCK_METHOD(StatusOr<AccessToken>, GetToken,
              (std::chrono::system_clock::time_point), (override));
  MOCK_METHOD(StatusOr<std::string>, universe_domain, (Options const&),
              (const, override));
};

using MockHttpClientFactory =
    ::testing::MockFunction<std::unique_ptr<rest_internal::RestClient>(
        Options const&)>;

TEST(ParseGenerateAccessTokenResponse, Success) {
  auto const response = std::string{R"""({
    "accessToken": "test-access-token",
    "expireTime": "2022-10-12T07:20:50.520Z"})"""};
  //  date --date=2022-10-12T07:20:50.52Z +%s
  auto const expiration = std::chrono::system_clock::from_time_t(1665559250) +
                          std::chrono::milliseconds(520);
  MockRestResponse mock;
  EXPECT_CALL(mock, StatusCode)
      .WillRepeatedly(Return(rest_internal::HttpStatusCode::kOk));
  EXPECT_CALL(std::move(mock), ExtractPayload)
      .WillOnce(Return(ByMove(MakeMockHttpPayloadSuccess(response))));

  auto ec = internal::ErrorContext();
  auto token = ParseGenerateAccessTokenResponse(mock, ec);
  ASSERT_STATUS_OK(token);
  EXPECT_EQ(token->token, "test-access-token");
  EXPECT_EQ(token->expiration, expiration);
}

TEST(ParseGenerateAccessTokenResponse, HttpError) {
  auto const response = std::string{R"""({
    "accessToken": "test-access-token",
    "expireTime": "2022-10-12T07:20:50.520Z"})"""};
  MockRestResponse mock;
  EXPECT_CALL(mock, StatusCode)
      .WillRepeatedly(Return(rest_internal::HttpStatusCode::kNotFound));
  EXPECT_CALL(std::move(mock), ExtractPayload)
      .WillOnce(Return(ByMove(MakeMockHttpPayloadSuccess(response))));

  auto ec = internal::ErrorContext();
  auto token = ParseGenerateAccessTokenResponse(mock, ec);
  EXPECT_THAT(token, StatusIs(StatusCode::kNotFound));
}

TEST(ParseGenerateAccessTokenResponse, NotJson) {
  auto const response = std::string{R"""(not-json)"""};
  MockRestResponse mock;
  EXPECT_CALL(mock, StatusCode)
      .WillRepeatedly(Return(rest_internal::HttpStatusCode::kOk));
  EXPECT_CALL(std::move(mock), ExtractPayload)
      .WillOnce(Return(ByMove(MakeMockHttpPayloadSuccess(response))));

  auto ec = internal::ErrorContext();
  auto token = ParseGenerateAccessTokenResponse(mock, ec);
  EXPECT_THAT(token,
              StatusIs(StatusCode::kInvalidArgument,
                       HasSubstr("cannot parse response as a JSON object")));
}

TEST(ParseGenerateAccessTokenResponse, NotJsonObject) {
  auto const response = std::string{R"""("JSON-but-not-object")"""};
  MockRestResponse mock;
  EXPECT_CALL(mock, StatusCode)
      .WillRepeatedly(Return(rest_internal::HttpStatusCode::kOk));
  EXPECT_CALL(std::move(mock), ExtractPayload)
      .WillOnce(Return(ByMove(MakeMockHttpPayloadSuccess(response))));

  auto ec = internal::ErrorContext();
  auto token = ParseGenerateAccessTokenResponse(mock, ec);
  EXPECT_THAT(token,
              StatusIs(StatusCode::kInvalidArgument,
                       HasSubstr("cannot parse response as a JSON object")));
}

TEST(ParseGenerateAccessTokenResponse, MissingAccessToken) {
  auto const response = std::string{R"""({
    "missing-accessToken": "test-access-token",
    "expireTime": "2022-10-12T07:20:50.520Z"})"""};
  MockRestResponse mock;
  EXPECT_CALL(mock, StatusCode)
      .WillRepeatedly(Return(rest_internal::HttpStatusCode::kOk));
  EXPECT_CALL(std::move(mock), ExtractPayload)
      .WillOnce(Return(ByMove(MakeMockHttpPayloadSuccess(response))));

  auto ec = internal::ErrorContext();
  auto token = ParseGenerateAccessTokenResponse(mock, ec);
  EXPECT_THAT(token, StatusIs(StatusCode::kInvalidArgument,
                              HasSubstr("cannot find `accessToken` field")));
}

TEST(ParseGenerateAccessTokenResponse, InvalidAccessToken) {
  auto const response = std::string{R"""({
    "accessToken": true,
    "expireTime": "2022-10-12T07:20:50.520Z"})"""};
  MockRestResponse mock;
  EXPECT_CALL(mock, StatusCode)
      .WillRepeatedly(Return(rest_internal::HttpStatusCode::kOk));
  EXPECT_CALL(std::move(mock), ExtractPayload)
      .WillOnce(Return(ByMove(MakeMockHttpPayloadSuccess(response))));

  auto ec = internal::ErrorContext();
  auto token = ParseGenerateAccessTokenResponse(mock, ec);
  EXPECT_THAT(token,
              StatusIs(StatusCode::kInvalidArgument,
                       HasSubstr("invalid type for `accessToken` field")));
}

TEST(ParseGenerateAccessTokenResponse, MissingExpireTime) {
  auto const response = std::string{R"""({
    "accessToken": "unused",
    "missing-expireTime": "2022-10-12T07:20:50.520Z"})"""};
  MockRestResponse mock;
  EXPECT_CALL(mock, StatusCode)
      .WillRepeatedly(Return(rest_internal::HttpStatusCode::kOk));
  EXPECT_CALL(std::move(mock), ExtractPayload)
      .WillOnce(Return(ByMove(MakeMockHttpPayloadSuccess(response))));

  auto ec = internal::ErrorContext();
  auto token = ParseGenerateAccessTokenResponse(mock, ec);
  EXPECT_THAT(token, StatusIs(StatusCode::kInvalidArgument,
                              HasSubstr("cannot find `expireTime` field")));
}

TEST(ParseGenerateAccessTokenResponse, InvalidExpireTime) {
  auto const response = std::string{R"""({
    "accessToken": "unused",
    "expireTime": true})"""};
  MockRestResponse mock;
  EXPECT_CALL(mock, StatusCode)
      .WillRepeatedly(Return(rest_internal::HttpStatusCode::kOk));
  EXPECT_CALL(std::move(mock), ExtractPayload)
      .WillOnce(Return(ByMove(MakeMockHttpPayloadSuccess(response))));

  auto ec = internal::ErrorContext();
  auto token = ParseGenerateAccessTokenResponse(mock, ec);
  EXPECT_THAT(token,
              StatusIs(StatusCode::kInvalidArgument,
                       HasSubstr("invalid type for `expireTime` field")));
}

TEST(ParseGenerateAccessTokenResponse, InvalidExpireTimeFormat) {
  auto const response = std::string{R"""({
    "accessToken": "unused",
    "expireTime": "not-a-RFC-3339-date"})"""};
  MockRestResponse mock;
  EXPECT_CALL(mock, StatusCode)
      .WillRepeatedly(Return(rest_internal::HttpStatusCode::kOk));
  EXPECT_CALL(std::move(mock), ExtractPayload)
      .WillOnce(Return(ByMove(MakeMockHttpPayloadSuccess(response))));

  auto ec = internal::ErrorContext();
  auto token = ParseGenerateAccessTokenResponse(mock, ec);
  EXPECT_THAT(token,
              StatusIs(StatusCode::kInvalidArgument,
                       HasSubstr("invalid format for `expireTime` field")));
}

TEST(MinimalIamCredentialsRestTest, GenerateAccessTokenSuccess) {
  std::string service_account = "foo@somewhere.com";
  std::chrono::seconds lifetime(3600);
  std::string scope = "my_scope";
  std::string delegate = "my_delegate";
  std::string response = R"""({
    "accessToken": "my_access_token",
    "expireTime": "2022-10-12T07:20:50.52Z"})""";
  MockHttpClientFactory mock_client_factory;
  EXPECT_CALL(mock_client_factory, Call).WillOnce([=](Options const&) {
    auto client = std::make_unique<MockRestClient>();
    EXPECT_CALL(*client,
                Post(_, _, A<std::vector<absl::Span<char const>> const&>()))
        .WillOnce([response, service_account](
                      RestContext&, RestRequest const& request,
                      std::vector<absl::Span<char const>> const& payload) {
          auto mock_response = std::make_unique<MockRestResponse>();
          EXPECT_CALL(*mock_response, StatusCode)
              .WillRepeatedly(Return(rest_internal::HttpStatusCode::kOk));
          EXPECT_CALL(std::move(*mock_response), ExtractPayload)
              .WillOnce([response] {
                return testing_util::MakeMockHttpPayloadSuccess(response);
              });

          EXPECT_THAT(
              request.path(),
              Eq(absl::StrCat("https://iamcredentials.googleapis.com/v1/",
                              "projects/-/serviceAccounts/", service_account,
                              ":generateAccessToken")));
          std::string str_payload(payload[0].begin(), payload[0].end());
          EXPECT_THAT(str_payload, HasSubstr("\"lifetime\":\"3600s\""));
          EXPECT_THAT(str_payload, HasSubstr("\"scope\":[\"my_scope\"]"));
          EXPECT_THAT(str_payload,
                      HasSubstr("\"delegates\":[\"my_delegate\"]"));
          return std::unique_ptr<RestResponse>(std::move(mock_response));
        });
    return std::unique_ptr<rest_internal::RestClient>(std::move(client));
  });
  auto mock_credentials = std::make_shared<MockCredentials>();
  EXPECT_CALL(*mock_credentials, GetToken).WillOnce([lifetime](auto tp) {
    return AccessToken{"test-token", tp + lifetime};
  });
  EXPECT_CALL(*mock_credentials, universe_domain)
      .WillOnce(::testing::Return(StatusOr<std::string>{"googleapis.com"}));

  auto stub =
      MinimalIamCredentialsRestStub(std::move(mock_credentials), Options{},
                                    mock_client_factory.AsStdFunction());
  GenerateAccessTokenRequest request;
  request.service_account = service_account;
  request.lifetime = lifetime;
  request.scopes.push_back(scope);
  request.delegates.push_back(delegate);
  auto access_token = stub.GenerateAccessToken(request);
  EXPECT_THAT(access_token, IsOk());
  EXPECT_THAT(access_token->token, Eq("my_access_token"));
}

TEST(MinimalIamCredentialsRestTest, GenerateAccessTokenWithUniverseDomain) {
  std::string universe_domain = "my-ud.net";
  std::string service_account = "foo@somewhere.com";
  std::chrono::seconds lifetime(3600);
  std::string response = R"""({
    "accessToken": "my_access_token",
    "expireTime": "2022-10-12T07:20:50.52Z"})""";
  MockHttpClientFactory mock_client_factory;
  EXPECT_CALL(mock_client_factory, Call).WillOnce([=](Options const&) {
    auto client = std::make_unique<MockRestClient>();
    EXPECT_CALL(*client,
                Post(_, _, A<std::vector<absl::Span<char const>> const&>()))
        .WillOnce([response, service_account, universe_domain](
                      RestContext&, RestRequest const& request,
                      std::vector<absl::Span<char const>> const&) {
          auto mock_response = std::make_unique<MockRestResponse>();
          EXPECT_CALL(*mock_response, StatusCode)
              .WillRepeatedly(Return(rest_internal::HttpStatusCode::kOk));
          EXPECT_CALL(std::move(*mock_response), ExtractPayload)
              .WillOnce([response] {
                return testing_util::MakeMockHttpPayloadSuccess(response);
              });

          EXPECT_THAT(
              request.path(),
              Eq(absl::StrCat("https://iamcredentials.", universe_domain,
                              "/v1/projects/-/serviceAccounts/",
                              service_account, ":generateAccessToken")));
          return std::unique_ptr<RestResponse>(std::move(mock_response));
        });
    return std::unique_ptr<rest_internal::RestClient>(std::move(client));
  });
  auto mock_credentials = std::make_shared<MockCredentials>();
  EXPECT_CALL(*mock_credentials, GetToken).WillOnce([](auto tp) {
    return AccessToken{"test-token", tp};
  });
  EXPECT_CALL(*mock_credentials, universe_domain)
      .WillOnce([&](Options const&) -> StatusOr<std::string> {
        return universe_domain;
      });

  auto stub =
      MinimalIamCredentialsRestStub(std::move(mock_credentials), Options{},
                                    mock_client_factory.AsStdFunction());
  GenerateAccessTokenRequest request;
  request.lifetime = lifetime;
  request.service_account = service_account;
  stub.GenerateAccessToken(request);
}

TEST(MinimalIamCredentialsRestTest, GenerateAccessTokenCredentialFailure) {
  auto mock_credentials = std::make_shared<MockCredentials>();
  EXPECT_CALL(*mock_credentials, GetToken).WillOnce([] {
    return Status(StatusCode::kPermissionDenied, "Permission Denied");
  });
  MockHttpClientFactory mock_client_factory;
  EXPECT_CALL(mock_client_factory, Call).Times(0);
  auto stub = MinimalIamCredentialsRestStub(
      std::move(mock_credentials), {}, mock_client_factory.AsStdFunction());
  GenerateAccessTokenRequest request;
  auto access_token = stub.GenerateAccessToken(request);
  EXPECT_THAT(access_token, StatusIs(StatusCode::kPermissionDenied));
}

TEST(MinimalIamCredentialsRestTest, GetUniverseDomainFromCredentials) {
  auto constexpr kExpectedUniverseDomain = "my-ud.net";
  auto mock_credentials = std::make_shared<MockCredentials>();
  EXPECT_CALL(*mock_credentials, universe_domain)
      .WillOnce([&](Options const&) -> StatusOr<std::string> {
        return std::string{kExpectedUniverseDomain};
      });
  auto stub =
      MinimalIamCredentialsRestStub(std::move(mock_credentials), Options{}, {});
  EXPECT_THAT(stub.universe_domain(Options{}),
              IsOkAndHolds(kExpectedUniverseDomain));
}

TEST(MinimalIamCredentialsRestTest, AllowedLocationsAuthorizationFailure) {
  auto mock_credentials = std::make_shared<MockCredentials>();
  EXPECT_CALL(*mock_credentials, GetToken).WillOnce([] {
    return Status(StatusCode::kPermissionDenied, "Permission Denied");
  });
  EXPECT_CALL(*mock_credentials, universe_domain)
      .WillOnce(::testing::Return(StatusOr<std::string>{"googleapis.com"}));

  MockHttpClientFactory mock_client_factory;
  EXPECT_CALL(mock_client_factory, Call).Times(0);
  auto stub = MinimalIamCredentialsRestStub(
      std::move(mock_credentials), {}, mock_client_factory.AsStdFunction());
  ServiceAccountAllowedLocationsRequest request;
  request.service_account_email = "foo@somewhere.com";
  auto access_token = stub.AllowedLocations(request);
  EXPECT_THAT(access_token, StatusIs(StatusCode::kPermissionDenied));
}

// TODO(#16177): Update these tests to compile with MSVC.
#ifndef _WIN32

TEST(MinimalIamCredentialsRestTest, AllowedLocationsMalformedResponseFailure) {
  std::string service_account = "foo@somewhere.com";
  std::chrono::seconds lifetime(3600);
  std::string response = R"""({})""";
  MockHttpClientFactory mock_client_factory;
  EXPECT_CALL(mock_client_factory, Call).WillOnce([=](Options const&) {
    auto client = std::make_unique<MockRestClient>();
    EXPECT_CALL(*client, Get)
        .WillOnce([&](RestContext&, RestRequest const& request) {
          auto mock_response = std::make_unique<MockRestResponse>();
          EXPECT_CALL(*mock_response, StatusCode)
              .WillRepeatedly(Return(rest_internal::HttpStatusCode::kOk));
          EXPECT_CALL(std::move(*mock_response), ExtractPayload)
              .WillOnce([response] {
                return testing_util::MakeMockHttpPayloadSuccess(response);
              });
          EXPECT_THAT(
              request.path(),
              Eq(absl::StrCat("https://iamcredentials.googleapis.com/v1/",
                              "projects/-/serviceAccounts/", service_account,
                              "/allowedLocations")));
          return std::unique_ptr<RestResponse>(std::move(mock_response));
        });
    return std::unique_ptr<rest_internal::RestClient>(std::move(client));
  });
  auto mock_credentials = std::make_shared<MockCredentials>();
  EXPECT_CALL(*mock_credentials, GetToken).WillOnce([lifetime](auto tp) {
    return AccessToken{"test-token", tp + lifetime};
  });
  EXPECT_CALL(*mock_credentials, universe_domain)
      .WillOnce(::testing::Return(StatusOr<std::string>{"googleapis.com"}));

  auto stub =
      MinimalIamCredentialsRestStub(std::move(mock_credentials), Options{},
                                    mock_client_factory.AsStdFunction());
  ServiceAccountAllowedLocationsRequest request;
  request.service_account_email = service_account;
  auto allowed_locations = stub.AllowedLocations(request);
  EXPECT_THAT(allowed_locations, StatusIs(StatusCode::kInvalidArgument));
  EXPECT_THAT(
      allowed_locations.status().error_info().metadata(),
      IsSupersetOf(
          {Pair("gcloud-cpp.root.class", "MinimalIamCredentialsRestStub"),
           Pair("gcloud-cpp.root.function", "AllowedLocationsHelper"),
           Pair("path",
                "https://iamcredentials.googleapis.com/v1/projects/-/"
                "serviceAccounts/foo@somewhere.com/allowedLocations")}));
}

TEST(MinimalIamCredentialsRestTest, ServiceAccountAllowedLocations) {
  std::string service_account = "foo@somewhere.com";
  std::chrono::seconds lifetime(3600);
  std::string response = R"""({
    "locations": [
      "us-central1", "us-east1", "europe-west1", "asia-east1"
    ],
    "encodedLocations" : "0xA30"})""";
  MockHttpClientFactory mock_client_factory;
  EXPECT_CALL(mock_client_factory, Call).WillOnce([=](Options const&) {
    auto client = std::make_unique<MockRestClient>();
    EXPECT_CALL(*client, Get)
        .WillOnce([&](RestContext&, RestRequest const& request) {
          auto mock_response = std::make_unique<MockRestResponse>();
          EXPECT_CALL(*mock_response, StatusCode)
              .WillRepeatedly(Return(rest_internal::HttpStatusCode::kOk));
          EXPECT_CALL(std::move(*mock_response), ExtractPayload)
              .WillOnce([response] {
                return testing_util::MakeMockHttpPayloadSuccess(response);
              });
          EXPECT_THAT(
              request.path(),
              Eq(absl::StrCat("https://iamcredentials.googleapis.com/v1/",
                              "projects/-/serviceAccounts/", service_account,
                              "/allowedLocations")));
          return std::unique_ptr<RestResponse>(std::move(mock_response));
        });
    return std::unique_ptr<rest_internal::RestClient>(std::move(client));
  });
  auto mock_credentials = std::make_shared<MockCredentials>();
  EXPECT_CALL(*mock_credentials, GetToken).WillOnce([lifetime](auto tp) {
    return AccessToken{"test-token", tp + lifetime};
  });
  EXPECT_CALL(*mock_credentials, universe_domain)
      .WillOnce(::testing::Return(StatusOr<std::string>{"googleapis.com"}));

  auto stub =
      MinimalIamCredentialsRestStub(std::move(mock_credentials), Options{},
                                    mock_client_factory.AsStdFunction());
  ServiceAccountAllowedLocationsRequest request;
  request.service_account_email = service_account;
  auto allowed_locations = stub.AllowedLocations(request);
  EXPECT_THAT(
      allowed_locations->locations,
      ElementsAre("us-central1", "us-east1", "europe-west1", "asia-east1"));
  EXPECT_THAT(allowed_locations->encoded_locations, Eq("0xA30"));
}

TEST(MinimalIamCredentialsRestTest, WorkloadIdentityAllowedLocations) {
  std::string project_id = "my-project";
  std::string pool_id = "my-pool";
  std::chrono::seconds lifetime(3600);
  std::string response = R"""({
    "locations": [
      "us-central1", "us-east1", "europe-west1", "asia-east1"
    ],
    "encodedLocations" : "0xA30"})""";
  MockHttpClientFactory mock_client_factory;
  EXPECT_CALL(mock_client_factory, Call).WillOnce([=](Options const&) {
    auto client = std::make_unique<MockRestClient>();
    EXPECT_CALL(*client, Get)
        .WillOnce([&](RestContext&, RestRequest const& request) {
          auto mock_response = std::make_unique<MockRestResponse>();
          EXPECT_CALL(*mock_response, StatusCode)
              .WillRepeatedly(Return(rest_internal::HttpStatusCode::kOk));
          EXPECT_CALL(std::move(*mock_response), ExtractPayload)
              .WillOnce([response] {
                return testing_util::MakeMockHttpPayloadSuccess(response);
              });
          EXPECT_THAT(
              request.path(),
              Eq(absl::StrCat("https://iamcredentials.googleapis.com/v1/",
                              "projects/", project_id,
                              "/locations/global/workloadIdentityPools/",
                              pool_id, "/allowedLocations")));
          return std::unique_ptr<RestResponse>(std::move(mock_response));
        });
    return std::unique_ptr<rest_internal::RestClient>(std::move(client));
  });
  auto mock_credentials = std::make_shared<MockCredentials>();
  EXPECT_CALL(*mock_credentials, GetToken).WillOnce([lifetime](auto tp) {
    return AccessToken{"test-token", tp + lifetime};
  });
  EXPECT_CALL(*mock_credentials, universe_domain)
      .WillOnce(::testing::Return(StatusOr<std::string>{"googleapis.com"}));

  auto stub =
      MinimalIamCredentialsRestStub(std::move(mock_credentials), Options{},
                                    mock_client_factory.AsStdFunction());
  WorkloadIdentityAllowedLocationsRequest request;
  request.project_id = project_id;
  request.pool_id = pool_id;
  auto allowed_locations = stub.AllowedLocations(request);
  EXPECT_THAT(
      allowed_locations->locations,
      ElementsAre("us-central1", "us-east1", "europe-west1", "asia-east1"));
  EXPECT_THAT(allowed_locations->encoded_locations, Eq("0xA30"));
}

TEST(MinimalIamCredentialsRestTest, WorkforceIdentityAllowedLocations) {
  std::string pool_id = "my-pool";
  std::chrono::seconds lifetime(3600);
  std::string response = R"""({
    "locations": [
      "us-central1", "us-east1", "europe-west1", "asia-east1"
    ],
    "encodedLocations" : "0xA30"})""";
  MockHttpClientFactory mock_client_factory;
  EXPECT_CALL(mock_client_factory, Call).WillOnce([=](Options const&) {
    auto client = std::make_unique<MockRestClient>();
    EXPECT_CALL(*client, Get)
        .WillOnce([&](RestContext&, RestRequest const& request) {
          auto mock_response = std::make_unique<MockRestResponse>();
          EXPECT_CALL(*mock_response, StatusCode)
              .WillRepeatedly(Return(rest_internal::HttpStatusCode::kOk));
          EXPECT_CALL(std::move(*mock_response), ExtractPayload)
              .WillOnce([response] {
                return testing_util::MakeMockHttpPayloadSuccess(response);
              });
          EXPECT_THAT(
              request.path(),
              Eq(absl::StrCat("https://iamcredentials.googleapis.com/v1/",
                              "locations/global/workforcePools/", pool_id,
                              "/allowedLocations")));
          return std::unique_ptr<RestResponse>(std::move(mock_response));
        });
    return std::unique_ptr<rest_internal::RestClient>(std::move(client));
  });
  auto mock_credentials = std::make_shared<MockCredentials>();
  EXPECT_CALL(*mock_credentials, GetToken).WillOnce([lifetime](auto tp) {
    return AccessToken{"test-token", tp + lifetime};
  });
  EXPECT_CALL(*mock_credentials, universe_domain)
      .WillOnce(::testing::Return(StatusOr<std::string>{"googleapis.com"}));

  auto stub =
      MinimalIamCredentialsRestStub(std::move(mock_credentials), Options{},
                                    mock_client_factory.AsStdFunction());
  WorkforceIdentityAllowedLocationsRequest request;
  request.pool_id = pool_id;
  auto allowed_locations = stub.AllowedLocations(request);
  EXPECT_THAT(
      allowed_locations->locations,
      ElementsAre("us-central1", "us-east1", "europe-west1", "asia-east1"));
  EXPECT_THAT(allowed_locations->encoded_locations, Eq("0xA30"));
}

#endif

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace oauth2_internal
}  // namespace cloud
}  // namespace google
