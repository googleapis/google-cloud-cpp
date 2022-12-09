// Copyright 2021 Google LLC
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

#include "google/cloud/internal/unified_rest_credentials.h"
#include "google/cloud/internal/absl_str_cat_quiet.h"
#include "google/cloud/internal/credentials_impl.h"
#include "google/cloud/internal/filesystem.h"
#include "google/cloud/internal/oauth2_google_application_default_credentials_file.h"
#include "google/cloud/internal/oauth2_service_account_credentials.h"
#include "google/cloud/internal/random.h"
#include "google/cloud/testing_util/chrono_output.h"
#include "google/cloud/testing_util/mock_http_payload.h"
#include "google/cloud/testing_util/mock_rest_client.h"
#include "google/cloud/testing_util/mock_rest_response.h"
#include "google/cloud/testing_util/scoped_environment.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>
#include <nlohmann/json.hpp>
#include <cstdlib>
#include <fstream>
#include <random>

namespace google {
namespace cloud {
namespace rest_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::google::cloud::MakeAccessTokenCredentials;
using ::google::cloud::MakeGoogleDefaultCredentials;
using ::google::cloud::MakeInsecureCredentials;
using ::google::cloud::testing_util::IsOk;
using ::google::cloud::testing_util::MakeMockHttpPayloadSuccess;
using ::google::cloud::testing_util::MockRestClient;
using ::google::cloud::testing_util::MockRestResponse;
using ::google::cloud::testing_util::ScopedEnvironment;
using ::google::cloud::testing_util::StatusIs;
using ::testing::A;
using ::testing::AtMost;
using ::testing::ByMove;
using ::testing::Contains;
using ::testing::Eq;
using ::testing::HasSubstr;
using ::testing::IsEmpty;
using ::testing::IsSupersetOf;
using ::testing::MatcherCast;
using ::testing::Pair;
using ::testing::Property;
using ::testing::Return;

using MockClientFactory =
    ::testing::MockFunction<std::unique_ptr<rest_internal::RestClient>(
        Options const&)>;

// Create a loadable, i.e., syntactically valid, key file, load it, and it
// has the right contents.
auto constexpr kServiceAccountKeyId = "test-only-key-id";
auto constexpr kServiceAccountEmail =
    "sa@invalid-test-only-project.iam.gserviceaccount.com";

// This is an invalidated private key. It was created using the Google Cloud
// Platform console, but then the key (and service account) were deleted.
auto constexpr kWellFormattedKey = R"""(-----BEGIN PRIVATE KEY-----
MIIEvQIBADANBgkqhkiG9w0BAQEFAASCBKcwggSjAgEAAoIBAQCltiF2oP3KJJ+S
tTc1McylY+TuAi3AdohX7mmqIjd8a3eBYDHs7FlnUrFC4CRijCr0rUqYfg2pmk4a
6TaKbQRAhWDJ7XD931g7EBvCtd8+JQBNWVKnP9ByJUaO0hWVniM50KTsWtyX3up/
fS0W2R8Cyx4yvasE8QHH8gnNGtr94iiORDC7De2BwHi/iU8FxMVJAIyDLNfyk0hN
eheYKfIDBgJV2v6VaCOGWaZyEuD0FJ6wFeLybFBwibrLIBE5Y/StCrZoVZ5LocFP
T4o8kT7bU6yonudSCyNMedYmqHj/iF8B2UN1WrYx8zvoDqZk0nxIglmEYKn/6U7U
gyETGcW9AgMBAAECggEAC231vmkpwA7JG9UYbviVmSW79UecsLzsOAZnbtbn1VLT
Pg7sup7tprD/LXHoyIxK7S/jqINvPU65iuUhgCg3Rhz8+UiBhd0pCH/arlIdiPuD
2xHpX8RIxAq6pGCsoPJ0kwkHSw8UTnxPV8ZCPSRyHV71oQHQgSl/WjNhRi6PQroB
Sqc/pS1m09cTwyKQIopBBVayRzmI2BtBxyhQp9I8t5b7PYkEZDQlbdq0j5Xipoov
9EW0+Zvkh1FGNig8IJ9Wp+SZi3rd7KLpkyKPY7BK/g0nXBkDxn019cET0SdJOHQG
DiHiv4yTRsDCHZhtEbAMKZEpku4WxtQ+JjR31l8ueQKBgQDkO2oC8gi6vQDcx/CX
Z23x2ZUyar6i0BQ8eJFAEN+IiUapEeCVazuxJSt4RjYfwSa/p117jdZGEWD0GxMC
+iAXlc5LlrrWs4MWUc0AHTgXna28/vii3ltcsI0AjWMqaybhBTTNbMFa2/fV2OX2
UimuFyBWbzVc3Zb9KAG4Y7OmJQKBgQC5324IjXPq5oH8UWZTdJPuO2cgRsvKmR/r
9zl4loRjkS7FiOMfzAgUiXfH9XCnvwXMqJpuMw2PEUjUT+OyWjJONEK4qGFJkbN5
3ykc7p5V7iPPc7Zxj4mFvJ1xjkcj+i5LY8Me+gL5mGIrJ2j8hbuv7f+PWIauyjnp
Nx/0GVFRuQKBgGNT4D1L7LSokPmFIpYh811wHliE0Fa3TDdNGZnSPhaD9/aYyy78
LkxYKuT7WY7UVvLN+gdNoVV5NsLGDa4cAV+CWPfYr5PFKGXMT/Wewcy1WOmJ5des
AgMC6zq0TdYmMBN6WpKUpEnQtbmh3eMnuvADLJWxbH3wCkg+4xDGg2bpAoGAYRNk
MGtQQzqoYNNSkfus1xuHPMA8508Z8O9pwKU795R3zQs1NAInpjI1sOVrNPD7Ymwc
W7mmNzZbxycCUL/yzg1VW4P1a6sBBYGbw1SMtWxun4ZbnuvMc2CTCh+43/1l+FHe
Mmt46kq/2rH2jwx5feTbOE6P6PINVNRJh/9BDWECgYEAsCWcH9D3cI/QDeLG1ao7
rE2NcknP8N783edM07Z/zxWsIsXhBPY3gjHVz2LDl+QHgPWhGML62M0ja/6SsJW3
YvLLIc82V7eqcVJTZtaFkuht68qu/Jn1ezbzJMJ4YXDYo1+KFi+2CAGR06QILb+I
lUtj+/nH3HDQjM4ltYfTPUg=
-----END PRIVATE KEY-----
)""";

std::string TempKeyFileName() {
  static auto generator =
      google::cloud::internal::DefaultPRNG(std::random_device{}());
  return google::cloud::internal::PathAppend(
      ::testing::TempDir(),
      ::google::cloud::internal::Sample(
          generator, 16, "abcdefghijlkmnopqrstuvwxyz0123456789") +
          ".json");
}

std::unique_ptr<RestResponse> MakeMockResponse(std::string contents) {
  auto response = absl::make_unique<MockRestResponse>();
  EXPECT_CALL(*response, StatusCode)
      .WillRepeatedly(Return(HttpStatusCode::kOk));
  EXPECT_CALL(std::move(*response), ExtractPayload)
      .Times(AtMost(1))
      .WillRepeatedly([contents = std::move(contents)]() mutable {
        return MakeMockHttpPayloadSuccess(std::move(contents));
      });
  return response;
}

nlohmann::json MakeServiceAccountContents() {
  return nlohmann::json{
      {"type", "service_account"},
      {"project_id", "invalid-test-only-project"},
      {"private_key_id", kServiceAccountKeyId},
      {"private_key", kWellFormattedKey},
      {"client_email", kServiceAccountEmail},
      {"client_id", "invalid-test-only-client-id"},
      {"auth_uri", "https://accounts.google.com/o/oauth2/auth"},
      {"token_uri", "https://accounts.google.com/o/oauth2/token"},
      {"auth_provider_x509_cert_url",
       "https://www.googleapis.com/oauth2/v1/certs"},
      {"client_x509_cert_url",
       "https://www.googleapis.com/robot/v1/metadata/x509/"
       "foo-email%40invalid-test-only-project.iam.gserviceaccount.com"},
  };
}

ScopedEnvironment SetUpAdcFile(std::string const& filename,
                               std::string const& contents) {
  std::ofstream(filename) << contents;
  return ScopedEnvironment(oauth2_internal::GoogleAdcEnvVar(),
                           filename.c_str());
}

// Generally, these tests verify that the right type of credentials was created
// by observing what HTTP requests they make. In general, the tests just return
// an error. There are tests for each class that verify the success case.

TEST(UnifiedRestCredentialsTest, Insecure) {
  auto credentials = MapCredentials(MakeInsecureCredentials());
  auto token = credentials->GetToken(std::chrono::system_clock::now());
  ASSERT_THAT(token, IsOk());
  EXPECT_THAT(token->token, IsEmpty());
}

TEST(UnifiedRestCredentialsTest, AdcIsServiceAccount) {
  auto const expected_expires_in = std::chrono::seconds(3600);
  auto const contents = MakeServiceAccountContents();

  auto const now = std::chrono::system_clock::now();
  auto info =
      oauth2_internal::ParseServiceAccountCredentials(contents.dump(), "test");
  ASSERT_STATUS_OK(info);
  auto const jwt = oauth2_internal::MakeSelfSignedJWT(*info, now);
  ASSERT_STATUS_OK(jwt);

  MockClientFactory client_factory;
  EXPECT_CALL(client_factory, Call).Times(0);

  auto const filename = TempKeyFileName();
  auto const env = SetUpAdcFile(filename, contents.dump());
  auto config =
      std::make_shared<internal::GoogleDefaultCredentialsConfig>(Options{});
  auto credentials = MapCredentials(config, client_factory.AsStdFunction());
  (void)std::remove(filename.c_str());

  auto access_token = credentials->GetToken(now);
  ASSERT_STATUS_OK(access_token);
  EXPECT_EQ(access_token->expiration, now + expected_expires_in);
  EXPECT_EQ(access_token->token, *jwt);
}

TEST(UnifiedRestCredentialsTest, AdcIsAuthorizedUser) {
  auto const token_uri = std::string{"https://user-refresh.example.com"};
  auto const contents = nlohmann::json{
      {"client_id", "a-client-id.example.com"},
      {"client_secret", "a-123456ABCDEF"},
      {"refresh_token", "1/THETOKEN"},
      {"type", "authorized_user"},
      {"token_uri", token_uri},
  };

  auto const now = std::chrono::system_clock::now();

  MockClientFactory client_factory;
  EXPECT_CALL(client_factory, Call).WillOnce([token_uri]() {
    auto client = absl::make_unique<MockRestClient>();
    using FormDataType = std::vector<std::pair<std::string, std::string>>;
    auto expected_request = Property(&RestRequest::path, token_uri);
    auto expected_form_data = MatcherCast<FormDataType const&>(IsSupersetOf({
        Pair("grant_type", "refresh_token"),
        Pair("client_id", "a-client-id.example.com"),
        Pair("client_secret", "a-123456ABCDEF"),
        Pair("refresh_token", "1/THETOKEN"),
    }));
    EXPECT_CALL(*client, Post(expected_request, expected_form_data))
        .WillOnce(Return(
            Status{StatusCode::kPermissionDenied, "uh-oh - user refresh"}));
    return client;
  });

  auto const filename = TempKeyFileName();
  auto const env = SetUpAdcFile(filename, contents.dump());
  auto config =
      std::make_shared<internal::GoogleDefaultCredentialsConfig>(Options{});
  auto credentials = MapCredentials(config, client_factory.AsStdFunction());
  (void)std::remove(filename.c_str());

  auto access_token = credentials->GetToken(now);
  EXPECT_THAT(access_token,
              StatusIs(StatusCode::kPermissionDenied, "uh-oh - user refresh"));
}

TEST(UnifiedRestCredentialsTest, AdcIsComputeEngine) {
  auto const filename = TempKeyFileName();
  auto const env =
      ScopedEnvironment(oauth2_internal::GoogleAdcEnvVar(), absl::nullopt);
  auto const override_default_path =
      ScopedEnvironment(oauth2_internal::GoogleGcloudAdcFileEnvVar(), filename);
  auto const now = std::chrono::system_clock::now();

  auto metadata_client = []() {
    auto client = absl::make_unique<MockRestClient>();
    auto expected_request = AllOf(
        Property(&RestRequest::path,
                 absl::StrCat("http://metadata.google.internal/",
                              "computeMetadata/v1/instance/service-accounts/",
                              "default/")),
        Property(&RestRequest::headers,
                 Contains(Pair("metadata-flavor", Contains("Google")))));
    EXPECT_CALL(*client, Get(expected_request))
        .WillOnce(Return(
            Status{StatusCode::kPermissionDenied, "uh-oh - GCE metadata"}));
    return client;
  }();
  auto token_client = []() {
    auto client = absl::make_unique<MockRestClient>();
    auto expected_request = AllOf(
        Property(&RestRequest::path,
                 absl::StrCat("http://metadata.google.internal/",
                              "computeMetadata/v1/instance/service-accounts/",
                              "default/", "token")),
        Property(&RestRequest::headers,
                 Contains(Pair("metadata-flavor", Contains("Google")))));
    EXPECT_CALL(*client, Get(expected_request))
        .WillOnce(
            Return(Status{StatusCode::kPermissionDenied, "uh-oh - GCE token"}));
    return client;
  }();

  MockClientFactory client_factory;
  EXPECT_CALL(client_factory, Call)
      .WillOnce(Return(ByMove(std::move(metadata_client))))
      .WillOnce(Return(ByMove(std::move(token_client))));

  auto config =
      std::make_shared<internal::GoogleDefaultCredentialsConfig>(Options{});
  auto credentials = MapCredentials(config, client_factory.AsStdFunction());

  auto access_token = credentials->GetToken(now);
  EXPECT_THAT(access_token,
              StatusIs(StatusCode::kPermissionDenied, "uh-oh - GCE token"));
}

TEST(UnifiedRestCredentialsTest, AccessToken) {
  auto const now = std::chrono::system_clock::now();
  auto const expiration = now + std::chrono::seconds(1800);
  auto credentials =
      MapCredentials(MakeAccessTokenCredentials("token1", expiration));
  auto token = credentials->GetToken(now);
  ASSERT_THAT(token, IsOk());
  EXPECT_THAT(token->token, Eq("token1"));
  EXPECT_THAT(token->expiration, Eq(expiration));
}

TEST(UnifiedRestCredentialsTest, ImpersonateServiceAccount) {
  auto const contents = MakeServiceAccountContents();

  // We will simply simulate a failure.
  MockClientFactory client_factory;
  EXPECT_CALL(client_factory, Call).WillOnce([] {
    auto client = absl::make_unique<MockRestClient>();
    auto expected_request = AllOf(
        Property(&RestRequest::path,
                 absl::StrCat("https://iamcredentials.googleapis.com/v1/",
                              "projects/-/serviceAccounts/",
                              kServiceAccountEmail, ":generateAccessToken")),
        Property(&RestRequest::headers,
                 Contains(Pair("authorization",
                               Contains("Bearer base-access-token")))));
    using PayloadType = std::vector<absl::Span<char const>>;
    EXPECT_CALL(*client, Post(expected_request, A<PayloadType const&>()))
        .WillOnce(Return(Status{StatusCode::kPermissionDenied,
                                "uh-oh - cannot impersonate"}));
    return client;
  });

  auto const now = std::chrono::system_clock::now();
  auto base = std::make_shared<internal::AccessTokenConfig>(
      "base-access-token", now + std::chrono::seconds(1800), Options{});
  auto config = std::make_shared<internal::ImpersonateServiceAccountConfig>(
      base, kServiceAccountEmail, Options{});
  auto credentials = MapCredentials(config, client_factory.AsStdFunction());
  auto access_token = credentials->GetToken(now);
  EXPECT_THAT(access_token, StatusIs(StatusCode::kPermissionDenied,
                                     HasSubstr("uh-oh - cannot impersonate")));
}

TEST(UnifiedRestCredentialsTest, ServiceAccount) {
  auto const expected_expires_in = std::chrono::seconds(3600);
  auto const contents = MakeServiceAccountContents();
  auto const now = std::chrono::system_clock::now();
  auto info =
      oauth2_internal::ParseServiceAccountCredentials(contents.dump(), "test");
  ASSERT_STATUS_OK(info);
  auto const jwt = oauth2_internal::MakeSelfSignedJWT(*info, now);
  ASSERT_STATUS_OK(jwt);

  MockClientFactory client_factory;
  EXPECT_CALL(client_factory, Call).Times(0);

  auto config = std::make_shared<internal::ServiceAccountConfig>(
      contents.dump(), Options{});
  auto credentials = MapCredentials(config, client_factory.AsStdFunction());
  auto access_token = credentials->GetToken(now);
  ASSERT_STATUS_OK(access_token);
  EXPECT_EQ(access_token->expiration, now + expected_expires_in);
  EXPECT_EQ(access_token->token, *jwt);
}

TEST(UnifiedRestCredentialsTest, ExternalAccount) {
  // This sets up a mocked request for the subject token.
  auto const subject_url = std::string{"https://test-only-oidc.example.com/"};
  auto const subject_token = std::string{"test-subject-token"};
  auto subject_token_client = [subject_url, subject_token] {
    auto expected_sts_request = Property(&RestRequest::path, subject_url);
    auto mock = absl::make_unique<MockRestClient>();
    EXPECT_CALL(*mock, Get(expected_sts_request))
        .WillOnce(Return(ByMove(MakeMockResponse(subject_token))));
    return mock;
  }();

  // This sets up a mocked request for the token exchange.
  auto const sts_url = std::string{"https://sts.example.com/"};
  auto sts_client = [sts_url, subject_token] {
    using FormDataType = std::vector<std::pair<std::string, std::string>>;
    auto expected_sts_request = Property(&RestRequest::path, sts_url);
    // Check only one value, there are other test for the full contents.
    auto expected_form_data = MatcherCast<FormDataType const&>(
        Contains(Pair("subject_token", subject_token)));
    auto mock = absl::make_unique<MockRestClient>();
    EXPECT_CALL(*mock, Post(expected_sts_request, expected_form_data))
        .WillOnce(Return(
            Status{StatusCode::kPermissionDenied, "uh-oh - STS exchange"}));
    return mock;
  }();

  auto const json_external_account = nlohmann::json{
      {"type", "external_account"},
      {"audience", "test-audience"},
      {"subject_token_type", "test-subject-token-type"},
      {"token_url", sts_url},
      {"credential_source", nlohmann::json{{"url", subject_url}}},
  };

  MockClientFactory client_factory;
  EXPECT_CALL(client_factory, Call)
      .WillOnce(Return(ByMove(std::move(subject_token_client))))
      .WillOnce(Return(ByMove(std::move(sts_client))));

  auto config = std::make_shared<internal::ExternalAccountConfig>(
      json_external_account.dump(), Options{});
  auto credentials = MapCredentials(config, client_factory.AsStdFunction());
  auto const now = std::chrono::system_clock::now();
  auto access_token = credentials->GetToken(now);
  EXPECT_THAT(access_token,
              StatusIs(StatusCode::kPermissionDenied, "uh-oh - STS exchange"));
}

TEST(UnifiedRestCredentialsTest, LoadError) {
  // Create a name for a non-existing file, try to load it, and verify it
  // returns errors.
  auto const filename = TempKeyFileName();
  ScopedEnvironment env("GOOGLE_APPLICATION_CREDENTIALS", filename);

  auto credentials = MapCredentials(MakeGoogleDefaultCredentials());
  EXPECT_THAT(credentials->GetToken(std::chrono::system_clock::now()),
              Not(IsOk()));
}

TEST(UnifiedRestCredentialsTest, LoadSuccess) {
  auto const contents = MakeServiceAccountContents();
  auto const filename = TempKeyFileName();
  std::ofstream(filename) << contents.dump(4) << "\n";

  ScopedEnvironment env("GOOGLE_APPLICATION_CREDENTIALS", filename);

  auto credentials = MapCredentials(MakeGoogleDefaultCredentials());
  // Calling AuthorizationHeader() makes RPCs which would turn this into an
  // integration test, fortunately there are easier ways to verify the file was
  // loaded correctly:
  EXPECT_EQ(kServiceAccountEmail, credentials->AccountEmail());
  EXPECT_EQ(kServiceAccountKeyId, credentials->KeyId());

  std::remove(filename.c_str());
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace rest_internal
}  // namespace cloud
}  // namespace google
