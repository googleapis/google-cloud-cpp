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

#include "google/cloud/internal/external_account_token_source_aws.h"
#include "google/cloud/testing_util/mock_http_payload.h"
#include "google/cloud/testing_util/mock_rest_client.h"
#include "google/cloud/testing_util/mock_rest_response.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>
#include <algorithm>

namespace google {
namespace cloud {
namespace oauth2_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::google::cloud::rest_internal::HttpStatusCode;
using ::google::cloud::rest_internal::RestRequest;
using ::google::cloud::rest_internal::RestResponse;
using ::google::cloud::testing_util::MakeMockHttpPayloadSuccess;
using ::google::cloud::testing_util::MockRestClient;
using ::google::cloud::testing_util::MockRestResponse;
using ::google::cloud::testing_util::StatusIs;
using ::testing::ByMove;
using ::testing::HasSubstr;
using ::testing::IsEmpty;
using ::testing::IsSupersetOf;
using ::testing::Pair;
using ::testing::Property;
using ::testing::ResultOf;
using ::testing::Return;

using MockClientFactory =
    ::testing::MockFunction<std::unique_ptr<rest_internal::RestClient>(
        Options const&)>;

internal::ErrorContext MakeTestErrorContext() {
  return internal::ErrorContext{
      {{"filename", "my-credentials.json"}, {"key", "value"}}};
}

std::unique_ptr<RestResponse> MakeMockResponseSuccess(std::string contents) {
  auto response = absl::make_unique<MockRestResponse>();
  EXPECT_CALL(*response, StatusCode)
      .WillRepeatedly(Return(HttpStatusCode::kOk));
  EXPECT_CALL(std::move(*response), ExtractPayload)
      .WillOnce(
          Return(ByMove(MakeMockHttpPayloadSuccess(std::move(contents)))));
  return response;
}

TEST(ExternalAccountTokenSource, WorkingImdsv2SessionTokenUrl) {
  auto const test_url = std::string{"http://169.254.169.254/latest/api/token"};
  auto const token = std::string{"a-test-only-token"};

  auto expected_options =
      ResultOf([](Options const& o) { return o.get<QuotaUserOption>(); },
               "test-quota-user");

  MockClientFactory client_factory;
  EXPECT_CALL(client_factory, Call(expected_options))
      .WillOnce([test_url, token]() {
        auto mock = absl::make_unique<MockRestClient>();
        auto expected_request = Property(&RestRequest::path, test_url);
        EXPECT_CALL(*mock, Get(expected_request))
            .WillOnce(Return(ByMove(MakeMockResponseSuccess(token))));
        return mock;
      });

  auto const creds = nlohmann::json{
      {"environment_id", "aws1"},
      {"region_url", "test-region-url"},
      {"url", "test-url"},
      {"regional_cred_verification_url", "test-verification-url"},
      {"imdsv2_session_token_url", test_url},
  };
  auto const source =
      MakeExternalAccountTokenSourceAws(creds, MakeTestErrorContext());
  ASSERT_STATUS_OK(source);
  auto const actual =
      (*source)(client_factory.AsStdFunction(),
                Options{}.set<QuotaUserOption>("test-quota-user"));
  ASSERT_STATUS_OK(actual);
  EXPECT_EQ(*actual, internal::SubjectToken{token});
}

TEST(ExternalAccountTokenSource, ParseSuccess) {
  auto const creds = nlohmann::json{
      {"environment_id", "aws1"},
      {"region_url", "test-region-url"},
      {"url", "test-url"},
      {"regional_cred_verification_url", "test-verification-url"},
      {"imdsv2_session_token_url", "test-imdsv2"},
  };
  auto const info =
      ParseExternalAccountTokenSourceAws(creds, MakeTestErrorContext());
  ASSERT_STATUS_OK(info);
  EXPECT_EQ(info->environment_id, "aws1");
  EXPECT_EQ(info->region_url, "test-region-url");
  EXPECT_EQ(info->url, "test-url");
  EXPECT_EQ(info->regional_cred_verification_url, "test-verification-url");
  EXPECT_EQ(info->imdsv2_session_token_url, "test-imdsv2");
}

TEST(ExternalAccountTokenSource, ParseSuccessNoUrl) {
  auto const creds = nlohmann::json{
      {"environment_id", "aws1"},
      {"region_url", "test-region-url"},
      // {"url", "test-url"},
      {"regional_cred_verification_url", "test-verification-url"},
      {"imdsv2_session_token_url", "test-imdsv2"},
  };
  auto const info =
      ParseExternalAccountTokenSourceAws(creds, MakeTestErrorContext());
  ASSERT_STATUS_OK(info);
  EXPECT_EQ(info->environment_id, "aws1");
  EXPECT_EQ(info->region_url, "test-region-url");
  EXPECT_EQ(info->url,
            "http://169.254.169.254/latest/meta-data/iam/security-credentials");
  EXPECT_EQ(info->regional_cred_verification_url, "test-verification-url");
  EXPECT_EQ(info->imdsv2_session_token_url, "test-imdsv2");
}

TEST(ExternalAccountTokenSource, ParseSuccessNoImdsv2) {
  auto const creds = nlohmann::json{
      {"environment_id", "aws1"},
      {"region_url", "test-region-url"},
      {"url", "test-url"},
      {"regional_cred_verification_url", "test-verification-url"},
      // {"imdsv2_session_token_url", "test-imdsv2"},
  };
  auto const info =
      ParseExternalAccountTokenSourceAws(creds, MakeTestErrorContext());
  ASSERT_STATUS_OK(info);
  EXPECT_EQ(info->environment_id, "aws1");
  EXPECT_EQ(info->region_url, "test-region-url");
  EXPECT_EQ(info->url, "test-url");
  EXPECT_EQ(info->regional_cred_verification_url, "test-verification-url");
  EXPECT_THAT(info->imdsv2_session_token_url, IsEmpty());
}

TEST(ExternalAccountTokenSource, MissingEnvironmentId) {
  auto const creds = nlohmann::json{
      // {"environment_id", "aws1"},
      {"region_url", "test-region-url"},
      {"regional_cred_verification_url", "test-verification-url"},
  };
  auto const info =
      ParseExternalAccountTokenSourceAws(creds, MakeTestErrorContext());
  EXPECT_THAT(info, StatusIs(StatusCode::kInvalidArgument,
                             HasSubstr("cannot find `environment_id` field")));
  EXPECT_THAT(info.status().error_info().metadata(),
              IsSupersetOf({Pair("filename", "my-credentials.json"),
                            Pair("key", "value")}));
}

TEST(ExternalAccountTokenSource, InvalidEnvironmentId) {
  auto const creds = nlohmann::json{
      {"environment_id", true},
      {"region_url", "test-region-url"},
      {"regional_cred_verification_url", "test-verification-url"},
  };
  auto const info =
      ParseExternalAccountTokenSourceAws(creds, MakeTestErrorContext());
  EXPECT_THAT(info,
              StatusIs(StatusCode::kInvalidArgument,
                       HasSubstr("invalid type for `environment_id` field")));
  EXPECT_THAT(info.status().error_info().metadata(),
              IsSupersetOf({Pair("filename", "my-credentials.json"),
                            Pair("key", "value")}));
}

TEST(ExternalAccountTokenSource, UnknownEnvironmentId) {
  auto const creds = nlohmann::json{
      {"environment_id", "not-aws"},
      {"region_url", "test-region-url"},
      {"regional_cred_verification_url", "test-verification-url"},
  };
  auto const info =
      ParseExternalAccountTokenSourceAws(creds, MakeTestErrorContext());
  EXPECT_THAT(
      info, StatusIs(StatusCode::kInvalidArgument,
                     HasSubstr("`environment_id` does not start with `aws`")));
  EXPECT_THAT(info.status().error_info().metadata(),
              IsSupersetOf({Pair("filename", "my-credentials.json"),
                            Pair("key", "value")}));
}

TEST(ExternalAccountTokenSource, UnsupportedEnvironmentId) {
  auto const creds = nlohmann::json{
      {"environment_id", "aws2"},
      {"region_url", "test-region-url"},
      {"regional_cred_verification_url", "test-verification-url"},
  };
  auto const info =
      ParseExternalAccountTokenSourceAws(creds, MakeTestErrorContext());
  EXPECT_THAT(info,
              StatusIs(StatusCode::kInvalidArgument,
                       HasSubstr("only `environment_id=aws1` is supported")));
  EXPECT_THAT(info.status().error_info().metadata(),
              IsSupersetOf({Pair("filename", "my-credentials.json"),
                            Pair("key", "value")}));
}

TEST(ExternalAccountTokenSource, MissingRegionUrl) {
  auto const creds = nlohmann::json{
      {"environment_id", "aws1"},
      // {"region_url", "test-region-url"},
      {"regional_cred_verification_url", "test-verification-url"},
  };
  auto const info =
      ParseExternalAccountTokenSourceAws(creds, MakeTestErrorContext());
  EXPECT_THAT(info, StatusIs(StatusCode::kInvalidArgument,
                             HasSubstr("cannot find `region_url` field")));
  EXPECT_THAT(info.status().error_info().metadata(),
              IsSupersetOf({Pair("filename", "my-credentials.json"),
                            Pair("key", "value")}));
}

TEST(ExternalAccountTokenSource, InvalidRegionUrl) {
  auto const creds = nlohmann::json{
      {"environment_id", "aws1"},
      {"region_url", true},
      {"regional_cred_verification_url", "test-verification-url"},
  };
  auto const info =
      ParseExternalAccountTokenSourceAws(creds, MakeTestErrorContext());
  EXPECT_THAT(info, StatusIs(StatusCode::kInvalidArgument,
                             HasSubstr("invalid type for `region_url` field")));
  EXPECT_THAT(info.status().error_info().metadata(),
              IsSupersetOf({Pair("filename", "my-credentials.json"),
                            Pair("key", "value")}));
}

TEST(ExternalAccountTokenSource, InvalidUrl) {
  auto const creds = nlohmann::json{
      {"environment_id", "aws1"},
      {"region_url", "test-region-url"},
      {"url", true},
      {"regional_cred_verification_url", "test-verification-url"},
  };
  auto const info =
      ParseExternalAccountTokenSourceAws(creds, MakeTestErrorContext());
  EXPECT_THAT(info, StatusIs(StatusCode::kInvalidArgument,
                             HasSubstr("invalid type for `url` field")));
  EXPECT_THAT(info.status().error_info().metadata(),
              IsSupersetOf({Pair("filename", "my-credentials.json"),
                            Pair("key", "value")}));
}

TEST(ExternalAccountTokenSource, MissingRegionalCredentialVerificationUrl) {
  auto const creds = nlohmann::json{
      {"environment_id", "aws1"}, {"region_url", "test-region-url"},
      // {"regional_cred_verification_url", "test-verification-url"},
  };
  auto const info =
      ParseExternalAccountTokenSourceAws(creds, MakeTestErrorContext());
  EXPECT_THAT(
      info,
      StatusIs(
          StatusCode::kInvalidArgument,
          HasSubstr("cannot find `regional_cred_verification_url` field")));
  EXPECT_THAT(info.status().error_info().metadata(),
              IsSupersetOf({Pair("filename", "my-credentials.json"),
                            Pair("key", "value")}));
}

TEST(ExternalAccountTokenSource, InvalidRegionalCredentialVerificationUrl) {
  auto const creds = nlohmann::json{
      {"environment_id", "aws1"},
      {"region_url", "test-region-url"},
      {"regional_cred_verification_url", true},
  };
  auto const info =
      ParseExternalAccountTokenSourceAws(creds, MakeTestErrorContext());
  EXPECT_THAT(
      info,
      StatusIs(StatusCode::kInvalidArgument,
               HasSubstr(
                   "invalid type for `regional_cred_verification_url` field")));
  EXPECT_THAT(info.status().error_info().metadata(),
              IsSupersetOf({Pair("filename", "my-credentials.json"),
                            Pair("key", "value")}));
}

TEST(ExternalAccountTokenSource, InvalidImdsv2SessionTokenUrl) {
  auto const creds = nlohmann::json{
      {"environment_id", "aws1"},
      {"region_url", "test-region-url"},
      {"regional_cred_verification_url", "test-regional-cred-verification-url"},
      {"imdsv2_session_token_url", true},
  };
  auto const info =
      ParseExternalAccountTokenSourceAws(creds, MakeTestErrorContext());
  EXPECT_THAT(
      info,
      StatusIs(StatusCode::kInvalidArgument,
               HasSubstr("invalid type for `imdsv2_session_token_url` field")));
  EXPECT_THAT(info.status().error_info().metadata(),
              IsSupersetOf({Pair("filename", "my-credentials.json"),
                            Pair("key", "value")}));
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace oauth2_internal
}  // namespace cloud
}  // namespace google
