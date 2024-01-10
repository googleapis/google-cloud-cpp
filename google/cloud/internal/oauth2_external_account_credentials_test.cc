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

#include "google/cloud/internal/oauth2_external_account_credentials.h"
#include "google/cloud/internal/oauth2_credential_constants.h"
#include "google/cloud/testing_util/chrono_output.h"
#include "google/cloud/testing_util/mock_http_payload.h"
#include "google/cloud/testing_util/mock_rest_client.h"
#include "google/cloud/testing_util/mock_rest_response.h"
#include "google/cloud/testing_util/scoped_environment.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>
#include <nlohmann/json.hpp>
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
using ::google::cloud::testing_util::MockHttpPayload;
using ::google::cloud::testing_util::MockRestClient;
using ::google::cloud::testing_util::MockRestResponse;
using ::google::cloud::testing_util::ScopedEnvironment;
using ::google::cloud::testing_util::StatusIs;
using ::testing::_;
using ::testing::AllOf;
using ::testing::An;
using ::testing::AtMost;
using ::testing::ByMove;
using ::testing::Contains;
using ::testing::ElementsAre;
using ::testing::HasSubstr;
using ::testing::MatcherCast;
using ::testing::Pair;
using ::testing::Property;
using ::testing::ResultOf;
using ::testing::Return;
using ::testing::UnorderedElementsAre;

using MockClientFactory =
    ::testing::MockFunction<std::unique_ptr<rest_internal::RestClient>(
        Options const&)>;

std::unique_ptr<RestResponse> MakeMockResponse(HttpStatusCode code,
                                               std::string contents) {
  auto response = std::make_unique<MockRestResponse>();
  EXPECT_CALL(*response, StatusCode).WillRepeatedly(Return(code));
  EXPECT_CALL(std::move(*response), ExtractPayload)
      .Times(AtMost(1))
      .WillRepeatedly([contents = std::move(contents)]() mutable {
        return MakeMockHttpPayloadSuccess(std::move(contents));
      });
  return response;
}

std::unique_ptr<RestResponse> MakeMockResponseSuccess(std::string contents) {
  return MakeMockResponse(HttpStatusCode::kOk, std::move(contents));
}

// A full error payload, parseable as an error info.
auto constexpr kErrorPayload = R"""({
  "error": {
    "code": 404,
    "message": "token not found.",
    "status": "NOT_FOUND",
    "details": [
      {
        "@type": "type.googleapis.com/google.rpc.ErrorInfo",
        "reason": "TEST ONLY",
        "domain": "metadata.google.internal",
        "metadata": {
          "service": "metadata.google.internal",
          "context": "GKE"
        }
      }
    ]
  }
})""";

std::unique_ptr<RestResponse> MakeMockResponseError() {
  return MakeMockResponse(HttpStatusCode::kNotFound,
                          std::string{kErrorPayload});
}

std::unique_ptr<RestResponse> MakeMockResponsePartialError(
    std::string partial) {
  auto response = std::make_unique<MockRestResponse>();
  EXPECT_CALL(*response, StatusCode)
      .WillRepeatedly(Return(HttpStatusCode::kOk));
  EXPECT_CALL(std::move(*response), ExtractPayload)
      .Times(AtMost(1))
      .WillRepeatedly([partial = std::move(partial)]() mutable {
        auto payload = std::make_unique<MockHttpPayload>();
        // This is shared by the next two mocking functions.
        auto c = std::make_shared<std::string>();
        EXPECT_CALL(*payload, HasUnreadData).WillRepeatedly([c] {
          return true;
        });
        EXPECT_CALL(*payload, Read)
            .WillRepeatedly(
                [c](absl::Span<char> buffer) -> StatusOr<std::size_t> {
                  auto const n = (std::min)(buffer.size(), c->size());
                  std::copy(c->begin(), std::next(c->begin(), n),
                            buffer.begin());
                  c->assign(c->substr(n));
                  if (n != 0) return n;
                  return Status{StatusCode::kUnavailable, "read error"};
                });
        return std::unique_ptr<rest_internal::HttpPayload>(std::move(payload));
      });
  return response;
}

using FormDataType = std::vector<std::pair<std::string, std::string>>;

struct TestOnlyOption {
  using Type = std::string;
};

TEST(ExternalAccount, ParseAwsSuccess) {
  // To simplify the test we provide all the parameters via environment
  // variables and avoid using imdsv2.
  auto const region = ScopedEnvironment("AWS_REGION", "expected-region");
  auto const access_key_id =
      ScopedEnvironment("AWS_ACCESS_KEY_ID", "test-access-key-id");
  auto const secret_access_key =
      ScopedEnvironment("AWS_SECRET_ACCESS_KEY", "test-secret-access-key");
  auto const token =
      ScopedEnvironment("AWS_SESSION_TOKEN", "test-session-token");
  auto constexpr kTestRegionUrl = "http://169.254.169.254/region";
  auto constexpr kTestMetadataUrl = "http://169.254.169.254/metadata";
  auto constexpr kTestVerificationUrl =
      "https://sts.{region}.aws.example.com"
      "?Action=GetCallerIdentity&Version=2011-06-15";
  auto constexpr kTestTokenType = "urn:ietf:params:aws:token-type:aws4_request";
  auto constexpr kTestAudience =
      "//iam.googleapis.com/projects/$PROJECT_NUMBER/locations/global/"
      "workloadIdentityPools/$POOL_ID/providers/$PROVIDER_ID";
  auto constexpr kUniverseDomain = "my-domain.net";

  auto const configuration = nlohmann::json{
      {"type", "external_account"},
      {"audience", kTestAudience},
      {"subject_token_type", kTestTokenType},
      {"token_url", "test-token-url"},
      {"universe_domain", kUniverseDomain},
      {"credential_source",
       nlohmann::json{
           {"environment_id", "aws1"},
           {"region_url", kTestRegionUrl},
           {"url", kTestMetadataUrl},
           {"regional_cred_verification_url", kTestVerificationUrl},
           // {"imdsv2_session_token_url", kTestImdsv2Url},
       }},
  };
  auto ec = internal::ErrorContext(
      {{"program", "test"}, {"full-configuration", configuration.dump()}});
  auto const actual =
      ParseExternalAccountConfiguration(configuration.dump(), ec);
  ASSERT_STATUS_OK(actual);
  EXPECT_EQ(actual->audience, kTestAudience);
  EXPECT_EQ(actual->subject_token_type, kTestTokenType);
  EXPECT_EQ(actual->token_url, "test-token-url");
  EXPECT_EQ(actual->universe_domain, kUniverseDomain);

  MockClientFactory client_factory;
  EXPECT_CALL(client_factory, Call).Times(0);
  auto const subject =
      actual->token_source(client_factory.AsStdFunction(), Options{});
  ASSERT_STATUS_OK(subject);
  EXPECT_THAT(subject->token,
              HasSubstr("%22key%22%3A%22host%22%2C%22value%22%3A"
                        "%22sts.expected-region.amazonaws.com%22"));
}

