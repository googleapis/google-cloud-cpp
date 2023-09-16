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
#include "google/cloud/internal/parse_rfc3339.h"
#include "google/cloud/location.h"
#include "google/cloud/project.h"
#include "google/cloud/testing_util/mock_http_payload.h"
#include "google/cloud/testing_util/mock_rest_client.h"
#include "google/cloud/testing_util/mock_rest_response.h"
#include "google/cloud/testing_util/scoped_environment.h"
#include "google/cloud/testing_util/status_matchers.h"
#include "absl/strings/str_split.h"
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
using ::testing::_;
using ::testing::ByMove;
using ::testing::Contains;
using ::testing::HasSubstr;
using ::testing::IsEmpty;
using ::testing::IsSupersetOf;
using ::testing::Not;
using ::testing::Pair;
using ::testing::Property;
using ::testing::ResultOf;
using ::testing::Return;
using ::testing::StartsWith;

using MockClientFactory =
    ::testing::MockFunction<std::unique_ptr<rest_internal::RestClient>(
        Options const&)>;

internal::ErrorContext MakeTestErrorContext() {
  return internal::ErrorContext{
      {{"filename", "my-credentials.json"}, {"key", "value"}}};
}

std::unique_ptr<RestResponse> MakeMockResponseSuccess(std::string contents) {
  auto response = std::make_unique<MockRestResponse>();
  EXPECT_CALL(*response, StatusCode)
      .WillRepeatedly(Return(HttpStatusCode::kOk));
  EXPECT_CALL(std::move(*response), ExtractPayload)
      .WillOnce(
          Return(ByMove(MakeMockHttpPayloadSuccess(std::move(contents)))));
  return response;
}

