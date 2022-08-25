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

#include "google/cloud/storage/oauth2/service_account_credentials.h"
#include "google/cloud/storage/oauth2/credential_constants.h"
#include "google/cloud/storage/oauth2/google_credentials.h"
#include "google/cloud/storage/testing/constants.h"
#include "google/cloud/storage/testing/mock_http_request.h"
#include "google/cloud/storage/testing/write_base64.h"
#include "google/cloud/internal/filesystem.h"
#include "google/cloud/internal/openssl_util.h"
#include "google/cloud/internal/random.h"
#include "google/cloud/testing_util/mock_fake_clock.h"
#include "google/cloud/testing_util/scoped_environment.h"
#include "google/cloud/testing_util/status_matchers.h"
#include "absl/strings/match.h"
#include "absl/strings/str_split.h"
#include <gmock/gmock.h>
#include <nlohmann/json.hpp>
#include <chrono>
#include <fstream>

namespace google {
namespace cloud {
namespace storage {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace oauth2 {
namespace {

using ::google::cloud::internal::SignUsingSha256;
using ::google::cloud::internal::UrlsafeBase64Decode;
using ::google::cloud::storage::internal::HttpResponse;
using ::google::cloud::storage::testing::kP12KeyFileContents;
using ::google::cloud::storage::testing::kP12ServiceAccountId;
using ::google::cloud::storage::testing::kWellFormattedKey;
using ::google::cloud::storage::testing::MockHttpRequest;
using ::google::cloud::storage::testing::MockHttpRequestBuilder;
using ::google::cloud::storage::testing::WriteBase64AsBinary;
using ::google::cloud::testing_util::FakeClock;
using ::google::cloud::testing_util::IsOk;
using ::google::cloud::testing_util::ScopedEnvironment;
using ::google::cloud::testing_util::StatusIs;
using ::testing::_;
using ::testing::An;
using ::testing::AtLeast;
using ::testing::ElementsAreArray;
using ::testing::HasSubstr;
using ::testing::Not;
using ::testing::Return;
using ::testing::StrEq;

constexpr char kScopeForTest0[] =
    "https://www.googleapis.com/auth/devstorage.full_control";
constexpr char kScopeForTest1[] =
    "https://www.googleapis.com/auth/cloud-platform";
// This "magic" assertion below was generated from helper script,
// "make_jwt_assertion_for_test_data.py". Note that when our JSON library dumps
// a string representation, the keys are always in alphabetical order; our
// helper script also takes special care to ensure Python dicts are dumped in
// this manner, as dumping the keys in a different order would result in a
// different Base64-encoded string, and thus a different assertion string.
constexpr char kExpectedAssertionParam[] =
    R"""(assertion=eyJhbGciOiJSUzI1NiIsImtpZCI6ImExYTExMWFhMTExMWExMWExMWExMWFhMTExYTExMWExYTExMTExMTEiLCJ0eXAiOiJKV1QifQ.eyJhdWQiOiJodHRwczovL29hdXRoMi5nb29nbGVhcGlzLmNvbS90b2tlbiIsImV4cCI6MTUzMDA2MzkyNCwiaWF0IjoxNTMwMDYwMzI0LCJpc3MiOiJmb28tZW1haWxAZm9vLXByb2plY3QuaWFtLmdzZXJ2aWNlYWNjb3VudC5jb20iLCJzY29wZSI6Imh0dHBzOi8vd3d3Lmdvb2dsZWFwaXMuY29tL2F1dGgvY2xvdWQtcGxhdGZvcm0ifQ.OtL40PSxdAB9rxRkXj-UeyuMhQCoT10WJY4ccOrPXriwm-DRl5AMgbBkQvVmWeYuPMTiFKWz_CMMBjVc3lFPW015eHvKT5r3ySGra1i8hJ9cDsWO7SdIGB-l00G-BdRxVEhN8U4C20eUhlvhtjXemOwlCFrKjF22rJB-ChiKy84rXs3O-Hz0dWmsSZPfVD9q-2S2vJdr9vz7NoP-fCmpxhQ3POVocYb-2OEM5c4Uo_e7lQTX3bRtVc19wz_wrTu9wMMMRYt52K8WPoWPURt7qpjHX88_EitXMzH-cJUQoDsgIoZ6vDlQMs7_nqNfgrlsGWHpPoSoGgvJMg1vJbzVLw)""";
constexpr std::time_t kFixedJwtTimestamp = 1530060324;
constexpr char kGrantParamUnescaped[] =
    "urn:ietf:params:oauth:grant-type:jwt-bearer";
constexpr char kGrantParamEscaped[] =
    "urn%3Aietf%3Aparams%3Aoauth%3Agrant-type%3Ajwt-bearer";

auto constexpr kProjectId = "foo-project";
auto constexpr kPrivateKeyId = "a1a111aa1111a11a11a11aa111a111a1a1111111";
auto constexpr kClientEmail = "foo-email@foo-project.iam.gserviceaccount.com";
auto constexpr kClientId = "100000000000000000001";
auto constexpr kAuthUri = "https://accounts.google.com/o/oauth2/auth";
auto constexpr kTokenUri = "https://oauth2.googleapis.com/token";
auto constexpr kAuthProviderX509CerlUrl =
    "https://www.googleapis.com/oauth2/v1/certs";
auto constexpr kClientX509CertUrl =
    "https://www.googleapis.com/robot/v1/metadata/x509/"
    "foo-email%40foo-project.iam.gserviceaccount.com";
constexpr char kSubjectForGrant[] = "user@foo.bar";

std::string MakeTestContents() {
  return nlohmann::json{
      {"type", "service_account"},
      {"project_id", kProjectId},
      {"private_key_id", kPrivateKeyId},
      {"private_key", kWellFormattedKey},
      {"client_email", kClientEmail},
      {"client_id", kClientId},
      {"auth_uri", kAuthUri},
      {"token_uri", kTokenUri},
      {"auth_provider_x509_cert_url", kAuthProviderX509CerlUrl},
      {"client_x509_cert_url", kClientX509CertUrl},
  }
      .dump();
}

class ServiceAccountCredentialsTest : public ::testing::Test {
 protected:
  void SetUp() override {
    MockHttpRequestBuilder::mock_ =
        std::make_shared<MockHttpRequestBuilder::Impl>();
    FakeClock::reset_clock(kFixedJwtTimestamp);
  }
  void TearDown() override { MockHttpRequestBuilder::mock_.reset(); }