TEST(ExternalAccount, ParseUrlSuccess) {
  auto const configuration = nlohmann::json{
      {"type", "external_account"},
      {"audience", "test-audience"},
      {"subject_token_type", "test-subject-token-type"},
      {"token_url", "test-token-url"},
      {"credential_source",
       nlohmann::json{
           {"url", "https://test-only-oidc.exampl.com/"},
       }},
  };
  auto ec = internal::ErrorContext(
      {{"program", "test"}, {"full-configuration", configuration.dump()}});
  auto const actual =
      ParseExternalAccountConfiguration(configuration.dump(), ec);
  ASSERT_STATUS_OK(actual);
  EXPECT_EQ(actual->audience, "test-audience");
  EXPECT_EQ(actual->subject_token_type, "test-subject-token-type");
  EXPECT_EQ(actual->token_url, "test-token-url");
}

TEST(ExternalAccount, ParseFileSuccess) {
  auto const configuration = nlohmann::json{
      {"type", "external_account"},
      {"audience", "test-audience"},
      {"subject_token_type", "test-subject-token-type"},
      {"token_url", "test-token-url"},
      {"credential_source", nlohmann::json{{"file", "/dev/null-test-only"}}},
  };
  auto ec = internal::ErrorContext(
      {{"program", "test"}, {"full-configuration", configuration.dump()}});
  auto const actual =
      ParseExternalAccountConfiguration(configuration.dump(), ec);
  ASSERT_STATUS_OK(actual);
  EXPECT_EQ(actual->audience, "test-audience");
  EXPECT_EQ(actual->subject_token_type, "test-subject-token-type");
  EXPECT_EQ(actual->token_url, "test-token-url");
}

TEST(ExternalAccount, ParseWithImpersonationSuccess) {
  auto const configuration = nlohmann::json{
      {"type", "external_account"},
      {"audience", "test-audience"},
      {"subject_token_type", "test-subject-token-type"},
      {"token_url", "test-token-url"},
      {"credential_source", nlohmann::json{{"file", "/dev/null-test-only"}}},
      {"service_account_impersonation_url", "https://test-only.example.com/"},
  };
  auto ec = internal::ErrorContext(
      {{"program", "test"}, {"full-configuration", configuration.dump()}});
  auto const actual =
      ParseExternalAccountConfiguration(configuration.dump(), ec);
  ASSERT_STATUS_OK(actual);
  EXPECT_EQ(actual->audience, "test-audience");
  EXPECT_EQ(actual->subject_token_type, "test-subject-token-type");
  EXPECT_EQ(actual->token_url, "test-token-url");
  ASSERT_TRUE(actual->impersonation_config.has_value());
  EXPECT_EQ(actual->impersonation_config->url,
            "https://test-only.example.com/");
  EXPECT_EQ(actual->impersonation_config->token_lifetime,
            std::chrono::seconds(3600));
}

TEST(ExternalAccount, ParseWithImpersonationLifetimeSuccess) {
  auto const configuration = nlohmann::json{
      {"type", "external_account"},
      {"audience", "test-audience"},
      {"subject_token_type", "test-subject-token-type"},
      {"token_url", "test-token-url"},
      {"credential_source", nlohmann::json{{"file", "/dev/null-test-only"}}},
      {"service_account_impersonation_url", "https://test-only.example.com/"},
      {"service_account_impersonation",
       nlohmann::json{{"token_lifetime_seconds", 2800}}},
  };
  auto ec = internal::ErrorContext(
      {{"program", "test"}, {"full-configuration", configuration.dump()}});
  auto const actual =
      ParseExternalAccountConfiguration(configuration.dump(), ec);
  ASSERT_STATUS_OK(actual);
  EXPECT_EQ(actual->audience, "test-audience");
  EXPECT_EQ(actual->subject_token_type, "test-subject-token-type");
  EXPECT_EQ(actual->token_url, "test-token-url");
  ASSERT_TRUE(actual->impersonation_config.has_value());
  EXPECT_EQ(actual->impersonation_config->url,
            "https://test-only.example.com/");
  EXPECT_EQ(actual->impersonation_config->token_lifetime,
            std::chrono::seconds(2800));
}

TEST(ExternalAccount, ParseWithImpersonationDefaultLifetimeSuccess) {
  auto const configuration = nlohmann::json{
      {"type", "external_account"},
      {"audience", "test-audience"},
      {"subject_token_type", "test-subject-token-type"},
      {"token_url", "test-token-url"},
      {"credential_source", nlohmann::json{{"file", "/dev/null-test-only"}}},
      {"service_account_impersonation_url", "https://test-only.example.com/"},
      {"service_account_impersonation", nlohmann::json::object()},
  };
  auto ec = internal::ErrorContext(
      {{"program", "test"}, {"full-configuration", configuration.dump()}});
  auto const actual =
      ParseExternalAccountConfiguration(configuration.dump(), ec);
  ASSERT_STATUS_OK(actual);
  EXPECT_EQ(actual->audience, "test-audience");
  EXPECT_EQ(actual->subject_token_type, "test-subject-token-type");
  EXPECT_EQ(actual->token_url, "test-token-url");
  ASSERT_TRUE(actual->impersonation_config.has_value());
  EXPECT_EQ(actual->impersonation_config->url,
            "https://test-only.example.com/");
  EXPECT_EQ(actual->impersonation_config->token_lifetime,
            std::chrono::seconds(3600));
}

TEST(ExternalAccount, ParseNotJson) {
  auto const configuration = std::string{"not-json"};
  auto ec = internal::ErrorContext(
      {{"program", "test"}, {"full-configuration", configuration}});
  auto const actual = ParseExternalAccountConfiguration(configuration, ec);
  EXPECT_THAT(actual, StatusIs(StatusCode::kInvalidArgument,
                               HasSubstr("not a JSON object")));
}

