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

using ::google::cloud::rest_internal::HttpPayload;
using ::google::cloud::rest_internal::RestRequest;
using ::google::cloud::rest_internal::RestResponse;
using ::google::cloud::testing_util::IsOk;
using ::google::cloud::testing_util::MockHttpPayload;
using ::google::cloud::testing_util::MockRestClient;
using ::google::cloud::testing_util::MockRestResponse;
using ::google::cloud::testing_util::StatusIs;
using ::testing::_;
using ::testing::A;
using ::testing::Eq;
using ::testing::Return;

class MockCredentials : public google::cloud::oauth2_internal::Credentials {
 public:
  MOCK_METHOD((StatusOr<std::pair<std::string, std::string>>),
              AuthorizationHeader, (), (override));
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

TEST_F(MinimalIamCredentialsRestTest, GenerateAccessTokenSuccess) {
  std::string service_account = "foo@somewhere.com";
  std::chrono::seconds lifetime(3600);
  std::string scope = "my_scope";
  std::string delegate = "my_delegate";

  EXPECT_CALL(*mock_credentials_, AuthorizationHeader).WillOnce([] {
    return std::make_pair(std::string("Authorization"), std::string("Foo"));
  });

  std::string response = R"""({
  "accessToken": "my_access_token",
  "expireTime": "2022-10-12T07:20:50.52Z"
})""";

  auto* mock_response = new MockRestResponse();
  EXPECT_CALL(*mock_response, StatusCode)
      .WillRepeatedly(Return(rest_internal::HttpStatusCode::kOk));
  EXPECT_CALL(std::move(*mock_response), ExtractPayload).WillOnce([&] {
    auto mock_http_payload = absl::make_unique<MockHttpPayload>();
    EXPECT_CALL(*mock_http_payload, Read)
        .WillOnce([&](absl::Span<char> buffer) {
          std::copy(response.begin(), response.end(), buffer.begin());
          return response.size();
        })
        .WillOnce([](absl::Span<char>) { return 0; });
    return std::unique_ptr<HttpPayload>(std::move(mock_http_payload));
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

TEST_F(MinimalIamCredentialsRestTest, GenerateAccessTokenNonRfc3339Time) {
  std::string service_account = "foo@somewhere.com";
  std::chrono::seconds lifetime(3600);
  std::string scope = "my_scope";
  std::string delegate = "my_delegate";

  EXPECT_CALL(*mock_credentials_, AuthorizationHeader).WillOnce([] {
    return std::make_pair(std::string("Authorization"), std::string("Foo"));
  });

  std::string response = R"""({
  "accessToken": "my_access_token",
  "expireTime": "Tomorrow"
})""";

  auto* mock_response = new MockRestResponse();
  EXPECT_CALL(*mock_response, StatusCode)
      .WillRepeatedly(Return(rest_internal::HttpStatusCode::kOk));
  EXPECT_CALL(std::move(*mock_response), ExtractPayload).WillOnce([&] {
    auto mock_http_payload = absl::make_unique<MockHttpPayload>();
    EXPECT_CALL(*mock_http_payload, Read)
        .WillOnce([&](absl::Span<char> buffer) {
          std::copy(response.begin(), response.end(), buffer.begin());
          return response.size();
        })
        .WillOnce([](absl::Span<char>) { return 0; });
    return std::unique_ptr<HttpPayload>(std::move(mock_http_payload));
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
  EXPECT_THAT(access_token, StatusIs(StatusCode::kInvalidArgument));
}

TEST_F(MinimalIamCredentialsRestTest, GenerateAccessTokenInvalidResponse) {
  std::string service_account = "foo@somewhere.com";
  std::chrono::seconds lifetime(3600);
  std::string scope = "my_scope";
  std::string delegate = "my_delegate";

  EXPECT_CALL(*mock_credentials_, AuthorizationHeader).WillOnce([] {
    return std::make_pair(std::string("Authorization"), std::string("Foo"));
  });

  std::string response = R"""({
  "accessToken": "my_access_token"
})""";

  auto* mock_response = new MockRestResponse();
  EXPECT_CALL(*mock_response, StatusCode)
      .WillRepeatedly(Return(rest_internal::HttpStatusCode::kOk));
  EXPECT_CALL(std::move(*mock_response), ExtractPayload).WillOnce([&] {
    auto mock_http_payload = absl::make_unique<MockHttpPayload>();
    EXPECT_CALL(*mock_http_payload, Read)
        .WillOnce([&](absl::Span<char> buffer) {
          std::copy(response.begin(), response.end(), buffer.begin());
          return response.size();
        })
        .WillOnce([](absl::Span<char>) { return 0; });
    return std::unique_ptr<HttpPayload>(std::move(mock_http_payload));
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
  EXPECT_THAT(access_token, StatusIs(StatusCode::kUnknown));
}

TEST_F(MinimalIamCredentialsRestTest, GenerateAccessTokenPostFailure) {
  std::string service_account = "foo@somewhere.com";
  std::chrono::seconds lifetime(3600);
  std::string scope = "my_scope";
  std::string delegate = "my_delegate";

  EXPECT_CALL(*mock_credentials_, AuthorizationHeader).WillOnce([] {
    return std::make_pair(std::string("Authorization"), std::string("Foo"));
  });

  auto* mock_response = new MockRestResponse();
  EXPECT_CALL(*mock_response, StatusCode)
      .WillRepeatedly(Return(rest_internal::HttpStatusCode::kNotFound));
  EXPECT_CALL(std::move(*mock_response), ExtractPayload).WillOnce([&] {
    auto mock_http_payload = absl::make_unique<MockHttpPayload>();
    EXPECT_CALL(*mock_http_payload, Read).WillOnce([](absl::Span<char>) {
      return 0;
    });
    return std::unique_ptr<HttpPayload>(std::move(mock_http_payload));
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
  EXPECT_THAT(access_token, StatusIs(StatusCode::kNotFound));
}

TEST_F(MinimalIamCredentialsRestTest, GenerateAccessTokenCredentialFailure) {
  EXPECT_CALL(*mock_credentials_, AuthorizationHeader).WillOnce([] {
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