  std::string CreateRandomFileName() {
    // When running on the internal Google CI systems we cannot write to the
    // local directory, GTest has a good temporary directory in that case.
    return google::cloud::internal::PathAppend(
        ::testing::TempDir(),
        google::cloud::internal::Sample(
            generator_, 8, "abcdefghijklmnopqrstuvwxyz0123456789"));
  }

  google::cloud::internal::DefaultPRNG generator_ =
      google::cloud::internal::MakeDefaultPRNG();
};

TEST_F(ServiceAccountCredentialsTest, MultipleScopes) {
  auto info = ParseServiceAccountCredentials(MakeTestContents(), "test");
  ASSERT_STATUS_OK(info);
  auto expected_info = *info;
  // .scopes is a `std::set<std::string>` so we need to preserve order.
  ASSERT_LT(std::string{kScopeForTest1}, kScopeForTest0);
  expected_info.scopes = {std::string{kScopeForTest1} + " " + kScopeForTest0};
  expected_info.subject = std::string(kSubjectForGrant);
  auto const now = std::chrono::system_clock::now();
  auto const expected_components =
      AssertionComponentsFromInfo(expected_info, now);

  auto actual_info = *info;
  actual_info.scopes = {kScopeForTest0, kScopeForTest1};
  actual_info.subject = std::string(kSubjectForGrant);
  auto const actual_components = AssertionComponentsFromInfo(actual_info, now);
  EXPECT_EQ(actual_components, expected_components);
}

/// @test Verify that we refresh service account credentials appropriately.
TEST_F(ServiceAccountCredentialsTest,
       RefreshCalledOnlyWhenAccessTokenIsMissingOrInvalid) {
  ScopedEnvironment disable_self_signed_jwt(
      "GOOGLE_CLOUD_CPP_EXPERIMENTAL_DISABLE_SELF_SIGNED_JWT", "1");

  // Prepare two responses, the first one is used but becomes immediately
  // expired, resulting in another refresh next time the caller tries to get
  // an authorization header.
  std::string r1 = R"""({
    "token_type": "Type",
    "access_token": "access-token-r1",
    "expires_in": 0
})""";
  std::string r2 = R"""({
    "token_type": "Type",
    "access_token": "access-token-r2",
    "expires_in": 1000
})""";

  // Now setup the builder to return those responses.
  auto mock_builder = MockHttpRequestBuilder::mock_;
  EXPECT_CALL(*mock_builder, BuildRequest())
      .WillOnce([&] {
        MockHttpRequest request;
        EXPECT_CALL(*request.mock, MakeRequest)
            .WillOnce(Return(HttpResponse{200, r1, {}}));
        return request;
      })
      .WillOnce([&] {
        MockHttpRequest request;
        EXPECT_CALL(*request.mock, MakeRequest)
            .WillOnce(Return(HttpResponse{200, r2, {}}));
        return request;
      });
  EXPECT_CALL(*mock_builder, AddHeader(An<std::string const&>()))
      .Times(AtLeast(1));
  EXPECT_CALL(*mock_builder, Constructor(GoogleOAuthRefreshEndpoint(), _, _))
      .Times(AtLeast(1));
  EXPECT_CALL(*mock_builder, MakeEscapedString(An<std::string const&>()))
      .WillRepeatedly([](std::string const& s) -> std::unique_ptr<char[]> {
        EXPECT_EQ(kGrantParamUnescaped, s);
        auto t = std::unique_ptr<char[]>(new char[sizeof(kGrantParamEscaped)]);
        std::copy(kGrantParamEscaped,
                  kGrantParamEscaped + sizeof(kGrantParamEscaped), t.get());
        return t;
      });

  auto info = ParseServiceAccountCredentials(MakeTestContents(), "test");
  ASSERT_STATUS_OK(info);
  ServiceAccountCredentials<MockHttpRequestBuilder> credentials(*info);
  // Calls Refresh to obtain the access token for our authorization header.
  EXPECT_EQ("Authorization: Type access-token-r1",
            credentials.AuthorizationHeader().value());
  // Token is expired, resulting in another call to Refresh.
  EXPECT_EQ("Authorization: Type access-token-r2",
            credentials.AuthorizationHeader().value());
  // Token still valid; should return cached token instead of calling Refresh.
  EXPECT_EQ("Authorization: Type access-token-r2",
            credentials.AuthorizationHeader().value());
}

/// @test Verify that parsing a service account JSON string works.
TEST_F(ServiceAccountCredentialsTest, ParseSimple) {
  std::string contents = R"""({
      "type": "service_account",
      "private_key_id": "not-a-key-id-just-for-testing",
      "private_key": "not-a-valid-key-just-for-testing",
      "client_email": "test-only@test-group.example.com",
      "token_uri": "https://oauth2.googleapis.com/test_endpoint"
})""";

  auto actual =
      ParseServiceAccountCredentials(contents, "test-data", "unused-uri");
  ASSERT_STATUS_OK(actual);
  EXPECT_EQ("not-a-key-id-just-for-testing", actual->private_key_id);
  EXPECT_EQ("not-a-valid-key-just-for-testing", actual->private_key);
  EXPECT_EQ("test-only@test-group.example.com", actual->client_email);
  EXPECT_EQ("https://oauth2.googleapis.com/test_endpoint", actual->token_uri);
}

/// @test Verify that parsing a service account JSON string works.
TEST_F(ServiceAccountCredentialsTest, ParseUsesExplicitDefaultTokenUri) {
  // No token_uri attribute here, so the default passed below should be used.
  std::string contents = R"""({
      "type": "service_account",
      "private_key_id": "not-a-key-id-just-for-testing",
      "private_key": "not-a-valid-key-just-for-testing",
      "client_email": "test-only@test-group.example.com"
})""";

  auto actual = ParseServiceAccountCredentials(
      contents, "test-data", "https://oauth2.googleapis.com/test_endpoint");
  ASSERT_STATUS_OK(actual);
  EXPECT_EQ("not-a-key-id-just-for-testing", actual->private_key_id);
  EXPECT_EQ("not-a-valid-key-just-for-testing", actual->private_key);
  EXPECT_EQ("test-only@test-group.example.com", actual->client_email);
  EXPECT_EQ("https://oauth2.googleapis.com/test_endpoint", actual->token_uri);
}

/// @test Verify that parsing a service account JSON string works.
TEST_F(ServiceAccountCredentialsTest, ParseUsesImplicitDefaultTokenUri) {
  // No token_uri attribute here.
  std::string contents = R"""({
      "type": "service_account",
      "private_key_id": "not-a-key-id-just-for-testing",
      "private_key": "not-a-valid-key-just-for-testing",
      "client_email": "test-only@test-group.example.com"
})""";

  // No token_uri passed in here, either.
  auto actual = ParseServiceAccountCredentials(contents, "test-data");
  ASSERT_STATUS_OK(actual);
  EXPECT_EQ("not-a-key-id-just-for-testing", actual->private_key_id);
  EXPECT_EQ("not-a-valid-key-just-for-testing", actual->private_key);
  EXPECT_EQ("test-only@test-group.example.com", actual->client_email);
  EXPECT_EQ(std::string(GoogleOAuthRefreshEndpoint()), actual->token_uri);
}

/// @test Verify that invalid contents result in a readable error.
TEST_F(ServiceAccountCredentialsTest, ParseInvalidContentsFails) {
  EXPECT_THAT(ParseServiceAccountCredentials(R"""( not-a-valid-json-string )""",
                                             "test-as-a-source"),
              StatusIs(Not(StatusCode::kOk),
                       AllOf(HasSubstr("Invalid ServiceAccountCredentials"),
                             HasSubstr("test-as-a-source"))));

  EXPECT_THAT(ParseServiceAccountCredentials(
                  R"""("valid-json-but-not-an-object")""", "test-as-a-source"),
              StatusIs(Not(StatusCode::kOk),
                       AllOf(HasSubstr("Invalid ServiceAccountCredentials"),
                             HasSubstr("test-as-a-source"))));
}

/// @test Parsing a service account JSON string should detect empty fields.
TEST_F(ServiceAccountCredentialsTest, ParseEmptyFieldFails) {
  std::string contents = R"""({
      "type": "service_account",
      "private_key": "not-a-valid-key-just-for-testing",
      "client_email": "test-only@test-group.example.com",
      "token_uri": "https://oauth2.googleapis.com/token"
})""";

  for (auto const& field : {"private_key", "client_email", "token_uri"}) {
    auto json = nlohmann::json::parse(contents);
    json[field] = "";
    auto actual = ParseServiceAccountCredentials(json.dump(), "test-data", "");
    EXPECT_THAT(actual,
                StatusIs(Not(StatusCode::kOk),
                         AllOf(HasSubstr(field), HasSubstr(" field is empty"),
                               HasSubstr("test-data"))));
  }
}

/// @test Parsing a service account JSON string should detect missing fields.
TEST_F(ServiceAccountCredentialsTest, ParseMissingFieldFails) {
  std::string contents = R"""({
      "type": "service_account",
      "private_key": "not-a-valid-key-just-for-testing",
      "client_email": "test-only@test-group.example.com",
      "token_uri": "https://oauth2.googleapis.com/token"
})""";

  for (auto const& field : {"private_key", "client_email"}) {
    auto json = nlohmann::json::parse(contents);
    json.erase(field);
    auto actual = ParseServiceAccountCredentials(json.dump(), "test-data", "");
    EXPECT_THAT(actual,
                StatusIs(Not(StatusCode::kOk),
                         AllOf(HasSubstr(field), HasSubstr(" field is missing"),
                               HasSubstr("test-data"))));
  }
}

/// @test Parsing a service account JSON string allows an optional field.
TEST_F(ServiceAccountCredentialsTest, ParseOptionalField) {
  std::string contents = R"""({
      "type": "service_account",
      "private_key_id": "",
      "private_key": "not-a-valid-key-just-for-testing",
      "client_email": "test-only@test-group.example.com",
      "token_uri": "https://oauth2.googleapis.com/token"
})""";

  auto json = nlohmann::json::parse(contents);
  auto actual = ParseServiceAccountCredentials(json.dump(), "test-data", "");
  ASSERT_STATUS_OK(actual.status());
}

/// @test Verify that the options are used in the constructor.
TEST_F(ServiceAccountCredentialsTest, UsesCARootsInfo) {
  ScopedEnvironment disable_self_signed_jwt(
      "GOOGLE_CLOUD_CPP_EXPERIMENTAL_DISABLE_SELF_SIGNED_JWT", "1");

  auto info = ParseServiceAccountCredentials(MakeTestContents(), "test");
  ASSERT_STATUS_OK(info);

  auto mock_builder = MockHttpRequestBuilder::mock_;
  EXPECT_CALL(*mock_builder, BuildRequest()).WillOnce([&] {
    MockHttpRequest result;
    EXPECT_CALL(*result.mock, MakeRequest).WillOnce([](std::string const&) {
      nlohmann::json response{{"token_type", "Mock-Type"},
                              {"access_token", "fake-token"},
                              {"expires_in", 3600}};
      return HttpResponse{200, response.dump(), {}};
    });
    return result;
  });

  // This is the key check in this test, verify the constructor is called with
  // the right parameters.
  auto const cainfo = std::string{"fake-cainfo-path-aka-roots-pem"};
  EXPECT_CALL(*mock_builder, Constructor(GoogleOAuthRefreshEndpoint(),
                                         absl::make_optional(cainfo), _))
      .Times(AtLeast(1));

  auto const expected_header =
      std::string{"Content-Type: application/x-www-form-urlencoded"};
  EXPECT_CALL(*mock_builder, AddHeader(StrEq(expected_header)))
      .Times(AtLeast(1));
  EXPECT_CALL(*mock_builder, MakeEscapedString(An<std::string const&>()))
      .WillRepeatedly([](std::string const& s) -> std::unique_ptr<char[]> {
        EXPECT_EQ(kGrantParamUnescaped, s);
        auto t = std::unique_ptr<char[]>(new char[sizeof(kGrantParamEscaped)]);
        std::copy(kGrantParamEscaped,
                  kGrantParamEscaped + sizeof(kGrantParamEscaped), t.get());
        return t;
      });

  ServiceAccountCredentials<MockHttpRequestBuilder, FakeClock> credentials(
      *info, ChannelOptions().set_ssl_root_path(cainfo));
  // Call Refresh to obtain the access token for our authorization header.
  auto authorization_header = credentials.AuthorizationHeader();
  ASSERT_STATUS_OK(authorization_header);
  EXPECT_EQ("Authorization: Mock-Type fake-token", *authorization_header);
}

/// @test Verify that we can create sign blobs using a service account.
TEST_F(ServiceAccountCredentialsTest, SignBlob) {
  auto info = ParseServiceAccountCredentials(MakeTestContents(), "test");
  ASSERT_STATUS_OK(info);
  ServiceAccountCredentials<MockHttpRequestBuilder, FakeClock> credentials(
      *info);

  std::string blob = R"""(GET
rmYdCNHKFXam78uCt7xQLw==
text/plain
1388534400
x-goog-encryption-algorithm:AES256
x-goog-meta-foo:bar,baz
/bucket/objectname)""";

  auto actual = credentials.SignBlob(SigningAccount(), blob);
  ASSERT_STATUS_OK(actual);

  // To generate the expected output I used:
  //   `openssl dgst -sha256 -sign private.pem blob.txt | openssl base64 -A`
  // where `blob.txt` contains the `blob` string, and `private.pem` contains
  // the private key embedded in `kJsonKeyfileContents`.
  std::string expected_signed =
      "Zsy8o5ci07DQTvO/"
      "SVr47PKsCXvN+"
      "FzXga0iYrReAnngdZYewHdcAnMQ8bZvFlTM8HY3msrRw64Jc6hoXVL979An5ugXoZ1ol/"
      "DT1KlKp3l9E0JSIbqL88ogpElTxFvgPHOtHOUsy2mzhqOVrNSXSj4EM50gKHhvHKSbFq8Pcj"
      "lAkROtq5gqp5t0OFd7EMIaRH+tekVUZjQPfFT/"
      "hRW9bSCCV8w1Ex+"
      "QxmB5z7P7zZn2pl7JAcL850emTo8f2tfv1xXWQGhACvIJeMdPmyjbc04Ye4M8Ljpkg3YhE6l"
      "4GwC2MnI8TkuoHe4Bj2MvA8mM8TVwIvpBs6Etsj6Jdaz4rg==";
  EXPECT_EQ(expected_signed, internal::Base64Encode(*actual));
}

/// @test Verify that signing blobs fails with invalid e-mail.
TEST_F(ServiceAccountCredentialsTest, SignBlobFailure) {
  auto info = ParseServiceAccountCredentials(MakeTestContents(), "test");
  ASSERT_STATUS_OK(info);
  ServiceAccountCredentials<MockHttpRequestBuilder, FakeClock> credentials(
      *info);

  auto actual =
      credentials.SignBlob(SigningAccount("fake@fake.com"), "test-blob");
  EXPECT_THAT(
      actual,
      StatusIs(StatusCode::kInvalidArgument,
               HasSubstr("The current_credentials cannot sign blobs for ")));
}

/// @test Verify that we can get the client id from a service account.
TEST_F(ServiceAccountCredentialsTest, ClientId) {
  auto info = ParseServiceAccountCredentials(MakeTestContents(), "test");
  ASSERT_STATUS_OK(info);
  ServiceAccountCredentials<MockHttpRequestBuilder, FakeClock> credentials(
      *info);

  EXPECT_EQ("foo-email@foo-project.iam.gserviceaccount.com",
            credentials.AccountEmail());
  EXPECT_EQ("a1a111aa1111a11a11a11aa111a111a1a1111111", credentials.KeyId());
}

char const kP12KeyFileMissingCerts[] =
    "MIIDzAIBAzCCA5IGCSqGSIb3DQEHAaCCA4MEggN/MIIDezCCA3cGCSqGSIb3DQEH"
    "BqCCA2gwggNkAgEAMIIDXQYJKoZIhvcNAQcBMBwGCiqGSIb3DQEMAQYwDgQILaGB"
    "fWhJ2V0CAggAgIIDMM5EI/ck4VQD4JyGchVPbgd5HQjFbn+HThIoxBYpMPEK+iT7"
    "t32idiirDi0qH+6nZancp69nnKhjpAOnMLSjCvba7HDFzi/op7fgf9hnwupEOahv"
    "4b8Wv0S9ePTqsLfJy8tJzOAPYKOJO7HGSeZanWh2HpyCd2g1K1dBXsqsabTtJBsF"
    "TSGsfUg08/SMT5o12BlMk/wjzUrcSNQxntyPXLfjO1uZ0gFjFO6xsFyclVWr8Zax"
    "7fTA6SLdgeE1Iu2+mS1ohwNNzeBrCU6kXVzgw1GSn0UV0ZGbANRWDZZThWzQs9UW"
    "sn8l1fr70OZ4JhUwPZe9g0Tu7EeGNPkM5dW1Lr3izKNtYdInBD/1J7wGxsmomsU3"
    "khIH2FMqqYX7NFkI0TZiHpLYk2bQmMnfFbBDlXluzO2iLvBY5FPUCn5W4ZPAJlFs"
    "Ryo/OytciwJUIRoz76CIg3TmzM1b+RLBMEr6lAsD1za3fcTMwbsBeYY0FEFfb/I6"
    "ddmJTxjbCLPLekgkV7MIFSWPiL4t2eXR3rlu1Vnoys0aTWmFtJhEOI16Q1bkJ9L1"
    "c/KXHm/Srccm8hTazNYQewHRXWiAvigg6slRnx1I36Z0TMbnikDVCRH8cjFsMKO5"
    "/qNMKSsZ6EAePHYAu4N5CpqaTl0hjHI8sW+CDzzmGOn8Acb00gJ+DOu+wiTZtJYS"
    "GIZogs7PluMJ7cU1Ju38OixWbQDvfDdloQ/7kZrM6DoEKhvC2bwMwlfxin9jUwjJ"
    "98dtdAwQVgckvnYYVpqKnn/dlkiStaiZFKx27kw6o2oobcDrkg0wtOZFeX8k0SXZ"
    "ekcmMc5Xfl+5HyJxH5ni8UmHyOHAM8dNjpnzCD9J2K0U7z8kdzslZ95X5MAxYIUa"
    "r50tIaWHxeLLYYZUi+nyjNbMZ+yvAqOjQqI1mIcYZurHRPRIHVi2x4nfcKKQIkxn"
    "UTF9d3VWbkWoJ1qfe0OSpWg4RrdgDCSB1BlF0gQHEsDTT5/xoZIEoUV8t6TYTVCe"
    "axreBYxLhvROONz94v6GD6Eb4kakbSObn8NuBiWnaPevFyEF5YluKR87MbZRQY0Z"
    "yJ/4PuEhDIioRdY7ujAxMCEwCQYFKw4DAhoFAAQU4/UMFJQGUvgPuTXRKp0gVU4B"
    "GbkECPTYJIica3DWAgIIAA==";

char const kP12KeyFileMissingKey[] =
    "MIIDzAIBAzCCA5IGCSqGSIb3DQEHAaCCA4MEggN/MIIDezCCA3cGCSqGSIb3DQEH"
    "BqCCA2gwggNkAgEAMIIDXQYJKoZIhvcNAQcBMBwGCiqGSIb3DQEMAQYwDgQILaGB"
    "fWhJ2V0CAggAgIIDMM5EI/ck4VQD4JyGchVPbgd5HQjFbn+HThIoxBYpMPEK+iT7"
    "t32idiirDi0qH+6nZancp69nnKhjpAOnMLSjCvba7HDFzi/op7fgf9hnwupEOahv"
    "4b8Wv0S9ePTqsLfJy8tJzOAPYKOJO7HGSeZanWh2HpyCd2g1K1dBXsqsabTtJBsF"
    "TSGsfUg08/SMT5o12BlMk/wjzUrcSNQxntyPXLfjO1uZ0gFjFO6xsFyclVWr8Zax"
    "7fTA6SLdgeE1Iu2+mS1ohwNNzeBrCU6kXVzgw1GSn0UV0ZGbANRWDZZThWzQs9UW"
    "sn8l1fr70OZ4JhUwPZe9g0Tu7EeGNPkM5dW1Lr3izKNtYdInBD/1J7wGxsmomsU3"
    "khIH2FMqqYX7NFkI0TZiHpLYk2bQmMnfFbBDlXluzO2iLvBY5FPUCn5W4ZPAJlFs"
    "Ryo/OytciwJUIRoz76CIg3TmzM1b+RLBMEr6lAsD1za3fcTMwbsBeYY0FEFfb/I6"
    "ddmJTxjbCLPLekgkV7MIFSWPiL4t2eXR3rlu1Vnoys0aTWmFtJhEOI16Q1bkJ9L1"
    "c/KXHm/Srccm8hTazNYQewHRXWiAvigg6slRnx1I36Z0TMbnikDVCRH8cjFsMKO5"
    "/qNMKSsZ6EAePHYAu4N5CpqaTl0hjHI8sW+CDzzmGOn8Acb00gJ+DOu+wiTZtJYS"
    "GIZogs7PluMJ7cU1Ju38OixWbQDvfDdloQ/7kZrM6DoEKhvC2bwMwlfxin9jUwjJ"
    "98dtdAwQVgckvnYYVpqKnn/dlkiStaiZFKx27kw6o2oobcDrkg0wtOZFeX8k0SXZ"
    "ekcmMc5Xfl+5HyJxH5ni8UmHyOHAM8dNjpnzCD9J2K0U7z8kdzslZ95X5MAxYIUa"
    "r50tIaWHxeLLYYZUi+nyjNbMZ+yvAqOjQqI1mIcYZurHRPRIHVi2x4nfcKKQIkxn"
    "UTF9d3VWbkWoJ1qfe0OSpWg4RrdgDCSB1BlF0gQHEsDTT5/xoZIEoUV8t6TYTVCe"
    "axreBYxLhvROONz94v6GD6Eb4kakbSObn8NuBiWnaPevFyEF5YluKR87MbZRQY0Z"
    "yJ/4PuEhDIioRdY7ujAxMCEwCQYFKw4DAhoFAAQU4/UMFJQGUvgPuTXRKp0gVU4B"
    "GbkECPTYJIica3DWAgIIAA==";

/// @test Verify that parsing a service account JSON string works.
TEST_F(ServiceAccountCredentialsTest, ParseSimpleP12) {
  auto filename = CreateRandomFileName() + ".p12";
  WriteBase64AsBinary(filename, kP12KeyFileContents);

  auto info = ParseServiceAccountP12File(filename);
  if (info.status().code() == StatusCode::kInvalidArgument &&
      absl::StrContains(info.status().message(), "error:0308010C")) {
    // With OpenSSL 3.0 the PKCS#12 files may not be supported by default.
    GTEST_SKIP();
  }
  ASSERT_STATUS_OK(info);

  EXPECT_EQ(kP12ServiceAccountId, info->client_email);
  EXPECT_FALSE(info->private_key.empty());
  EXPECT_EQ(0, std::remove(filename.c_str()));

  ServiceAccountCredentials<> credentials(*info);

  auto signed_blob = credentials.SignBlob(SigningAccount(), "test-blob");
  EXPECT_STATUS_OK(signed_blob);
}

TEST_F(ServiceAccountCredentialsTest, ParseP12MissingKey) {
  std::string filename = CreateRandomFileName() + ".p12";
  WriteBase64AsBinary(filename, kP12KeyFileMissingKey);
  auto info = ParseServiceAccountP12File(filename);
  EXPECT_THAT(info, Not(IsOk()));
}

TEST_F(ServiceAccountCredentialsTest, ParseP12MissingCerts) {
  std::string filename = google::cloud::internal::PathAppend(
      ::testing::TempDir(), CreateRandomFileName() + ".p12");
  WriteBase64AsBinary(filename, kP12KeyFileMissingCerts);
  auto info = ParseServiceAccountP12File(filename);
  EXPECT_THAT(info, Not(IsOk()));
}

TEST_F(ServiceAccountCredentialsTest, CreateFromP12MissingFile) {
  std::string filename = CreateRandomFileName();
  // Loading a non-existent file should fail.
  auto actual = CreateServiceAccountCredentialsFromP12FilePath(filename);
  EXPECT_THAT(actual, Not(IsOk()));
}

TEST_F(ServiceAccountCredentialsTest, CreateFromP12EmptyFile) {
  std::string filename = CreateRandomFileName();
  std::ofstream(filename).close();

  // Loading an empty file should fail.
  auto actual = CreateServiceAccountCredentialsFromP12FilePath(filename);
  EXPECT_THAT(actual, Not(IsOk()));

  EXPECT_EQ(0, std::remove(filename.c_str()));
}

TEST_F(ServiceAccountCredentialsTest, CreateFromP12ValidFile) {
  std::string filename = CreateRandomFileName() + ".p12";
  WriteBase64AsBinary(filename, kP12KeyFileContents);

  auto actual = CreateServiceAccountCredentialsFromP12FilePath(filename);
  if (actual.status().code() == StatusCode::kInvalidArgument &&
      absl::StrContains(actual.status().message(), "error:0308010C")) {
    // With OpenSSL 3.0 the PKCS#12 files may not be supported by default.
    GTEST_SKIP();
  }
  EXPECT_STATUS_OK(actual);

  EXPECT_EQ(0, std::remove(filename.c_str()));
}

/// @test Verify we can obtain JWT assertion components given the info parsed
/// from a keyfile.
TEST_F(ServiceAccountCredentialsTest, AssertionComponentsFromInfo) {
  auto info = ParseServiceAccountCredentials(MakeTestContents(), "test");
  ASSERT_STATUS_OK(info);
  auto const clock_value_1 = 10000;
  FakeClock::now_value_ = clock_value_1;
  auto components = AssertionComponentsFromInfo(*info, FakeClock::now());

  auto header = nlohmann::json::parse(components.first);
  EXPECT_EQ("RS256", header.value("alg", ""));
  EXPECT_EQ("JWT", header.value("typ", ""));
  EXPECT_EQ(info->private_key_id, header.value("kid", ""));

  auto payload = nlohmann::json::parse(components.second);
  EXPECT_EQ(clock_value_1, payload.value("iat", 0));
  EXPECT_EQ(clock_value_1 + 3600, payload.value("exp", 0));
  EXPECT_EQ(info->client_email, payload.value("iss", ""));
  EXPECT_EQ(info->token_uri, payload.value("aud", ""));
}

/// @test Verify we can construct a JWT assertion given the info parsed from a
/// keyfile.
TEST_F(ServiceAccountCredentialsTest, MakeJWTAssertion) {
  auto info = ParseServiceAccountCredentials(MakeTestContents(), "test");
  ASSERT_STATUS_OK(info);
  FakeClock::reset_clock(kFixedJwtTimestamp);
  auto components = AssertionComponentsFromInfo(*info, FakeClock::now());
  auto assertion =
      MakeJWTAssertion(components.first, components.second, info->private_key);

  std::vector<std::string> expected_tokens =
      absl::StrSplit(kExpectedAssertionParam, '.');
  std::string const& expected_encoded_header = expected_tokens[0];
  std::string const& expected_encoded_payload = expected_tokens[1];
  std::string const& expected_encoded_signature = expected_tokens[2];

  std::vector<std::string> actual_tokens = absl::StrSplit(assertion, '.');
  std::string const& actual_encoded_header = actual_tokens[0];
  std::string const& actual_encoded_payload = actual_tokens[1];
  std::string const& actual_encoded_signature = actual_tokens[2];

  EXPECT_EQ(expected_encoded_header, "assertion=" + actual_encoded_header);
  EXPECT_EQ(expected_encoded_payload, actual_encoded_payload);
  EXPECT_EQ(expected_encoded_signature, actual_encoded_signature);
}

/// @test Verify we can construct a service account refresh payload given the
/// info parsed from a keyfile.
TEST_F(ServiceAccountCredentialsTest, CreateServiceAccountRefreshPayload) {
  auto info = ParseServiceAccountCredentials(MakeTestContents(), "test");
  ASSERT_STATUS_OK(info);
  FakeClock::reset_clock(kFixedJwtTimestamp);
  auto components = AssertionComponentsFromInfo(*info, FakeClock::now());
  auto assertion =
      MakeJWTAssertion(components.first, components.second, info->private_key);
  auto actual_payload = CreateServiceAccountRefreshPayload(
      *info, kGrantParamEscaped, FakeClock::now());

  EXPECT_THAT(actual_payload, HasSubstr(std::string("assertion=") + assertion));
  EXPECT_THAT(actual_payload, HasSubstr(kGrantParamUnescaped));
}

/// @test Parsing a refresh response with missing fields results in failure.
TEST_F(ServiceAccountCredentialsTest,
       ParseServiceAccountRefreshResponseInvalid) {
  ScopedEnvironment disable_self_signed_jwt(
      "GOOGLE_CLOUD_CPP_EXPERIMENTAL_DISABLE_SELF_SIGNED_JWT", "1");

  std::string r1 = R"""({})""";
  // Does not have access_token.
  std::string r2 = R"""({
    "token_type": "Type",
    "id_token": "id-token-value",
    "expires_in": 1000
})""";

  FakeClock::reset_clock(1000);
  auto status = ParseServiceAccountRefreshResponse(HttpResponse{400, r1, {}},
                                                   FakeClock::now());
  EXPECT_THAT(status,
              StatusIs(StatusCode::kInvalidArgument,
                       HasSubstr("Could not find all required fields")));

  status = ParseServiceAccountRefreshResponse(HttpResponse{400, r2, {}},
                                              FakeClock::now());
  EXPECT_THAT(status,
              StatusIs(StatusCode::kInvalidArgument,
                       HasSubstr("Could not find all required fields")));

  EXPECT_THAT(
      ParseServiceAccountRefreshResponse(
          HttpResponse{400, R"js("valid-json-but-not-an-object")js", {}},
          FakeClock::now()),
      StatusIs(StatusCode::kInvalidArgument,
               HasSubstr("Could not find all required fields")));
}

/// @test Parsing a refresh response yields a TemporaryToken.
TEST_F(ServiceAccountCredentialsTest, ParseServiceAccountRefreshResponse) {
  ScopedEnvironment disable_self_signed_jwt(
      "GOOGLE_CLOUD_CPP_EXPERIMENTAL_DISABLE_SELF_SIGNED_JWT", "1");

  std::string r1 = R"""({
    "token_type": "Type",
    "access_token": "access-token-r1",
    "expires_in": 1000
})""";

  auto expires_in = 1000;
  FakeClock::reset_clock(2000);
  auto status = ParseServiceAccountRefreshResponse(HttpResponse{200, r1, {}},
                                                   FakeClock::now());
  EXPECT_STATUS_OK(status);
  auto token = *status;
  EXPECT_EQ(
      std::chrono::time_point_cast<std::chrono::seconds>(token.expiration_time)
          .time_since_epoch()
          .count(),
      FakeClock::now_value_ + expires_in);
  EXPECT_EQ(token.token, "Authorization: Type access-token-r1");
}

TEST_F(ServiceAccountCredentialsTest, MakeSelfSignedJWT) {
  auto info = ParseServiceAccountCredentials(MakeTestContents(), "test");
  ASSERT_STATUS_OK(info);
  auto const now = std::chrono::system_clock::now();
  auto actual = MakeSelfSignedJWT(*info, now);
  ASSERT_STATUS_OK(actual);

  std::vector<std::string> components = absl::StrSplit(*actual, '.');
  std::vector<std::string> decoded(components.size());
  std::transform(components.begin(), components.end(), decoded.begin(),
                 [](std::string const& e) {
                   auto v = UrlsafeBase64Decode(e).value();
                   return std::string{v.begin(), v.end()};
                 });
  ASSERT_THAT(3, decoded.size());
  auto const header = nlohmann::json::parse(decoded[0], nullptr);
  ASSERT_FALSE(header.is_null()) << "header=" << decoded[0];
  auto const payload = nlohmann::json::parse(decoded[1], nullptr);
  ASSERT_FALSE(payload.is_null()) << "payload=" << decoded[1];

  auto const expected_header = nlohmann::json{
      {"alg", "RS256"}, {"typ", "JWT"}, {"kid", info->private_key_id}};

  auto const iat =
      std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch());
  auto const exp = iat + std::chrono::hours(1);
  auto const expected_payload = nlohmann::json{
      {"iss", info->client_email},
      {"sub", info->client_email},
      {"iat", iat.count()},
      {"exp", exp.count()},
      {"scope", "https://www.googleapis.com/auth/cloud-platform"},
  };