TEST(ExternalAccount, ParseNotJsonObject) {
  auto const configuration = std::string{R"""("json-but-not-json-object")"""};
  auto ec = internal::ErrorContext(
      {{"program", "test"}, {"full-configuration", configuration}});
  auto const actual = ParseExternalAccountConfiguration(configuration, ec);
  EXPECT_THAT(actual, StatusIs(StatusCode::kInvalidArgument,
                               HasSubstr("not a JSON object")));
}

TEST(ExternalAccount, ParseMissingType) {
  auto const configuration = nlohmann::json{
      // {"type", "external_account"},
      {"audience", "test-audience"},
      {"subject_token_type", "test-subject-token-type"},
      {"token_url", "test-token-url"},
      {"credential_source", nlohmann::json{{"file", "/dev/null-test-only"}}},
  };
  auto ec = internal::ErrorContext(
      {{"program", "test"}, {"full-configuration", configuration.dump()}});
  auto const actual =
      ParseExternalAccountConfiguration(configuration.dump(), ec);
  EXPECT_THAT(actual, StatusIs(StatusCode::kInvalidArgument,
                               HasSubstr("cannot find `type` field")));
}

TEST(ExternalAccount, ParseInvalidType) {
  auto const configuration = nlohmann::json{
      {"type", true},  // should be string
      {"audience", "test-audience"},
      {"subject_token_type", "test-subject-token-type"},
      {"token_url", "test-token-url"},
      {"credential_source", nlohmann::json{{"file", "/dev/null-test-only"}}},
  };
  auto ec = internal::ErrorContext(
      {{"program", "test"}, {"full-configuration", configuration.dump()}});
  auto const actual =
      ParseExternalAccountConfiguration(configuration.dump(), ec);
  EXPECT_THAT(actual, StatusIs(StatusCode::kInvalidArgument,
                               HasSubstr("invalid type for `type` field")));
}

TEST(ExternalAccount, ParseMismatchedType) {
  auto const configuration = nlohmann::json{
      {"type", "mismatched-external_account"},
      {"audience", "test-audience"},
      {"subject_token_type", "test-subject-token-type"},
      {"token_url", "test-token-url"},
      {"credential_source", nlohmann::json{{"file", "/dev/null-test-only"}}},
  };
  auto ec = internal::ErrorContext(
      {{"program", "test"}, {"full-configuration", configuration.dump()}});
  auto const actual =
      ParseExternalAccountConfiguration(configuration.dump(), ec);
  EXPECT_THAT(
      actual,
      StatusIs(StatusCode::kInvalidArgument,
               HasSubstr("mismatched type (mismatched-external_account)")));
}

TEST(ExternalAccount, ParseMissingAudience) {
  auto const configuration = nlohmann::json{
      {"type", "external_account"},
      // {"audience", "test-audience"},
      {"subject_token_type", "test-subject-token-type"},
      {"token_url", "test-token-url"},
      {"credential_source", nlohmann::json{{"file", "/dev/null-test-only"}}},
  };
  auto ec = internal::ErrorContext(
      {{"program", "test"}, {"full-configuration", configuration.dump()}});
  auto const actual =
      ParseExternalAccountConfiguration(configuration.dump(), ec);
  EXPECT_THAT(actual, StatusIs(StatusCode::kInvalidArgument,
                               HasSubstr("cannot find `audience` field")));
}

TEST(ExternalAccount, ParseInvalidAudience) {
  auto const configuration = nlohmann::json{
      {"type", "external_account"},
      {"audience", true},  // should be string
      {"subject_token_type", "test-subject-token-type"},
      {"token_url", "test-token-url"},
      {"credential_source", nlohmann::json{{"file", "/dev/null-test-only"}}},
  };
  auto ec = internal::ErrorContext(
      {{"program", "test"}, {"full-configuration", configuration.dump()}});
  auto const actual =
      ParseExternalAccountConfiguration(configuration.dump(), ec);
  EXPECT_THAT(actual, StatusIs(StatusCode::kInvalidArgument,
                               HasSubstr("invalid type for `audience` field")));
}

TEST(ExternalAccount, ParseMissingSubjectTokenType) {
  auto const configuration = nlohmann::json{
      {"type", "external_account"},
      {"audience", "test-audience"},
      // {"subject_token_type", "test-subject-token-type"},
      {"token_url", "test-token-url"},
      {"credential_source", nlohmann::json{{"file", "/dev/null-test-only"}}},
  };
  auto ec = internal::ErrorContext(
      {{"program", "test"}, {"full-configuration", configuration.dump()}});
  auto const actual =
      ParseExternalAccountConfiguration(configuration.dump(), ec);
  EXPECT_THAT(actual,
              StatusIs(StatusCode::kInvalidArgument,
                       HasSubstr("cannot find `subject_token_type` field")));
}

TEST(ExternalAccount, ParseInvalidSubjectTokenType) {
  auto const configuration = nlohmann::json{
      {"type", "external_account"},
      {"audience", "test-audience"},
      {"subject_token_type", true},  // should be string
      {"token_url", "test-token-url"},
      {"credential_source", nlohmann::json{{"file", "/dev/null-test-only"}}},
  };
  auto ec = internal::ErrorContext(
      {{"program", "test"}, {"full-configuration", configuration.dump()}});
  auto const actual =
      ParseExternalAccountConfiguration(configuration.dump(), ec);
  EXPECT_THAT(
      actual,
      StatusIs(StatusCode::kInvalidArgument,
               HasSubstr("invalid type for `subject_token_type` field")));
}

TEST(ExternalAccount, ParseMissingTokenUrl) {
  auto const configuration = nlohmann::json{
      {"type", "external_account"},
      {"audience", "test-audience"},
      {"subject_token_type", "test-subject-token-type"},
      // {"token_url", "test-token-url"},
      {"credential_source", nlohmann::json{{"file", "/dev/null-test-only"}}},
  };
  auto ec = internal::ErrorContext(
      {{"program", "test"}, {"full-configuration", configuration.dump()}});
  auto const actual =
      ParseExternalAccountConfiguration(configuration.dump(), ec);
  EXPECT_THAT(actual, StatusIs(StatusCode::kInvalidArgument,
                               HasSubstr("cannot find `token_url` field")));
}

TEST(ExternalAccount, ParseInvalidTokenUrl) {
  auto const configuration = nlohmann::json{
      {"type", "external_account"},
      {"audience", "test-audience"},
      {"subject_token_type", "test-subject-token-type"},
      {"token_url", true},  // should be string
      {"credential_source", nlohmann::json{{"file", "/dev/null-test-only"}}},
  };
  auto ec = internal::ErrorContext(
      {{"program", "test"}, {"full-configuration", configuration.dump()}});
  auto const actual =
      ParseExternalAccountConfiguration(configuration.dump(), ec);
  EXPECT_THAT(actual,
              StatusIs(StatusCode::kInvalidArgument,
                       HasSubstr("invalid type for `token_url` field")));
}

