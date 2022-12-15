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
#include "google/cloud/testing_util/scoped_environment.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>
#include <algorithm>

namespace google {
namespace cloud {
namespace oauth2_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::google::cloud::rest_internal::HttpStatusCode;
using ::google::cloud::rest_internal::RestClient;
using ::google::cloud::rest_internal::RestRequest;
using ::google::cloud::rest_internal::RestResponse;
using ::google::cloud::testing_util::MakeMockHttpPayloadSuccess;
using ::google::cloud::testing_util::MockRestClient;
using ::google::cloud::testing_util::MockRestResponse;
using ::google::cloud::testing_util::ScopedEnvironment;
using ::google::cloud::testing_util::StatusIs;
using ::testing::ByMove;
using ::testing::Contains;
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

Options MakeTestOptions() {
  return Options{}.set<QuotaUserOption>("test-quota-user");
}

auto make_expected_options = [] {
  return ResultOf([](Options const& o) { return o.get<QuotaUserOption>(); },
                  "test-quota-user");
};

auto constexpr kTestRegionUrl = "http://169.254.169.254/region";
auto constexpr kTestMetadataUrl = "http://169.254.169.254/metadata";
auto constexpr kTestVerificationUrl =
    "https://sts.{region}.aws.example.com"
    "?Action=GetCallerIdentity&Version=2011-06-15";
auto constexpr kTestImdsv2Url = "http://169.254.169.254/imdsv2";
auto constexpr kTestImdsv2SessionToken = "test-imdsv2-token";

auto constexpr kMetadataTokenTtlHeader = "x-aws-ec2-metadata-token-ttl-seconds";
auto constexpr kMetadataTokenHeader = "x-aws-ec2-metadata-token";

ExternalAccountTokenSourceAwsInfo MakeTestInfoImdsV1() {
  return ExternalAccountTokenSourceAwsInfo{
      /*environment_id=*/"aws1",
      /*region_url=*/kTestRegionUrl,
      /*url=*/kTestMetadataUrl,
      /*regional_cred_verification_url=*/kTestVerificationUrl,
      /*imdsv2_session_token_url=*/std::string{}};
}

ExternalAccountTokenSourceAwsInfo MakeTestInfoImdsV2() {
  auto info = MakeTestInfoImdsV1();
  info.imdsv2_session_token_url = kTestImdsv2Url;
  return info;
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

TEST(ExternalAccountTokenSource, FetchMetadataTokenV1) {
  auto const info = MakeTestInfoImdsV1();

  MockClientFactory client_factory;
  EXPECT_CALL(client_factory, Call).Times(0);

  auto const actual = FetchMetadataToken(info, client_factory.AsStdFunction(),
                                         Options{}, MakeTestErrorContext());
  ASSERT_STATUS_OK(actual);
  EXPECT_THAT(*actual, IsEmpty());
}

TEST(ExternalAccountTokenSource, FetchMetadataTokenV2) {
  auto const info = MakeTestInfoImdsV2();

  MockClientFactory client_factory;
  EXPECT_CALL(client_factory, Call(make_expected_options())).WillOnce([] {
    using ::testing::_;
    auto mock = absl::make_unique<MockRestClient>();
    auto expected_request = AllOf(
        Property(&RestRequest::path, kTestImdsv2Url),
        Property(&RestRequest::headers,
                 Contains(Pair(kMetadataTokenTtlHeader, Not(IsEmpty())))));
    EXPECT_CALL(*mock, Put(expected_request, _))
        .WillOnce(
            Return(ByMove(MakeMockResponseSuccess(kTestImdsv2SessionToken))));
    return mock;
  });

  auto const actual =
      FetchMetadataToken(info, client_factory.AsStdFunction(),
                         MakeTestOptions(), MakeTestErrorContext());
  ASSERT_STATUS_OK(actual);
  EXPECT_EQ(*actual, kTestImdsv2SessionToken);
}

TEST(ExternalAccountTokenSource, FetchRegionFromEnvRegion) {
  auto const info = MakeTestInfoImdsV1();
  auto const region = ScopedEnvironment("AWS_REGION", "expected-region");
  auto const default_region =
      ScopedEnvironment("AWS_DEFAULT_REGION", "default-region-unused");

  MockClientFactory client_factory;
  EXPECT_CALL(client_factory, Call).Times(0);

  auto const actual =
      FetchRegion(info, std::string{}, client_factory.AsStdFunction(),
                  Options{}, MakeTestErrorContext());
  ASSERT_STATUS_OK(actual);
  EXPECT_EQ(*actual, "expected-region");
}

TEST(ExternalAccountTokenSource, FetchRegionFromEnvDefaultRegion) {
  auto const info = MakeTestInfoImdsV1();
  auto const region = ScopedEnvironment("AWS_REGION", absl::nullopt);
  auto const default_region =
      ScopedEnvironment("AWS_DEFAULT_REGION", "default-region");

  MockClientFactory client_factory;
  EXPECT_CALL(client_factory, Call).Times(0);

  auto const actual =
      FetchRegion(info, std::string{}, client_factory.AsStdFunction(),
                  Options{}, MakeTestErrorContext());
  ASSERT_STATUS_OK(actual);
  EXPECT_EQ(*actual, "default-region");
}

TEST(ExternalAccountTokenSource, FetchRegionFromUrlV1) {
  auto const info = MakeTestInfoImdsV1();
  auto const region = ScopedEnvironment("AWS_REGION", absl::nullopt);
  auto const default_region =
      ScopedEnvironment("AWS_DEFAULT_REGION", absl::nullopt);

  MockClientFactory client_factory;
  EXPECT_CALL(client_factory, Call(make_expected_options()))
      .WillOnce([url = info.region_url]() {
        auto mock = absl::make_unique<MockRestClient>();
        auto expected_request = Property(&RestRequest::path, url);
        EXPECT_CALL(*mock, Get(expected_request))
            .WillOnce(Return(ByMove(MakeMockResponseSuccess("test-only-1d"))));
        return mock;
      });

  auto const actual =
      FetchRegion(info, std::string{}, client_factory.AsStdFunction(),
                  MakeTestOptions(), MakeTestErrorContext());
  ASSERT_STATUS_OK(actual);
  EXPECT_EQ(*actual, "test-only-1");
}

TEST(ExternalAccountTokenSource, FetchRegionFromUrlV2) {
  auto const info = MakeTestInfoImdsV2();
  auto const region = ScopedEnvironment("AWS_REGION", absl::nullopt);
  auto const default_region =
      ScopedEnvironment("AWS_DEFAULT_REGION", absl::nullopt);

  MockClientFactory client_factory;
  EXPECT_CALL(client_factory, Call(make_expected_options()))
      .WillOnce([url = info.region_url]() {
        auto mock = absl::make_unique<MockRestClient>();
        auto expected_request =
            AllOf(Property(&RestRequest::path, url),
                  // We expect the token returned by MakeImdsV2Client()
                  Property(&RestRequest::headers,
                           Contains(Pair(kMetadataTokenHeader,
                                         Contains(kTestImdsv2SessionToken)))));
        EXPECT_CALL(*mock, Get(expected_request))
            .WillOnce(Return(ByMove(MakeMockResponseSuccess("test-only-1d"))));
        return mock;
      });

  auto const actual =
      FetchRegion(info, kTestImdsv2SessionToken, client_factory.AsStdFunction(),
                  MakeTestOptions(), MakeTestErrorContext());
  ASSERT_STATUS_OK(actual);
  EXPECT_EQ(*actual, "test-only-1");
}

TEST(ExternalAccountTokenSource, FetchRegionFromUrlEmpty) {
  auto const info = MakeTestInfoImdsV1();
  auto const region = ScopedEnvironment("AWS_REGION", absl::nullopt);
  auto const default_region =
      ScopedEnvironment("AWS_DEFAULT_REGION", absl::nullopt);

  MockClientFactory client_factory;
  EXPECT_CALL(client_factory, Call(make_expected_options()))
      .WillOnce([url = info.region_url]() {
        auto mock = absl::make_unique<MockRestClient>();
        auto expected_request = Property(&RestRequest::path, url);
        EXPECT_CALL(*mock, Get(expected_request))
            .WillOnce(Return(ByMove(MakeMockResponseSuccess(""))));
        return mock;
      });

  auto const actual =
      FetchRegion(info, kTestImdsv2SessionToken, client_factory.AsStdFunction(),
                  MakeTestOptions(), MakeTestErrorContext());
  EXPECT_THAT(actual, StatusIs(StatusCode::kInvalidArgument,
                               HasSubstr("invalid (empty) region")));
  EXPECT_THAT(actual.status().error_info().metadata(),
              IsSupersetOf({Pair("filename", "my-credentials.json"),
                            Pair("key", "value")}));
}

TEST(ExternalAccountTokenSource, FetchSecretsFromEnv) {
  auto const info = MakeTestInfoImdsV1();
  auto const access_key_id =
      ScopedEnvironment("AWS_ACCESS_KEY_ID", "test-access-key-id");
  auto const secret_access_key =
      ScopedEnvironment("AWS_SECRET_ACCESS_KEY", "test-secret-access-key");
  auto const token =
      ScopedEnvironment("AWS_SESSION_TOKEN", "test-session-token");

  MockClientFactory client_factory;
  EXPECT_CALL(client_factory, Call).Times(0);

  auto const actual =
      FetchSecrets(info, std::string{}, client_factory.AsStdFunction(),
                   MakeTestOptions(), MakeTestErrorContext());
  ASSERT_STATUS_OK(actual);
  EXPECT_EQ(actual->access_key_id, "test-access-key-id");
  EXPECT_EQ(actual->secret_access_key, "test-secret-access-key");
  EXPECT_EQ(actual->session_token, "test-session-token");
}

TEST(ExternalAccountTokenSource, FetchSecretsFromEnvNoSessionToken) {
  auto const info = MakeTestInfoImdsV1();
  auto const access_key_id =
      ScopedEnvironment("AWS_ACCESS_KEY_ID", "test-access-key-id");
  auto const secret_access_key =
      ScopedEnvironment("AWS_SECRET_ACCESS_KEY", "test-secret-access-key");
  auto const token = ScopedEnvironment("AWS_SESSION_TOKEN", absl::nullopt);

  MockClientFactory client_factory;
  EXPECT_CALL(client_factory, Call).Times(0);

  auto const actual =
      FetchSecrets(info, std::string{}, client_factory.AsStdFunction(),
                   MakeTestOptions(), MakeTestErrorContext());
  ASSERT_STATUS_OK(actual);
  EXPECT_EQ(actual->access_key_id, "test-access-key-id");
  EXPECT_EQ(actual->secret_access_key, "test-secret-access-key");
  EXPECT_EQ(actual->session_token, "");
}

TEST(ExternalAccountTokenSource, FetchSecretsFromUrlV1) {
  auto const info = MakeTestInfoImdsV1();
  auto const access_key_id =
      ScopedEnvironment("AWS_ACCESS_KEY_ID", absl::nullopt);
  auto const secret_access_key =
      ScopedEnvironment("AWS_SECRET_ACCESS_KEY", absl::nullopt);
  auto const token = ScopedEnvironment("AWS_SESSION_TOKEN", absl::nullopt);

  auto make_client = [](std::string const& url, std::string const& contents) {
    auto mock = absl::make_unique<MockRestClient>();
    auto expected_request = Property(&RestRequest::path, url);
    EXPECT_CALL(*mock, Get(expected_request))
        .WillOnce(Return(ByMove(MakeMockResponseSuccess(contents))));
    return mock;
  };

  auto const response = nlohmann::json{
      {"Code", "Success"},
      {"Type", "AWS-HMAC"},
      {"AccessKeyId", "test-access-key-id"},
      {"SecretAccessKey", "test-secret-access-key"},
      {"Token", "test-token"},
  };

  MockClientFactory client_factory;
  EXPECT_CALL(client_factory, Call(make_expected_options()))
      .WillOnce(Return(ByMove(make_client(info.url, "test-role"))))
      .WillOnce(Return(
          ByMove(make_client(info.url + "/test-role", response.dump()))));

  auto const actual =
      FetchSecrets(info, std::string{}, client_factory.AsStdFunction(),
                   MakeTestOptions(), MakeTestErrorContext());
  ASSERT_STATUS_OK(actual);
  EXPECT_EQ(actual->access_key_id, "test-access-key-id");
  EXPECT_EQ(actual->secret_access_key, "test-secret-access-key");
  EXPECT_EQ(actual->session_token, "test-token");
}

TEST(ExternalAccountTokenSource, FetchSecretsFromUrlV2) {
  auto const info = MakeTestInfoImdsV2();
  auto const access_key_id =
      ScopedEnvironment("AWS_ACCESS_KEY_ID", absl::nullopt);
  auto const secret_access_key =
      ScopedEnvironment("AWS_SECRET_ACCESS_KEY", absl::nullopt);
  auto const token = ScopedEnvironment("AWS_SESSION_TOKEN", absl::nullopt);

  auto make_client = [](std::string const& url, std::string const& contents) {
    auto mock = absl::make_unique<MockRestClient>();
    auto expected_request =
        AllOf(Property(&RestRequest::path, url),
              Property(&RestRequest::headers,
                       Contains(Pair(kMetadataTokenHeader,
                                     Contains(kTestImdsv2SessionToken)))));
    EXPECT_CALL(*mock, Get(expected_request))
        .WillOnce(Return(ByMove(MakeMockResponseSuccess(contents))));
    return mock;
  };

  auto const response = nlohmann::json{
      {"Code", "Success"},
      {"Type", "AWS-HMAC"},
      {"AccessKeyId", "test-access-key-id"},
      {"SecretAccessKey", "test-secret-access-key"},
      {"Token", "test-token"},
  };

  MockClientFactory client_factory;
  EXPECT_CALL(client_factory, Call(make_expected_options()))
      .WillOnce(Return(ByMove(make_client(info.url, "test-role"))))
      .WillOnce(Return(
          ByMove(make_client(info.url + "/test-role", response.dump()))));

  auto const actual = FetchSecrets(info, kTestImdsv2SessionToken,
                                   client_factory.AsStdFunction(),
                                   MakeTestOptions(), MakeTestErrorContext());
  ASSERT_STATUS_OK(actual);
  EXPECT_EQ(actual->access_key_id, "test-access-key-id");
  EXPECT_EQ(actual->secret_access_key, "test-secret-access-key");
  EXPECT_EQ(actual->session_token, "test-token");
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace oauth2_internal
}  // namespace cloud
}  // namespace google