  ASSERT_EQ(expected_header, header) << "header=" << header;
  ASSERT_EQ(expected_payload, payload) << "payload=" << payload;

  auto signature =
      SignUsingSha256(components[0] + '.' + components[1], info->private_key);
  ASSERT_STATUS_OK(signature);
  EXPECT_THAT(*signature,
              ElementsAreArray(decoded[2].begin(), decoded[2].end()));
}

TEST_F(ServiceAccountCredentialsTest, MakeSelfSignedJWTWithScopes) {
  auto info = ParseServiceAccountCredentials(MakeTestContents(), "test");
  ASSERT_STATUS_OK(info);
  info->scopes = std::set<std::string>{"test-only-s1", "test-only-s2"};

  auto const now = std::chrono::system_clock::now();
  auto actual = MakeSelfSignedJWT(*info, now);
  ASSERT_STATUS_OK(actual);

  std::vector<std::string> components = absl::StrSplit(*actual, '.');
  std::vector<std::string> decoded(components.size());
  std::transform(components.begin(), components.end(), decoded.begin(),
                 [](std::string const& e) {
                   auto v = UrlsafeBase64Decode(e).value();
                   return std::string{v.begin(), v.end()};
                 });
  ASSERT_THAT(3, decoded.size());
  auto const header = nlohmann::json::parse(decoded[0], nullptr);
  ASSERT_FALSE(header.is_null()) << "header=" << decoded[0];
  auto const payload = nlohmann::json::parse(decoded[1], nullptr);
  ASSERT_FALSE(payload.is_null()) << "payload=" << decoded[1];

  auto const expected_header = nlohmann::json{
      {"alg", "RS256"}, {"typ", "JWT"}, {"kid", info->private_key_id}};

  auto const iat =
      std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch());
  auto const exp = iat + std::chrono::hours(1);
  auto const expected_payload = nlohmann::json{
      {"iss", info->client_email},
      {"sub", info->client_email},
      {"iat", iat.count()},
      {"exp", exp.count()},
      {"scope", "test-only-s1 test-only-s2"},
  };