TEST(ExternalAccount, ParseMissingUniverseDomain) {
  auto const configuration = nlohmann::json{
      {"type", "external_account"},
      {"audience", "test-audience"},
      {"subject_token_type", "test-subject-token-type"},
      {"token_url", "test-token-url"},
      {"credential_source", nlohmann::json{{"file", "/dev/null-test-only"}}},
  };
  auto ec = internal::ErrorContext(
      {{"program", "test"}, {"full-configuration", configuration.dump()}});
  auto const actual =
      ParseExternalAccountConfiguration(configuration.dump(), ec);
  ASSERT_STATUS_OK(actual);
  EXPECT_EQ(actual->universe_domain, "googleapis.com");
}

TEST(ExternalAccount, ParseInvalidUniverseDomain) {
  auto const configuration = nlohmann::json{
      {"type", "external_account"},
      {"audience", "test-audience"},
      {"subject_token_type", "test-subject-token-type"},
      {"token_url", "test-token-url"},
      {"universe_domain", ""},
      {"credential_source", nlohmann::json{{"file", "/dev/null-test-only"}}},
  };
  auto ec = internal::ErrorContext(
      {{"program", "test"}, {"full-configuration", configuration.dump()}});
  auto const actual =
      ParseExternalAccountConfiguration(configuration.dump(), ec);
  EXPECT_THAT(
      actual,
      StatusIs(
          StatusCode::kInvalidArgument,
          HasSubstr(
              "universe_domain field in credentials file cannot be empty")));
}

TEST(ExternalAccount, ParseIncorrectTypeUniverseDomain) {
  auto const configuration = nlohmann::json{
      {"type", "external_account"},
      {"audience", "test-audience"},
      {"subject_token_type", "test-subject-token-type"},
      {"token_url", "test-token-url"},
      {"universe_domain", true},
      {"credential_source", nlohmann::json{{"file", "/dev/null-test-only"}}},
  };
  auto ec = internal::ErrorContext(
      {{"program", "test"}, {"full-configuration", configuration.dump()}});
  auto const actual =
      ParseExternalAccountConfiguration(configuration.dump(), ec);
  EXPECT_THAT(actual,
              StatusIs(StatusCode::kInvalidArgument,
                       HasSubstr("Invalid type for universe_domain field in "
                                 "credentials; expected string")));
}

TEST(ExternalAccount, ParseMissingCredentialSource) {
  auto const configuration = nlohmann::json{
      {"type", "external_account"},
      {"audience", "test-audience"},
      {"subject_token_type", "test-subject-token-type"},
      {"token_url", "test-token-url"},
      // {"credential_source", nlohmann::json{{"file", "/dev/null-test-only"}}},
  };
  auto ec = internal::ErrorContext(
      {{"program", "test"}, {"full-configuration", configuration.dump()}});
  auto const actual =
      ParseExternalAccountConfiguration(configuration.dump(), ec);
  EXPECT_THAT(actual, StatusIs(StatusCode::kInvalidArgument,
                               HasSubstr("missing `credential_source` field")));
}

TEST(ExternalAccount, ParseInvalidCredentialSource) {
  auto const configuration = nlohmann::json{
      {"type", "external_account"},
      {"audience", "test-audience"},
      {"subject_token_type", "test-subject-token-type"},
      {"token_url", "test-token-url"},
      {"credential_source", true},  // should be object
  };
  auto ec = internal::ErrorContext(
      {{"program", "test"}, {"full-configuration", configuration.dump()}});
  auto const actual =
      ParseExternalAccountConfiguration(configuration.dump(), ec);
  EXPECT_THAT(
      actual,
      StatusIs(StatusCode::kInvalidArgument,
               HasSubstr("`credential_source` field is not a JSON object")));
}

TEST(ExternalAccount, ParseUnknownCredentialSourceType) {
  auto const configuration = nlohmann::json{
      {"type", "external_account"},
      {"audience", "test-audience"},
      {"subject_token_type", "test-subject-token-type"},
      {"token_url", "test-token-url"},
      {"credential_source", nlohmann::json{{"environment_id", "aws1"}}},
  };
  auto ec = internal::ErrorContext(
      {{"program", "test"}, {"full-configuration", configuration.dump()}});
  auto const actual =
      ParseExternalAccountConfiguration(configuration.dump(), ec);
  EXPECT_THAT(actual, StatusIs(StatusCode::kInvalidArgument,
                               HasSubstr("unknown subject token source")));
}

TEST(ExternalAccount, ParseInvalidServiceAccountImpersonationUrl) {
  auto const configuration = nlohmann::json{
      {"type", "external_account"},
      {"audience", "test-audience"},
      {"subject_token_type", "test-subject-token-type"},
      {"token_url", "test-token-url"},
      {"credential_source", nlohmann::json{{"file", "/dev/null-test-only"}}},
      {"service_account_impersonation_url", true},  // should be string
  };
  auto ec = internal::ErrorContext(
      {{"program", "test"}, {"full-configuration", configuration.dump()}});
  auto const actual =
      ParseExternalAccountConfiguration(configuration.dump(), ec);
  EXPECT_THAT(
      actual,
      StatusIs(
          StatusCode::kInvalidArgument,
          HasSubstr(
              "invalid type for `service_account_impersonation_url` field")));
}

TEST(ExternalAccount, ParseInvalidServiceAccountLifetime) {
  auto const configuration = nlohmann::json{
      {"type", "external_account"},
      {"audience", "test-audience"},
      {"subject_token_type", "test-subject-token-type"},
      {"token_url", "test-token-url"},
      {"credential_source", nlohmann::json{{"file", "/dev/null-test-only"}}},
      {"service_account_impersonation_url", "test-impersonation-url"},
      {"service_account_impersonation",
       nlohmann::json{
           {"token_lifetime_seconds", true},  // should be numeric
       }},
  };
  auto ec = internal::ErrorContext(
      {{"program", "test"}, {"full-configuration", configuration.dump()}});
  auto const actual =
      ParseExternalAccountConfiguration(configuration.dump(), ec);
  EXPECT_THAT(
      actual,
      StatusIs(StatusCode::kInvalidArgument,
               HasSubstr("invalid type for `token_lifetime_seconds` field")));
}

