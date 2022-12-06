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
#include "google/cloud/internal/absl_str_cat_quiet.h"
#include "google/cloud/internal/http_payload.h"
#include "google/cloud/internal/rest_request.h"
#include "google/cloud/internal/rest_response.h"
#include "google/cloud/testing_util/chrono_output.h"
#include "google/cloud/testing_util/mock_http_payload.h"
#include "google/cloud/testing_util/mock_rest_client.h"
#include "google/cloud/testing_util/mock_rest_response.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>
#include <memory>

namespace google {
namespace cloud {
namespace oauth2_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::google::cloud::rest_internal::RestRequest;
using ::google::cloud::rest_internal::RestResponse;
using ::google::cloud::testing_util::IsOk;
using ::google::cloud::testing_util::MakeMockHttpPayloadSuccess;
using ::google::cloud::testing_util::MockRestClient;
using ::google::cloud::testing_util::MockRestResponse;
using ::google::cloud::testing_util::StatusIs;
using ::testing::_;
using ::testing::A;
using ::testing::ByMove;
using ::testing::Eq;
using ::testing::HasSubstr;
using ::testing::Return;

class MockCredentials : public google::cloud::oauth2_internal::Credentials {
 public:
  MOCK_METHOD(StatusOr<internal::AccessToken>, GetToken,
              (std::chrono::system_clock::time_point), (override));
};

class MinimalIamCredentialsRestTest : public ::testing::Test {
 protected:
  void SetUp() override {
    mock_rest_client_ = std::make_shared<MockRestClient>();
    mock_credentials_ = std::make_shared<MockCredentials>();
  }

  std::shared_ptr<MockRestClient> mock_rest_client_;
  std::shared_ptr<MockCredentials> mock_credentials_;
};

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

TEST_F(MinimalIamCredentialsRestTest, GenerateAccessTokenSuccess) {
  std::string service_account = "foo@somewhere.com";
  std::chrono::seconds lifetime(3600);
  std::string scope = "my_scope";
  std::string delegate = "my_delegate";

  EXPECT_CALL(*mock_credentials_, GetToken).WillOnce([lifetime](auto tp) {
    return internal::AccessToken{"test-token", tp + lifetime};
  });

  std::string response = R"""({
  "accessToken": "my_access_token",
  "expireTime": "2022-10-12T07:20:50.52Z"
})""";

  auto* mock_response = new MockRestResponse();
  EXPECT_CALL(*mock_response, StatusCode)
      .WillRepeatedly(Return(rest_internal::HttpStatusCode::kOk));
  EXPECT_CALL(std::move(*mock_response), ExtractPayload).WillOnce([&] {
    return testing_util::MakeMockHttpPayloadSuccess(response);
  });

  EXPECT_CALL(*mock_rest_client_,
              Post(_, A<std::vector<absl::Span<char const>> const&>()))
      .WillOnce([&](RestRequest const& request,
                    std::vector<absl::Span<char const>> const& payload) {
        EXPECT_THAT(request.path(),
                    Eq(absl::StrCat("projects/-/serviceAccounts/",
                                    service_account, ":generateAccessToken")));
        std::string str_payload(payload[0].begin(), payload[0].end());
        EXPECT_THAT(str_payload, testing::HasSubstr("\"lifetime\":\"3600s\""));
        EXPECT_THAT(str_payload,
                    testing::HasSubstr("\"scope\":[\"my_scope\"]"));
        EXPECT_THAT(str_payload,
                    testing::HasSubstr("\"delegates\":[\"my_delegate\"]"));
        return std::unique_ptr<RestResponse>(std::move(mock_response));
      });

  auto stub = MinimalIamCredentialsRestStub(std::move(mock_credentials_), {},
                                            std::move(mock_rest_client_));
  GenerateAccessTokenRequest request;
  request.service_account = service_account;
  request.lifetime = lifetime;
  request.scopes.push_back(scope);
  request.delegates.push_back(delegate);

  auto access_token = stub.GenerateAccessToken(request);
  EXPECT_THAT(access_token, IsOk());
  EXPECT_THAT(access_token->token, Eq("my_access_token"));
}

TEST_F(MinimalIamCredentialsRestTest, GenerateAccessTokenCredentialFailure) {
  EXPECT_CALL(*mock_credentials_, GetToken).WillOnce([] {
    return Status(StatusCode::kPermissionDenied, "Permission Denied");
  });
  auto stub = MinimalIamCredentialsRestStub(std::move(mock_credentials_), {},
                                            std::move(mock_rest_client_));
  GenerateAccessTokenRequest request;
  auto access_token = stub.GenerateAccessToken(request);
  EXPECT_THAT(access_token, StatusIs(StatusCode::kPermissionDenied));
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace oauth2_internal
}  // namespace cloud
}  // namespace google