  ASSERT_EQ(expected_header, header) << "header=" << header;
  ASSERT_EQ(expected_payload, payload) << "payload=" << payload;

  auto signature =
      SignUsingSha256(components[0] + '.' + components[1], info->private_key);
  ASSERT_STATUS_OK(signature);
  EXPECT_THAT(*signature,
              ElementsAreArray(decoded[2].begin(), decoded[2].end()));
}

TEST_F(ServiceAccountCredentialsTest, UseOauth) {
  std::string filename = CreateRandomFileName() + ".p12";
  WriteBase64AsBinary(filename, kP12KeyFileContents);

  auto p12_info = ParseServiceAccountP12File(filename);
  EXPECT_EQ(0, std::remove(filename.c_str()));
  if (!p12_info) GTEST_SKIP();  // Some environments do not support PKCS#12
  ASSERT_STATUS_OK(p12_info);

  auto json_info = ParseServiceAccountCredentials(MakeTestContents(), "test");
  ASSERT_STATUS_OK(json_info);

  struct TestCase {
    std::string name;
    ServiceAccountCredentialsInfo info;
    absl::optional<std::string> environment;
    bool expected;
  } cases[] = {
      {"JSON/no-env", *json_info, absl::nullopt, false},
      {"JSON/env", *json_info, "1", true},
      {"P12/no-env", *p12_info, absl::nullopt, true},
      {"P12/env", *p12_info, "1", true},
  };

  for (auto const& test : cases) {
    SCOPED_TRACE("Testing for " + test.name);
    ScopedEnvironment env(
        "GOOGLE_CLOUD_CPP_EXPERIMENTAL_DISABLE_SELF_SIGNED_JWT",
        test.environment);
    EXPECT_EQ(test.expected, ServiceAccountUseOAuth(test.info));
  }
}

}  // namespace
}  // namespace oauth2
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google