auto make_expected_options = []() {
  return AllOf(
      ResultOf(
          "has TestOnlyOption",
          [](Options const& o) { return o.has<TestOnlyOption>(); }, true),
      ResultOf(
          "TestOnlyOption is `test-option`",
          [](Options const& o) { return o.get<TestOnlyOption>(); },
          std::string{"test-option"}));
};

auto make_expected_token_exchange_request = [](std::string const& path) {
  return AllOf(
      Property(&RestRequest::path, path),
      Property(&RestRequest::headers,
               Contains(Pair("content-type",
                             Contains("application/x-www-form-urlencoded")))));
};

TEST(ExternalAccount, Working) {
  auto const test_url = std::string{"https://sts.example.com/"};
  auto const expected_access_token = std::string{"test-access-token"};
  auto const expected_expires_in = std::chrono::seconds(3456);
  auto const json_response = nlohmann::json{
      {"access_token", expected_access_token},
      {"expires_in", expected_expires_in.count()},
      {"issued_token_type", "urn:ietf:params:oauth:token-type:access_token"},
      {"token_type", "Bearer"},
  };
  auto mock_source = [](HttpClientFactory const&, Options const&) {
    return make_status_or(internal::SubjectToken{"test-subject-token"});
  };
  auto const info =
      ExternalAccountInfo{"test-audience", "test-subject-token-type",
                          test_url,        mock_source,
                          absl::nullopt,   {}};

  MockClientFactory client_factory;
  EXPECT_CALL(client_factory, Call(make_expected_options())).WillOnce([&]() {
    auto mock = std::make_unique<MockRestClient>();
    auto expected_request = make_expected_token_exchange_request(test_url);
    auto expected_payload =
        MatcherCast<FormDataType const&>(UnorderedElementsAre(
            Pair("grant_type",
                 "urn:ietf:params:oauth:grant-type:token-exchange"),
            Pair("requested_token_type",
                 "urn:ietf:params:oauth:token-type:access_token"),
            Pair("scope", "https://www.googleapis.com/auth/cloud-platform"),
            Pair("audience", "test-audience"),
            Pair("subject_token_type", "test-subject-token-type"),
            Pair("subject_token", "test-subject-token")));
    EXPECT_CALL(*mock, Post(_, expected_request, expected_payload))
        .WillOnce(
            Return(ByMove(MakeMockResponseSuccess(json_response.dump()))));
    return mock;
  });

  auto credentials =
      ExternalAccountCredentials(info, client_factory.AsStdFunction(),
                                 Options{}.set<TestOnlyOption>("test-option"));
  auto const now = std::chrono::system_clock::now();
  auto access_token = credentials.GetToken(now);
  ASSERT_STATUS_OK(access_token);
  EXPECT_EQ(access_token->expiration, now + expected_expires_in);
  EXPECT_EQ(access_token->token, expected_access_token);
}

TEST(ExternalAccount, WorkingWithImpersonation) {
  auto const sts_test_url = std::string{"https://sts.example.com/"};
  auto const sts_access_token = std::string{"test-sts-access-token"};
  auto const sts_expires_in = std::chrono::seconds(3456);
  auto const sts_payload = nlohmann::json{
      {"access_token", sts_access_token},
      {"expires_in", sts_expires_in.count()},
      {"issued_token_type", "urn:ietf:params:oauth:token-type:access_token"},
      {"token_type", "Bearer"},
  };
  auto const impersonate_test_url =
      std::string{"https://iamcredentials.example.com/test-account"};
  auto const impersonate_test_lifetime = std::chrono::seconds(2345);
  auto const impersonate_access_token = std::string{"test-access-token"};
  auto const impersonate_request_payload = nlohmann::json{
      {"delegates", nlohmann::json::array()},
      {"scope", nlohmann::json::array({GoogleOAuthScopeCloudPlatform()})},
      {"lifetime", std::to_string(impersonate_test_lifetime.count()) + "s"}};
  auto const impersonate_request_payload_dump =
      impersonate_request_payload.dump();
  auto const impersonate_expires_in = std::chrono::seconds(1234);
  auto const now = std::chrono::system_clock::now();
  auto const impersonate_expire_time = now + impersonate_expires_in;
  auto const impersonate_response_payload = nlohmann::json{
      {"accessToken", impersonate_access_token},
      {"expireTime",
       absl::FormatTime(absl::FromChrono(impersonate_expire_time))},
  };
  auto mock_source = [](HttpClientFactory const&, Options const&) {
    return make_status_or(internal::SubjectToken{"test-subject-token"});
  };
  auto const info =
      ExternalAccountInfo{"test-audience",
                          "test-subject-token-type",
                          sts_test_url,
                          mock_source,
                          ExternalAccountImpersonationConfig{
                              impersonate_test_url, impersonate_test_lifetime},
                          {}};

  auto sts_client = [&] {
    auto expected_sts_request = Property(&RestRequest::path, sts_test_url);
    auto expected_form_data =
        MatcherCast<FormDataType const&>(UnorderedElementsAre(
            Pair("grant_type",
                 "urn:ietf:params:oauth:grant-type:token-exchange"),
            Pair("requested_token_type",
                 "urn:ietf:params:oauth:token-type:access_token"),
            Pair("scope", "https://www.googleapis.com/auth/cloud-platform"),
            Pair("audience", "test-audience"),
            Pair("subject_token_type", "test-subject-token-type"),
            Pair("subject_token", "test-subject-token")));
    auto sts_response = MakeMockResponseSuccess(sts_payload.dump());
    auto mock = std::make_unique<MockRestClient>();
    EXPECT_CALL(*mock, Post(_, expected_sts_request, expected_form_data))
        .WillOnce(Return(ByMove(std::move(sts_response))));
    return mock;
  }();

  auto impersonate_client = [&] {
    auto expected_impersonate_request = AllOf(
        Property(&RestRequest::path, impersonate_test_url),
        Property(&RestRequest::headers,
                 Contains(Pair("authorization",
                               Contains("Bearer " + sts_access_token)))),
        Property(&RestRequest::headers,
                 Contains(Pair("content-type", Contains("application/json")))));
    auto expected_impersonate_payload =
        MatcherCast<std::vector<absl::Span<char const>> const&>(ElementsAre(
            absl::Span<char const>(impersonate_request_payload_dump)));
    auto impersonate_response =
        MakeMockResponseSuccess(impersonate_response_payload.dump());
    auto mock = std::make_unique<MockRestClient>();
    EXPECT_CALL(*mock, Post(_, expected_impersonate_request,
                            expected_impersonate_payload))
        .WillOnce(Return(ByMove(std::move(impersonate_response))));
    return mock;
  }();

  MockClientFactory client_factory;
  EXPECT_CALL(client_factory, Call(make_expected_options()))
      .WillOnce(Return(ByMove(std::move(sts_client))))
      .WillOnce(Return(ByMove(std::move(impersonate_client))));

  auto credentials =
      ExternalAccountCredentials(info, client_factory.AsStdFunction(),
                                 Options{}.set<TestOnlyOption>("test-option"));
  auto access_token = credentials.GetToken(now);
  ASSERT_STATUS_OK(access_token);
  EXPECT_EQ(access_token->expiration, impersonate_expire_time);
  EXPECT_EQ(access_token->token, impersonate_access_token);
}