std::unique_ptr<RestResponse> MakeMockResponseNotFound() {
  auto response = std::make_unique<MockRestResponse>();
  EXPECT_CALL(*response, StatusCode)
      .WillRepeatedly(Return(HttpStatusCode::kNotFound));
  EXPECT_CALL(std::move(*response), ExtractPayload)
      .WillOnce(
          Return(ByMove(MakeMockHttpPayloadSuccess(std::string{"NOT FOUND"}))));
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

TEST(ExternalAccountTokenSource, SourceWorking) {
  // To simplify the test we provide all the parameters via environment
  // variables and avoid using imdsv2.
  auto const region = ScopedEnvironment("AWS_REGION", "expected-region");
  auto const access_key_id =
      ScopedEnvironment("AWS_ACCESS_KEY_ID", "test-access-key-id");
  auto const secret_access_key =
      ScopedEnvironment("AWS_SECRET_ACCESS_KEY", "test-secret-access-key");
  auto const token =
      ScopedEnvironment("AWS_SESSION_TOKEN", "test-session-token");
  auto const credentials_source = nlohmann::json{
      {"environment_id", "aws1"},
      {"region_url", kTestRegionUrl},
      {"url", kTestMetadataUrl},
      {"regional_cred_verification_url", kTestVerificationUrl},
      // {"imdsv2_session_token_url", kTestImdsv2Url},
  };
  auto const target =
      std::string{"//iam.googleapis.com/" +
                  Location(Project("PROJECT_NUMBER"), "global").FullName() +
                  "/workloadIdentityPools/POOL_ID/providers/PROVIDER_ID"};
  auto const source = MakeExternalAccountTokenSourceAws(
      credentials_source, target, MakeTestErrorContext());
  ASSERT_STATUS_OK(source);

  MockClientFactory client_factory;
  EXPECT_CALL(client_factory, Call).Times(0);

  auto subject_token = (*source)(client_factory.AsStdFunction(), Options{});
  ASSERT_STATUS_OK(subject_token);
  EXPECT_THAT(subject_token->token,
              HasSubstr("%22key%22%3A%22host%22%2C%22value%22%3A"
                        "%22sts.expected-region.amazonaws.com%22"));
}

TEST(ExternalAccountTokenSource, SourceImdsv2Failure) {
  // Return an error in fetching the imdsv2 token. In any implementation the
  // imdsv2 query *must* happen if it is needed to fetch the region and secrets.
  // Unset the environment variables to set that behavior.
  auto const region = ScopedEnvironment("AWS_REGION", absl::nullopt);
  auto const default_region =
      ScopedEnvironment("AWS_DEFAULT_REGION", absl::nullopt);
  auto const access_key_id =
      ScopedEnvironment("AWS_ACCESS_KEY_ID", absl::nullopt);
  auto const secret_access_key =
      ScopedEnvironment("AWS_SECRET_ACCESS_KEY", absl::nullopt);
  auto const token = ScopedEnvironment("AWS_SESSION_TOKEN", absl::nullopt);
  auto const credentials_source = nlohmann::json{
      {"environment_id", "aws1"},
      {"region_url", kTestRegionUrl},
      {"url", kTestMetadataUrl},
      {"regional_cred_verification_url", kTestVerificationUrl},
      {"imdsv2_session_token_url", kTestImdsv2Url},
  };
  auto const target =
      std::string{"//iam.googleapis.com/" +
                  Location(Project("$PROJECT_NUMBER"), "global").FullName() +
                  "/workloadIdentityPools/$POOL_ID/providers/$PROVIDER_ID"};
  auto const source = MakeExternalAccountTokenSourceAws(
      credentials_source, target, MakeTestErrorContext());
  ASSERT_STATUS_OK(source);

  MockClientFactory client_factory;
  EXPECT_CALL(client_factory, Call).WillOnce([] {
    auto mock = std::make_unique<MockRestClient>();
    auto expected_request = Property(&RestRequest::path, kTestImdsv2Url);
    EXPECT_CALL(*mock, Put(_, expected_request, _))
        .WillOnce(Return(ByMove(MakeMockResponseNotFound())));
    return mock;
  });

  auto subject_token = (*source)(client_factory.AsStdFunction(), Options{});
  EXPECT_THAT(subject_token, StatusIs(StatusCode::kNotFound));
}

TEST(ExternalAccountTokenSource, SourceRegionFailure) {
  // Do not use imdsv2, and force a metadata query to get the region. Use
  // environment variables for the secrets, in case the implementation order
  // changes.
  auto const region = ScopedEnvironment("AWS_REGION", absl::nullopt);
  auto const default_region =
      ScopedEnvironment("AWS_DEFAULT_REGION", absl::nullopt);
  auto const access_key_id =
      ScopedEnvironment("AWS_ACCESS_KEY_ID", "test-access-key-id");
  auto const secret_access_key =
      ScopedEnvironment("AWS_SECRET_ACCESS_KEY", "test-secret-access-key");
  auto const token =
      ScopedEnvironment("AWS_SESSION_TOKEN", "test-session-token");
  auto const credentials_source = nlohmann::json{
      {"environment_id", "aws1"},
      {"region_url", kTestRegionUrl},
      {"url", kTestMetadataUrl},
      {"regional_cred_verification_url", kTestVerificationUrl},
      // {"imdsv2_session_token_url", kTestImdsv2Url},
  };
  auto const target =
      std::string{"//iam.googleapis.com/" +
                  Location(Project("$PROJECT_NUMBER"), "global").FullName() +
                  "/workloadIdentityPools/$POOL_ID/providers/$PROVIDER_ID"};
  auto const source = MakeExternalAccountTokenSourceAws(
      credentials_source, target, MakeTestErrorContext());
  ASSERT_STATUS_OK(source);

  MockClientFactory client_factory;
  EXPECT_CALL(client_factory, Call).WillOnce([] {
    auto mock = std::make_unique<MockRestClient>();
    auto expected_request = Property(&RestRequest::path, kTestRegionUrl);
    EXPECT_CALL(*mock, Get(_, expected_request))
        .WillOnce(Return(ByMove(MakeMockResponseNotFound())));
    return mock;
  });

  auto subject_token = (*source)(client_factory.AsStdFunction(), Options{});
  EXPECT_THAT(subject_token, StatusIs(StatusCode::kNotFound));
}

TEST(ExternalAccountTokenSource, SourceSecretsFailure) {
  // Do not use imdsv2. Unset the environment variables for secrets to force an
  // HTTP request to obtain the secrets.  Keep the environment variables for
  // the region set, so this test works regardless of the query order.
  auto const region = ScopedEnvironment("AWS_REGION", "us-central1");
  auto const access_key_id =
      ScopedEnvironment("AWS_ACCESS_KEY_ID", absl::nullopt);
  auto const secret_access_key =
      ScopedEnvironment("AWS_SECRET_ACCESS_KEY", absl::nullopt);
  auto const token = ScopedEnvironment("AWS_SESSION_TOKEN", absl::nullopt);
  auto const credentials_source = nlohmann::json{
      {"environment_id", "aws1"},
      {"region_url", kTestRegionUrl},
      {"url", kTestMetadataUrl},
      {"regional_cred_verification_url", kTestVerificationUrl},
      // {"imdsv2_session_token_url", kTestImdsv2Url},
  };
  auto const target =
      std::string{"//iam.googleapis.com/" +
                  Location(Project("$PROJECT_NUMBER"), "global").FullName() +
                  "/workloadIdentityPools/$POOL_ID/providers/$PROVIDER_ID"};
  auto const source = MakeExternalAccountTokenSourceAws(
      credentials_source, target, MakeTestErrorContext());
  ASSERT_STATUS_OK(source);

  MockClientFactory client_factory;
  EXPECT_CALL(client_factory, Call).WillOnce([] {
    auto mock = std::make_unique<MockRestClient>();
    auto expected_request = Property(&RestRequest::path, kTestMetadataUrl);
    EXPECT_CALL(*mock, Get(_, expected_request))
        .WillOnce(Return(ByMove(MakeMockResponseNotFound())));
    return mock;
  });

  auto subject_token = (*source)(client_factory.AsStdFunction(), Options{});
  EXPECT_THAT(subject_token, StatusIs(StatusCode::kNotFound));
}

TEST(ExternalAccountTokenSource, ParseSuccess) {
  auto const creds = nlohmann::json{
      {"environment_id", "aws1"},
      {"region_url", kTestRegionUrl},
      {"url", kTestMetadataUrl},
      {"regional_cred_verification_url", kTestVerificationUrl},
      {"imdsv2_session_token_url", kTestImdsv2Url},
  };
  auto const info =
      ParseExternalAccountTokenSourceAws(creds, MakeTestErrorContext());
  ASSERT_STATUS_OK(info);
  EXPECT_EQ(info->environment_id, "aws1");
  EXPECT_EQ(info->region_url, kTestRegionUrl);
  EXPECT_EQ(info->url, kTestMetadataUrl);
  EXPECT_EQ(info->regional_cred_verification_url, kTestVerificationUrl);
  EXPECT_EQ(info->imdsv2_session_token_url, kTestImdsv2Url);
}

TEST(ExternalAccountTokenSource, ParseSuccessIPv6) {
  auto constexpr kTestMetadataUrlV6 = "http://[fd00:ec2::254]/metadata";
  auto constexpr kTestRegionUrlV6 = "http://[fd00:ec2::254]/region";
  auto constexpr kTestImdsv2UrlV6 = "http://[fd00:ec2::254]/imdsv2";

  auto const creds = nlohmann::json{
      {"environment_id", "aws1"},
      {"region_url", kTestRegionUrlV6},
      {"url", kTestMetadataUrlV6},
      {"regional_cred_verification_url", kTestVerificationUrl},
      {"imdsv2_session_token_url", kTestImdsv2UrlV6},
  };
  auto const info =
      ParseExternalAccountTokenSourceAws(creds, MakeTestErrorContext());
  ASSERT_STATUS_OK(info);
  EXPECT_EQ(info->environment_id, "aws1");
  EXPECT_EQ(info->region_url, kTestRegionUrlV6);
  EXPECT_EQ(info->url, kTestMetadataUrlV6);
  EXPECT_EQ(info->regional_cred_verification_url, kTestVerificationUrl);
  EXPECT_EQ(info->imdsv2_session_token_url, kTestImdsv2UrlV6);
}

TEST(ExternalAccountTokenSource, ParseSuccessNoUrl) {
  auto const creds = nlohmann::json{
      {"environment_id", "aws1"},
      {"region_url", kTestRegionUrl},
      // {"url", "test-url"},
      {"regional_cred_verification_url", kTestVerificationUrl},
      {"imdsv2_session_token_url", kTestImdsv2Url},
  };
  auto const info =
      ParseExternalAccountTokenSourceAws(creds, MakeTestErrorContext());
  ASSERT_STATUS_OK(info);
  EXPECT_EQ(info->environment_id, "aws1");
  EXPECT_EQ(info->region_url, kTestRegionUrl);
  EXPECT_EQ(info->url,
            "http://169.254.169.254/latest/meta-data/iam/security-credentials");
  EXPECT_EQ(info->regional_cred_verification_url, kTestVerificationUrl);
  EXPECT_EQ(info->imdsv2_session_token_url, kTestImdsv2Url);
}

TEST(ExternalAccountTokenSource, ParseSuccessNoImdsv2) {
  auto const creds = nlohmann::json{
      {"environment_id", "aws1"},
      {"region_url", kTestRegionUrl},
      {"url", kTestMetadataUrl},
      {"regional_cred_verification_url", kTestVerificationUrl},
      // {"imdsv2_session_token_url", "test-imdsv2"},
  };
  auto const info =
      ParseExternalAccountTokenSourceAws(creds, MakeTestErrorContext());
  ASSERT_STATUS_OK(info);
  EXPECT_EQ(info->environment_id, "aws1");
  EXPECT_EQ(info->region_url, kTestRegionUrl);
  EXPECT_EQ(info->url, kTestMetadataUrl);
  EXPECT_EQ(info->regional_cred_verification_url, kTestVerificationUrl);
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

TEST(ExternalAccountTokenSource, NonMetadataRegionUrl) {
  auto const creds = nlohmann::json{
      {"environment_id", "aws1"},
      {"region_url", "https://example.com"},
      {"regional_cred_verification_url", "test-verification-url"},
  };
  auto const info =
      ParseExternalAccountTokenSourceAws(creds, MakeTestErrorContext());
  EXPECT_THAT(info,
              StatusIs(StatusCode::kInvalidArgument,
                       AllOf(HasSubstr("the `region_url` field should refer"),
                             HasSubstr("https://example.com"))));
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

TEST(ExternalAccountTokenSource, NonMetadataUrl) {
  auto const creds = nlohmann::json{
      {"environment_id", "aws1"},
      {"region_url", "http://169.254.169.254/region"},
      {"url", "https://example.com"},
      {"regional_cred_verification_url", "test-verification-url"},
  };
  auto const info =
      ParseExternalAccountTokenSourceAws(creds, MakeTestErrorContext());
  EXPECT_THAT(info, StatusIs(StatusCode::kInvalidArgument,
                             AllOf(HasSubstr("the `url` field should refer"),
                                   HasSubstr("https://example.com"))));
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

TEST(ExternalAccountTokenSource, NonMetadataImdsv2SessionTokenUrl) {
  auto const creds = nlohmann::json{
      {"environment_id", "aws1"},
      {"region_url", kTestRegionUrl},
      {"regional_cred_verification_url", kTestVerificationUrl},
      {"imdsv2_session_token_url", "https://example.com"},
  };
  auto const info =
      ParseExternalAccountTokenSourceAws(creds, MakeTestErrorContext());
  EXPECT_THAT(
      info,
      StatusIs(
          StatusCode::kInvalidArgument,
          AllOf(HasSubstr("the `imdsv2_session_token_url` field should refer"),
                HasSubstr("https://example.com"))));
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
    auto mock = std::make_unique<MockRestClient>();
    auto expected_request = AllOf(
        Property(&RestRequest::path, kTestImdsv2Url),
        Property(&RestRequest::headers,
                 Contains(Pair(kMetadataTokenTtlHeader, Not(IsEmpty())))));
    EXPECT_CALL(*mock, Put(_, expected_request, _))
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
        auto mock = std::make_unique<MockRestClient>();
        auto expected_request = Property(&RestRequest::path, url);
        EXPECT_CALL(*mock, Get(_, expected_request))
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
        auto mock = std::make_unique<MockRestClient>();
        auto expected_request =
            AllOf(Property(&RestRequest::path, url),
                  // We expect the token returned by MakeImdsV2Client()
                  Property(&RestRequest::headers,
                           Contains(Pair(kMetadataTokenHeader,
                                         Contains(kTestImdsv2SessionToken)))));
        EXPECT_CALL(*mock, Get(_, expected_request))
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
        auto mock = std::make_unique<MockRestClient>();
        auto expected_request = Property(&RestRequest::path, url);
        EXPECT_CALL(*mock, Get(_, expected_request))
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
    auto mock = std::make_unique<MockRestClient>();
    auto expected_request = Property(&RestRequest::path, url);
    EXPECT_CALL(*mock, Get(_, expected_request))
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
    auto mock = std::make_unique<MockRestClient>();
    auto expected_request =
        AllOf(Property(&RestRequest::path, url),
              Property(&RestRequest::headers,
                       Contains(Pair(kMetadataTokenHeader,
                                     Contains(kTestImdsv2SessionToken)))));
    EXPECT_CALL(*mock, Get(_, expected_request))
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

TEST(ExternalAccountTokenSource, ComputeSubjectToken) {
  // Put the test values inline because it is hard to see how they are affected
  // otherwise.
  auto constexpr kTestRegion = "us-central1";
  auto constexpr kTestKeyId = "TESTKEYID123";
  auto constexpr kTestSecretAccessKey = "test/secret/access/key";
  auto constexpr kTestSessionToken = "";
  auto constexpr kRegionalCredUrl =
      "https://sts.{region}.example.com"
      "?Action=GetCallerIdentity&Version=2011-06-15&Test=Only";
  auto const secrets = ExternalAccountTokenSourceAwsSecrets{
      /*access_key_id=*/kTestKeyId,
      /*secret_access_key=*/kTestSecretAccessKey,
      /*session_token=*/kTestSessionToken};
  auto const info = ExternalAccountTokenSourceAwsInfo{
      /*environment_id=*/"aws1",
      /*region_url=*/kTestRegionUrl,
      /*url=*/kTestMetadataUrl,
      /*regional_cred_verification_url=*/kRegionalCredUrl,
      /*imdsv2_session_token_url=*/std::string{}};

  // Use a fixed timestamp to make the expected output more predictable.
  auto const tp =
      google::cloud::internal::ParseRfc3339("2022-12-15T01:02:03.123456789Z")
          .value();

  auto const target =
      std::string{"//iam.googleapis.com/" +
                  Location(Project("PROJECT_NUMBER"), "global").FullName() +
                  "/workloadIdentityPools/POOL_ID/providers/PROVIDER_ID"};

  auto const actual =
      ComputeSubjectToken(info, kTestRegion, secrets, tp, target);
  auto const subject =
      ComputeSubjectToken(info, kTestRegion, secrets, tp, target, true);
  // Verify that the debug version only adds a few values.
  auto expected = [](nlohmann::json json) {
    for (auto const* k :
         {"body_hash", "canonical_request", "canonical_request_hash",
          "string_to_sign", "k1", "k2", "k3", "k4", "signature"}) {
      json.erase(k);
    }
    return json;
  }(subject);

  EXPECT_EQ(expected, actual);
  EXPECT_EQ(subject.value("url", ""),
            "https://sts.us-central1.example.com"
            "?Action=GetCallerIdentity&Version=2011-06-15&Test=Only");
  EXPECT_THAT(subject.value("method", ""), "POST");
  EXPECT_TRUE(subject.contains("body"));
  EXPECT_THAT(subject.value("body", ""), IsEmpty());
  auto const headers = subject.value("headers", std::vector<nlohmann::json>{});
  // Most headers are easy to predict.
  EXPECT_THAT(headers, Contains(nlohmann::json{
                           {"key", "x-goog-cloud-target-resource"},
                           {"value", target},
                       }));
  EXPECT_THAT(headers, Contains(nlohmann::json{
                           {"key", "x-amz-date"},
                           {"value", "20221215T010203Z"},
                       }));
  EXPECT_THAT(headers, Contains(nlohmann::json{
                           {"key", "host"},
                           {"value", "sts.us-central1.amazonaws.com"},
                       }));
  EXPECT_THAT(headers, Not(Contains(nlohmann::json{
                           {"key", "x-amz-security-token"},
                           {"value", kTestSessionToken},
                       })));

  auto const authorization = [&] {
    for (auto const& j : headers) {
      if (j.value("key", "") == "authorization") return j.value("value", "");
    }
    return std::string{};
  }();
  EXPECT_THAT(authorization, StartsWith("AWS4-HMAC-SHA256 "));
  auto fields = std::map<std::string, std::string>{};
  for (auto text : absl::StrSplit(
           absl::StripPrefix(authorization, "AWS4-HMAC-SHA256 "), ',')) {
    fields.insert(absl::StrSplit(text, absl::MaxSplits('=', 1)));
  }
  EXPECT_THAT(fields,
              Contains(Pair("Credential",
                            secrets.access_key_id +
                                "/20221215/us-central1/sts/aws4_request")));
  EXPECT_THAT(fields,
              Contains(Pair("SignedHeaders",
                            "host;x-amz-date;x-goog-cloud-target-resource")));
  EXPECT_THAT(
      fields,
      Contains(Pair(
          "Signature",
          "e3d3681f82032cc2d5772173ca3f63ed1a1b5ae89f5a3c24dae5c29edb62f298")));

  // To obtain the signature we use a fairly complicated pipeline.
  // clang-format off
  /**
      # body_hash
      EH=$(printf "" | sha256sum | cut -f1 -d ' ')
      K1=$(echo -n "20221215"     | openssl  dgst -sha256 -mac HMAC -macopt key:AWS4test/secret/access/key | cut -f2 -d' ')
      K2=$(echo -n "us-central1"  | openssl  dgst -sha256 -mac HMAC -macopt hexkey:${K1} | cut -f2 -d' ')
      K3=$(echo -n "sts"          | openssl  dgst -sha256 -mac HMAC -macopt hexkey:${K2} | cut -f2 -d' ')
      K4=$(echo -n "aws4_request" | openssl  dgst -sha256 -mac HMAC -macopt hexkey:${K3} | cut -f2 -d' ')
      # canonical_request_hash
      RH=$( (echo "POST" ;
             echo "/" ;
             echo "Action=GetCallerIdentity&Version=2011-06-15&Test=Only" ;
             echo "host:sts.us-central1.amazonaws.com" ;
             echo "x-amz-date:20221215T010203Z" ;
             echo "x-goog-cloud-target-resource://iam.googleapis.com/projects/PROJECT_NUMBER/locations/global/workloadIdentityPools/POOL_ID/providers/PROVIDER_ID" ;
             echo "" ;
             echo "host;x-amz-date;x-goog-cloud-target-resource" ;
             printf "${EH}"
             ) | sha256sum | cut -f1 -d ' ')
      # signature
      S=$( (echo "AWS4-HMAC-SHA256";
            echo "20221215T010203Z";
            echo "20221215/us-central1/sts/aws4_request";
            printf "${RH}"
            ) |
            openssl dgst -sha256 -mac HMAC -macopt hexkey:${K4} | cut -f2 -d' ')

      EH=e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855
      K1=29e98e306cb1b36b91ac7f41b613d2836ad7c34dcbef0b4ba5c1fedab921f1a2
      K2=fddbd9a610661ec06144237e777bbca4b2da50b7f214cec6a929e3e9cf20779f
      K3=b4aa191870313f880efeb84203e140214e7a1eaa8bf199c34d26519007fc3305
      K4=fe95b5bac5f243cd7c77fb4f2b143377d9cdd42e6ef1a7ef4d68bb2526dbe414
      RH=27d7c240af4b780c8e9380e7e57bea58802b8488f776fc260510041de294727a
      S=e3d3681f82032cc2d5772173ca3f63ed1a1b5ae89f5a3c24dae5c29edb62f298
  */
  // clang-format on

  auto constexpr kStringToSign = R"""(AWS4-HMAC-SHA256
20221215T010203Z
20221215/us-central1/sts/aws4_request
27d7c240af4b780c8e9380e7e57bea58802b8488f776fc260510041de294727a)""";

  auto constexpr kCanonicalRequest = R"""(POST
/
Action=GetCallerIdentity&Version=2011-06-15&Test=Only
host:sts.us-central1.amazonaws.com
x-amz-date:20221215T010203Z
x-goog-cloud-target-resource://iam.googleapis.com/projects/PROJECT_NUMBER/locations/global/workloadIdentityPools/POOL_ID/providers/PROVIDER_ID

host;x-amz-date;x-goog-cloud-target-resource
e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855)""";

  struct ExpectedDebug {
    std::string name;
    std::string value;
  } const expected_values[] = {
      {"body_hash",
       "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"},
      {"canonical_request_hash",
       "27d7c240af4b780c8e9380e7e57bea58802b8488f776fc260510041de294727a"},
      {"string_to_sign", kStringToSign},
      {"canonical_request", kCanonicalRequest},
      {"k1",
       "29e98e306cb1b36b91ac7f41b613d2836ad7c34dcbef0b4ba5c1fedab921f1a2"},
      {"k2",
       "fddbd9a610661ec06144237e777bbca4b2da50b7f214cec6a929e3e9cf20779f"},
      {"k3",
       "b4aa191870313f880efeb84203e140214e7a1eaa8bf199c34d26519007fc3305"},
      {"k4",
       "fe95b5bac5f243cd7c77fb4f2b143377d9cdd42e6ef1a7ef4d68bb2526dbe414"},
      {"signature",
       "e3d3681f82032cc2d5772173ca3f63ed1a1b5ae89f5a3c24dae5c29edb62f298"},
  };

  for (auto const& e : expected_values) {
    EXPECT_EQ(subject.value(e.name, ""), e.value) << "name=" << e.name;
  }
}

TEST(ExternalAccountTokenSource, ComputeSubjectTokenWithSessionToken) {
  // Put the test values inline because it is hard to see how they are affected
  // otherwise.
  auto constexpr kTestRegion = "us-central1";
  auto constexpr kTestKeyId = "TESTKEYID123";
  auto constexpr kTestSecretAccessKey = "test/secret/access/key";
  auto constexpr kTestSessionToken = "test-session-token";
  auto constexpr kRegionalCredUrl =
      "https://sts.{region}.example.com"
      "?Action=GetCallerIdentity&Version=2011-06-15&Test=Only";
  auto const secrets = ExternalAccountTokenSourceAwsSecrets{
      /*access_key_id=*/kTestKeyId,
      /*secret_access_key=*/kTestSecretAccessKey,
      /*session_token=*/kTestSessionToken};
  auto const info = ExternalAccountTokenSourceAwsInfo{
      /*environment_id=*/"aws1",
      /*region_url=*/kTestRegionUrl,
      /*url=*/kTestMetadataUrl,
      /*regional_cred_verification_url=*/kRegionalCredUrl,
      /*imdsv2_session_token_url=*/std::string{}};

  // Use a fixed timestamp to make the expected output more predictable.
  auto const tp =
      google::cloud::internal::ParseRfc3339("2022-12-15T01:02:03.123456789Z")
          .value();

  auto const target =
      std::string{"//iam.googleapis.com/" +
                  Location(Project("PROJECT_NUMBER"), "global").FullName() +
                  "/workloadIdentityPools/POOL_ID/providers/PROVIDER_ID"};

  auto const actual =
      ComputeSubjectToken(info, kTestRegion, secrets, tp, target);
  auto const subject =
      ComputeSubjectToken(info, kTestRegion, secrets, tp, target, true);
  // Verify that the debug version only adds a few values.
  auto expected = [](nlohmann::json json) {
    for (auto const* k :
         {"body_hash", "canonical_request", "canonical_request_hash",
          "string_to_sign", "k1", "k2", "k3", "k4", "signature"}) {
      json.erase(k);
    }
    return json;
  }(subject);

  EXPECT_EQ(expected, actual);
  EXPECT_EQ(subject.value("url", ""),
            "https://sts.us-central1.example.com"
            "?Action=GetCallerIdentity&Version=2011-06-15&Test=Only");
  EXPECT_THAT(subject.value("method", ""), "POST");
  EXPECT_TRUE(subject.contains("body"));
  EXPECT_THAT(subject.value("body", ""), IsEmpty());
  auto const headers = subject.value("headers", std::vector<nlohmann::json>{});
  // Most headers are easy to predict.
  EXPECT_THAT(headers, Contains(nlohmann::json{
                           {"key", "x-goog-cloud-target-resource"},
                           {"value", target},
                       }));
  EXPECT_THAT(headers, Contains(nlohmann::json{
                           {"key", "x-amz-date"},
                           {"value", "20221215T010203Z"},
                       }));
  EXPECT_THAT(headers, Contains(nlohmann::json{
                           {"key", "host"},
                           {"value", "sts.us-central1.amazonaws.com"},
                       }));
  EXPECT_THAT(headers, Contains(nlohmann::json{
                           {"key", "x-amz-security-token"},
                           {"value", kTestSessionToken},
                       }));

  auto const authorization = [&] {
    for (auto const& j : headers) {
      if (j.value("key", "") == "authorization") return j.value("value", "");
    }
    return std::string{};
  }();
  EXPECT_THAT(authorization, StartsWith("AWS4-HMAC-SHA256 "));
  auto fields = std::map<std::string, std::string>{};
  for (auto text : absl::StrSplit(
           absl::StripPrefix(authorization, "AWS4-HMAC-SHA256 "), ',')) {
    fields.insert(absl::StrSplit(text, absl::MaxSplits('=', 1)));
  }
  EXPECT_THAT(fields,
              Contains(Pair("Credential",
                            secrets.access_key_id +
                                "/20221215/us-central1/sts/aws4_request")));
  EXPECT_THAT(fields, Contains(Pair("SignedHeaders",
                                    "host;x-amz-date;x-amz-security-token;x-"
                                    "goog-cloud-target-resource")));
  EXPECT_THAT(
      fields,
      Contains(Pair(
          "Signature",
          "c7e686c23990b72ea77d0163651ab0d087ecdb43d8dde1499387d7c24fdc92eb")));

  // To obtain the signature we use a fairly complicated pipeline.
  // clang-format off
  /**
      # body_hash
      EH=$(printf "" | sha256sum | cut -f1 -d ' ')
      K1=$(echo -n "20221215"     | openssl  dgst -sha256 -mac HMAC -macopt key:AWS4test/secret/access/key | cut -f2 -d' ')
      K2=$(echo -n "us-central1"  | openssl  dgst -sha256 -mac HMAC -macopt hexkey:${K1} | cut -f2 -d' ')
      K3=$(echo -n "sts"          | openssl  dgst -sha256 -mac HMAC -macopt hexkey:${K2} | cut -f2 -d' ')
      K4=$(echo -n "aws4_request" | openssl  dgst -sha256 -mac HMAC -macopt hexkey:${K3} | cut -f2 -d' ')
      # canonical_request_hash
      RH=$( (echo "POST" ;
             echo "/" ;
             echo "Action=GetCallerIdentity&Version=2011-06-15&Test=Only" ;
             echo "host:sts.us-central1.amazonaws.com" ;
             echo "x-amz-date:20221215T010203Z" ;
             echo "x-amz-security-token:test-session-token" ;
             echo "x-goog-cloud-target-resource://iam.googleapis.com/projects/PROJECT_NUMBER/locations/global/workloadIdentityPools/POOL_ID/providers/PROVIDER_ID" ;
             echo "" ;
             echo "host;x-amz-date;x-amz-security-token;x-goog-cloud-target-resource" ;
             printf "${EH}"
             ) | sha256sum | cut -f1 -d ' ')
      # signature
      S=$( (echo "AWS4-HMAC-SHA256";
            echo "20221215T010203Z";
            echo "20221215/us-central1/sts/aws4_request";
            printf "${RH}"
            ) |
            openssl dgst -sha256 -mac HMAC -macopt hexkey:${K4} | cut -f2 -d' ')

      EH=e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855
      K1=29e98e306cb1b36b91ac7f41b613d2836ad7c34dcbef0b4ba5c1fedab921f1a2
      K2=fddbd9a610661ec06144237e777bbca4b2da50b7f214cec6a929e3e9cf20779f
      K3=b4aa191870313f880efeb84203e140214e7a1eaa8bf199c34d26519007fc3305
      K4=fe95b5bac5f243cd7c77fb4f2b143377d9cdd42e6ef1a7ef4d68bb2526dbe414
      RH=6b0a92261314977c6befef7b6899a60db5b8bce3ea78bb8b455ce53242bf3a9c
      S=c7e686c23990b72ea77d0163651ab0d087ecdb43d8dde1499387d7c24fdc92eb
  */
  // clang-format on

  auto constexpr kStringToSign = R"""(AWS4-HMAC-SHA256
20221215T010203Z
20221215/us-central1/sts/aws4_request
6b0a92261314977c6befef7b6899a60db5b8bce3ea78bb8b455ce53242bf3a9c)""";

  auto constexpr kCanonicalRequest = R"""(POST
/
Action=GetCallerIdentity&Version=2011-06-15&Test=Only
host:sts.us-central1.amazonaws.com
x-amz-date:20221215T010203Z
x-amz-security-token:test-session-token
x-goog-cloud-target-resource://iam.googleapis.com/projects/PROJECT_NUMBER/locations/global/workloadIdentityPools/POOL_ID/providers/PROVIDER_ID

host;x-amz-date;x-amz-security-token;x-goog-cloud-target-resource
e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855)""";

  struct ExpectedDebug {
    std::string name;
    std::string value;
  } const expected_values[] = {
      {"body_hash",
       "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"},
      {"canonical_request_hash",
       "6b0a92261314977c6befef7b6899a60db5b8bce3ea78bb8b455ce53242bf3a9c"},
      {"string_to_sign", kStringToSign},
      {"canonical_request", kCanonicalRequest},
      {"k1",
       "29e98e306cb1b36b91ac7f41b613d2836ad7c34dcbef0b4ba5c1fedab921f1a2"},
      {"k2",
       "fddbd9a610661ec06144237e777bbca4b2da50b7f214cec6a929e3e9cf20779f"},
      {"k3",
       "b4aa191870313f880efeb84203e140214e7a1eaa8bf199c34d26519007fc3305"},
      {"k4",
       "fe95b5bac5f243cd7c77fb4f2b143377d9cdd42e6ef1a7ef4d68bb2526dbe414"},
      {"signature",
       "c7e686c23990b72ea77d0163651ab0d087ecdb43d8dde1499387d7c24fdc92eb"},
  };

  for (auto const& e : expected_values) {
    EXPECT_EQ(subject.value(e.name, ""), e.value) << "name=" << e.name;
  }
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace oauth2_internal
}  // namespace cloud
}  // namespace google
