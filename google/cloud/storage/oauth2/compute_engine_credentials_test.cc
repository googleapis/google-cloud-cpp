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
#include "google/cloud/internal/setenv.h"
#include "google/cloud/storage/internal/compute_engine_util.h"
#include "google/cloud/storage/internal/nljson.h"
#include "google/cloud/storage/oauth2/credential_constants.h"
#include "google/cloud/storage/testing/mock_http_request.h"
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
using ::google::cloud::storage::testing::MockHttpRequest;
using ::google::cloud::storage::testing::MockHttpRequestBuilder;
using ::testing::_;
using ::testing::An;
using ::testing::HasSubstr;
using ::testing::Invoke;
using ::testing::Return;
using ::testing::StrEq;
using ::testing::UnorderedElementsAre;

class ComputeEngineCredentialsTest : public ::testing::Test {
 protected:
  void SetUp() override {
    MockHttpRequestBuilder::mock =
        std::make_shared<MockHttpRequestBuilder::Impl>();
  }
  void TearDown() override { MockHttpRequestBuilder::mock.reset(); }
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

  auto mock_req_builder = MockHttpRequestBuilder::mock;
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

}  // namespace
}  // namespace oauth2
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google