TEST(ExternalAccount, HandleHttpError) {
  auto const test_url = std::string{"https://sts.example.com/"};
  auto const expected_access_token = std::string{"test-access-token"};
  auto const expected_expires_in = std::chrono::seconds(3456);
  auto const json_response = nlohmann::json{
      {"access_token", expected_access_token},
      {"expected_expires_in", expected_expires_in.count()},
      {"issued_token_type", "urn:ietf:params:oauth:token-type:access_token"},
      {"token_type", "Bearer"},
  };
  auto mock_source = [](HttpClientFactory const&, Options const&) {
    return make_status_or(internal::SubjectToken{"test-subject-token"});
  };
  auto const info =
      ExternalAccountInfo{"test-audience", "test-subject-token-type",
                          test_url,        mock_source,
                          absl::nullopt,   {}};
  MockClientFactory client_factory;
  EXPECT_CALL(client_factory, Call).WillOnce([&]() {
    auto mock = std::make_unique<MockRestClient>();
    auto expected_request = make_expected_token_exchange_request(test_url);
    auto expected_form_data =
        MatcherCast<FormDataType const&>(UnorderedElementsAre(
            Pair("grant_type",
                 "urn:ietf:params:oauth:grant-type:token-exchange"),
            Pair("requested_token_type",
                 "urn:ietf:params:oauth:token-type:access_token"),
            Pair("scope", "https://www.googleapis.com/auth/cloud-platform"),
            Pair("audience", "test-audience"),
            Pair("subject_token_type", "test-subject-token-type"),
            Pair("subject_token", "test-subject-token")));

    EXPECT_CALL(*mock, Post(_, expected_request, expected_form_data))
        .WillOnce(Return(ByMove(MakeMockResponseError())));
    return mock;
  });

  auto credentials =
      ExternalAccountCredentials(info, client_factory.AsStdFunction());
  auto const now = std::chrono::system_clock::now();
  auto access_token = credentials.GetToken(now);
  EXPECT_THAT(access_token, StatusIs(StatusCode::kNotFound));
}

TEST(ExternalAccount, HandleHttpPartialError) {
  auto const test_url = std::string{"https://sts.example.com/"};
  auto const expected_access_token = std::string{"test-access-token"};
  auto const response = std::string{R"""({"access_token": "1234--uh-oh)"""};
  auto mock_source = [](HttpClientFactory const&, Options const&) {
    return make_status_or(internal::SubjectToken{"test-subject-token"});
  };
  auto const info =
      ExternalAccountInfo{"test-audience", "test-subject-token-type",
                          test_url,        mock_source,
                          absl::nullopt,   {}};
  MockClientFactory client_factory;
  EXPECT_CALL(client_factory, Call).WillOnce([&]() {
    auto mock = std::make_unique<MockRestClient>();
    auto expected_request = make_expected_token_exchange_request(test_url);
    auto expected_form_data =
        MatcherCast<FormDataType const&>(UnorderedElementsAre(
            Pair("grant_type",
                 "urn:ietf:params:oauth:grant-type:token-exchange"),
            Pair("requested_token_type",
                 "urn:ietf:params:oauth:token-type:access_token"),
            Pair("scope", "https://www.googleapis.com/auth/cloud-platform"),
            Pair("audience", "test-audience"),
            Pair("subject_token_type", "test-subject-token-type"),
            Pair("subject_token", "test-subject-token")));

    EXPECT_CALL(*mock, Post(_, expected_request, expected_form_data))
        .WillOnce(Return(ByMove(MakeMockResponsePartialError(response))));
    return mock;
  });

  auto credentials =
      ExternalAccountCredentials(info, client_factory.AsStdFunction());
  auto const now = std::chrono::system_clock::now();
  auto access_token = credentials.GetToken(now);
  EXPECT_THAT(access_token,
              StatusIs(StatusCode::kUnavailable, HasSubstr("read error")));
}

TEST(ExternalAccount, HandleNotJson) {
  auto const test_url = std::string{"https://sts.example.com/"};
  auto const expected_access_token = std::string{"test-access-token"};
  auto const payload = std::string{R"""("abc--unterminated)"""};
  auto mock_source = [](HttpClientFactory const&, Options const&) {
    return make_status_or(internal::SubjectToken{"test-subject-token"});
  };
  auto const info =
      ExternalAccountInfo{"test-audience", "test-subject-token-type",
                          test_url,        mock_source,
                          absl::nullopt,   {}};
  MockClientFactory client_factory;
  EXPECT_CALL(client_factory, Call).WillOnce([&]() {
    auto mock = std::make_unique<MockRestClient>();
    auto expected_request = make_expected_token_exchange_request(test_url);
    auto expected_payload =
        MatcherCast<FormDataType const&>(UnorderedElementsAre(
            Pair("grant_type",
                 "urn:ietf:params:oauth:grant-type:token-exchange"),
            Pair("requested_token_type",
                 "urn:ietf:params:oauth:token-type:access_token"),
            Pair("scope", "https://www.googleapis.com/auth/cloud-platform"),
            Pair("audience", "test-audience"),
            Pair("subject_token_type", "test-subject-token-type"),
            Pair("subject_token", "test-subject-token")));
    EXPECT_CALL(*mock, Post(_, expected_request, expected_payload))
        .WillOnce(Return(ByMove(MakeMockResponseSuccess(payload))));
    return mock;
  });

  auto credentials =
      ExternalAccountCredentials(info, client_factory.AsStdFunction());
  auto const now = std::chrono::system_clock::now();
  auto access_token = credentials.GetToken(now);
  EXPECT_THAT(access_token,
              StatusIs(StatusCode::kInvalidArgument,
                       HasSubstr("cannot be parsed as JSON object")));
}

TEST(ExternalAccount, HandleNotJsonObject) {
  auto const test_url = std::string{"https://sts.example.com/"};
  auto const expected_access_token = std::string{"test-access-token"};
  auto const payload = std::string{R"""("json-string-is-not-object")"""};
  auto mock_source = [](HttpClientFactory const&, Options const&) {
    return make_status_or(internal::SubjectToken{"test-subject-token"});
  };
  auto const info =
      ExternalAccountInfo{"test-audience", "test-subject-token-type",
                          test_url,        mock_source,
                          absl::nullopt,   {}};
  MockClientFactory client_factory;
  EXPECT_CALL(client_factory, Call).WillOnce([&]() {
    auto mock = std::make_unique<MockRestClient>();
    auto expected_request = make_expected_token_exchange_request(test_url);
    auto expected_payload =
        MatcherCast<FormDataType const&>(UnorderedElementsAre(
            Pair("grant_type",
                 "urn:ietf:params:oauth:grant-type:token-exchange"),
            Pair("requested_token_type",
                 "urn:ietf:params:oauth:token-type:access_token"),
            Pair("scope", "https://www.googleapis.com/auth/cloud-platform"),
            Pair("audience", "test-audience"),
            Pair("subject_token_type", "test-subject-token-type"),
            Pair("subject_token", "test-subject-token")));
    EXPECT_CALL(*mock, Post(_, expected_request, expected_payload))
        .WillOnce(Return(ByMove(MakeMockResponseSuccess(payload))));
    return mock;
  });

  auto credentials =
      ExternalAccountCredentials(info, client_factory.AsStdFunction());
  auto const now = std::chrono::system_clock::now();
  auto access_token = credentials.GetToken(now);
  EXPECT_THAT(access_token,
              StatusIs(StatusCode::kInvalidArgument,
                       HasSubstr("cannot be parsed as JSON object")));
}

TEST(ExternalAccount, MissingToken) {
  auto const test_url = std::string{"https://sts.example.com/"};
  auto const expected_access_token = std::string{"test-access-token"};
  auto const expected_expires_in = std::chrono::seconds(3456);
  auto const json_response = nlohmann::json{
      // {"access_token", expected_access_token},
      {"expires_in", expected_expires_in.count()},
      {"issued_token_type", "urn:ietf:params:oauth:token-type:access_token"},
      {"token_type", "Bearer"},
  };
  auto mock_source = [](HttpClientFactory const&, Options const&) {
    return make_status_or(internal::SubjectToken{"test-subject-token"});
  };
  auto const info =
      ExternalAccountInfo{"test-audience", "test-subject-token-type",
                          test_url,        mock_source,
                          absl::nullopt,   {}};
  MockClientFactory client_factory;
  EXPECT_CALL(client_factory, Call).WillOnce([&]() {
    auto mock = std::make_unique<MockRestClient>();
    EXPECT_CALL(*mock, Post(_, _, An<FormDataType const&>()))
        .WillOnce(
            Return(ByMove(MakeMockResponseSuccess(json_response.dump()))));
    return mock;
  });

  auto credentials =
      ExternalAccountCredentials(info, client_factory.AsStdFunction());
  auto const now = std::chrono::system_clock::now();
  auto access_token = credentials.GetToken(now);
  EXPECT_THAT(access_token, StatusIs(StatusCode::kInvalidArgument));
  EXPECT_THAT(access_token.status().error_info().domain(), "gcloud-cpp");
}

TEST(ExternalAccount, MissingIssuedTokenType) {
  auto const test_url = std::string{"https://sts.example.com/"};
  auto const expected_access_token = std::string{"test-access-token"};
  auto const expected_expires_in = std::chrono::seconds(3456);
  auto const json_response = nlohmann::json{
      {"access_token", expected_access_token},
      {"expires_in", expected_expires_in.count()},
      // {"issued_token_type", "urn:ietf:params:oauth:token-type:access_token"},
      {"token_type", "Bearer"},
  };
  auto mock_source = [](HttpClientFactory const&, Options const&) {
    return make_status_or(internal::SubjectToken{"test-subject-token"});
  };
  auto const info =
      ExternalAccountInfo{"test-audience", "test-subject-token-type",
                          test_url,        mock_source,
                          absl::nullopt,   {}};
  MockClientFactory client_factory;
  EXPECT_CALL(client_factory, Call).WillOnce([&]() {
    auto mock = std::make_unique<MockRestClient>();
    EXPECT_CALL(*mock, Post(_, _, An<FormDataType const&>()))
        .WillOnce(
            Return(ByMove(MakeMockResponseSuccess(json_response.dump()))));
    return mock;
  });

  auto credentials =
      ExternalAccountCredentials(info, client_factory.AsStdFunction());
  auto const now = std::chrono::system_clock::now();
  auto access_token = credentials.GetToken(now);
  EXPECT_THAT(access_token, StatusIs(StatusCode::kInvalidArgument));
  EXPECT_THAT(access_token.status().error_info().domain(), "gcloud-cpp");
}

TEST(ExternalAccount, MissingTokenType) {
  auto const test_url = std::string{"https://sts.example.com/"};
  auto const expected_access_token = std::string{"test-access-token"};
  auto const expected_expires_in = std::chrono::seconds(3456);
  auto const json_response = nlohmann::json{
      {"access_token", expected_access_token},
      {"expires_in", expected_expires_in.count()},
      {"issued_token_type", "urn:ietf:params:oauth:token-type:access_token"},
      // {"token_type", "Bearer"},
  };
  auto mock_source = [](HttpClientFactory const&, Options const&) {
    return make_status_or(internal::SubjectToken{"test-subject-token"});
  };
  auto const info =
      ExternalAccountInfo{"test-audience", "test-subject-token-type",
                          test_url,        mock_source,
                          absl::nullopt,   {}};
  MockClientFactory client_factory;
  EXPECT_CALL(client_factory, Call).WillOnce([&]() {
    auto mock = std::make_unique<MockRestClient>();
    EXPECT_CALL(*mock, Post(_, _, An<FormDataType const&>()))
        .WillOnce(
            Return(ByMove(MakeMockResponseSuccess(json_response.dump()))));
    return mock;
  });

  auto credentials =
      ExternalAccountCredentials(info, client_factory.AsStdFunction());
  auto const now = std::chrono::system_clock::now();
  auto access_token = credentials.GetToken(now);
  EXPECT_THAT(access_token, StatusIs(StatusCode::kInvalidArgument));
  EXPECT_THAT(access_token.status().error_info().domain(), "gcloud-cpp");
}

TEST(ExternalAccount, InvalidIssuedTokenType) {
  auto const test_url = std::string{"https://sts.example.com/"};
  auto const expected_access_token = std::string{"test-access-token"};
  auto const expected_expires_in = std::chrono::seconds(3456);
  auto const json_response = nlohmann::json{
      {"access_token", expected_access_token},
      {"expires_in", expected_expires_in.count()},
      {"issued_token_type", "--invalid--"},
      {"token_type", "Bearer"},
  };
  auto mock_source = [](HttpClientFactory const&, Options const&) {
    return make_status_or(internal::SubjectToken{"test-subject-token"});
  };
  auto const info =
      ExternalAccountInfo{"test-audience", "test-subject-token-type",
                          test_url,        mock_source,
                          absl::nullopt,   {}};
  MockClientFactory client_factory;
  EXPECT_CALL(client_factory, Call).WillOnce([&]() {
    auto mock = std::make_unique<MockRestClient>();
    EXPECT_CALL(*mock, Post(_, _, An<FormDataType const&>()))
        .WillOnce(
            Return(ByMove(MakeMockResponseSuccess(json_response.dump()))));
    return mock;
  });

  auto credentials =
      ExternalAccountCredentials(info, client_factory.AsStdFunction());
  auto const now = std::chrono::system_clock::now();
  auto access_token = credentials.GetToken(now);
  EXPECT_THAT(access_token,
              StatusIs(StatusCode::kInvalidArgument,
                       HasSubstr("expected a Bearer access token")));
  EXPECT_THAT(access_token.status().error_info().domain(), "gcloud-cpp");
}

TEST(ExternalAccount, InvalidTokenType) {
  auto const test_url = std::string{"https://sts.example.com/"};
  auto const expected_access_token = std::string{"test-access-token"};
  auto const expected_expires_in = std::chrono::seconds(3456);
  auto const json_response = nlohmann::json{
      {"access_token", expected_access_token},
      {"expires_in", expected_expires_in.count()},
      {"issued_token_type", "urn:ietf:params:oauth:token-type:access_token"},
      {"token_type", "--invalid--"},
  };
  auto mock_source = [](HttpClientFactory const&, Options const&) {
    return make_status_or(internal::SubjectToken{"test-subject-token"});
  };
  auto const info =
      ExternalAccountInfo{"test-audience", "test-subject-token-type",
                          test_url,        mock_source,
                          absl::nullopt,   {}};
  MockClientFactory client_factory;
  EXPECT_CALL(client_factory, Call).WillOnce([&]() {
    auto mock = std::make_unique<MockRestClient>();
    EXPECT_CALL(*mock, Post(_, _, An<FormDataType const&>()))
        .WillOnce(
            Return(ByMove(MakeMockResponseSuccess(json_response.dump()))));
    return mock;
  });

  auto credentials =
      ExternalAccountCredentials(info, client_factory.AsStdFunction());
  auto const now = std::chrono::system_clock::now();
  auto access_token = credentials.GetToken(now);
  EXPECT_THAT(access_token,
              StatusIs(StatusCode::kInvalidArgument,
                       HasSubstr("expected a Bearer access token")));
  EXPECT_THAT(access_token.status().error_info().domain(), "gcloud-cpp");
}

TEST(ExternalAccount, MissingExpiresIn) {
  auto const test_url = std::string{"https://sts.example.com/"};
  auto const expected_access_token = std::string{"test-access-token"};
  auto const expected_expires_in = std::chrono::seconds(3456);
  auto const json_response = nlohmann::json{
      {"access_token", expected_access_token},
      {"invalid-expires_in", expected_expires_in.count()},
      {"issued_token_type", "urn:ietf:params:oauth:token-type:access_token"},
      {"token_type", "Bearer"},
      // {"expires_in", 3500},
  };
  auto mock_source = [](HttpClientFactory const&, Options const&) {
    return make_status_or(internal::SubjectToken{"test-subject-token"});
  };
  auto const info =
      ExternalAccountInfo{"test-audience", "test-subject-token-type",
                          test_url,        mock_source,
                          absl::nullopt,   {}};
  MockClientFactory client_factory;
  EXPECT_CALL(client_factory, Call).WillOnce([&]() {
    auto mock = std::make_unique<MockRestClient>();
    EXPECT_CALL(*mock, Post(_, _, An<FormDataType const&>()))
        .WillOnce(
            Return(ByMove(MakeMockResponseSuccess(json_response.dump()))));
    return mock;
  });

  auto credentials =
      ExternalAccountCredentials(info, client_factory.AsStdFunction());
  auto const now = std::chrono::system_clock::now();
  auto access_token = credentials.GetToken(now);
  EXPECT_THAT(access_token,
              StatusIs(StatusCode::kInvalidArgument,
                       HasSubstr("cannot find `expires_in` field")));
  EXPECT_THAT(access_token.status().error_info().domain(), "gcloud-cpp");
}

TEST(ExternalAccount, InvalidExpiresIn) {
  auto const test_url = std::string{"https://sts.example.com/"};
  auto const expected_access_token = std::string{"test-access-token"};
  auto const json_response = nlohmann::json{
      {"access_token", expected_access_token},
      {"expires_in", "--invalid--"},
      {"issued_token_type", "urn:ietf:params:oauth:token-type:access_token"},
      {"token_type", "Bearer"},
  };
  auto mock_source = [](HttpClientFactory const&, Options const&) {
    return make_status_or(internal::SubjectToken{"test-subject-token"});
  };
  auto const info =
      ExternalAccountInfo{"test-audience", "test-subject-token-type",
                          test_url,        mock_source,
                          absl::nullopt,   {}};
  MockClientFactory client_factory;
  EXPECT_CALL(client_factory, Call).WillOnce([&]() {
    auto mock = std::make_unique<MockRestClient>();
    EXPECT_CALL(*mock, Post(_, _, An<FormDataType const&>()))
        .WillOnce(
            Return(ByMove(MakeMockResponseSuccess(json_response.dump()))));
    return mock;
  });

  auto credentials =
      ExternalAccountCredentials(info, client_factory.AsStdFunction());
  auto const now = std::chrono::system_clock::now();
  auto access_token = credentials.GetToken(now);
  EXPECT_THAT(access_token,
              StatusIs(StatusCode::kInvalidArgument,
                       HasSubstr("invalid type for `expires_in` field")));
  EXPECT_THAT(access_token.status().error_info().domain(), "gcloud-cpp");
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace oauth2_internal
}  // namespace cloud
}  // namespace google
