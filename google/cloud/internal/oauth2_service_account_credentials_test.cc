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

#include "google/cloud/internal/oauth2_service_account_credentials.h"
#include "google/cloud/internal/base64_transforms.h"
#include "google/cloud/internal/oauth2_credential_constants.h"
#include "google/cloud/internal/openssl_util.h"
#include "google/cloud/internal/random.h"
#include "google/cloud/testing_util/mock_fake_clock.h"
#include "google/cloud/testing_util/mock_http_payload.h"
#include "google/cloud/testing_util/mock_rest_client.h"
#include "google/cloud/testing_util/mock_rest_response.h"
#include "google/cloud/testing_util/status_matchers.h"
#include "absl/strings/str_split.h"
#include <gmock/gmock.h>
#include <nlohmann/json.hpp>
#include <chrono>

namespace google {
namespace cloud {
namespace oauth2_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::google::cloud::internal::SignUsingSha256;
using ::google::cloud::internal::UrlsafeBase64Decode;
using ::google::cloud::rest_internal::HttpPayload;
using ::google::cloud::testing_util::FakeClock;
using ::google::cloud::testing_util::MockHttpPayload;
using ::google::cloud::testing_util::MockRestClient;
using ::google::cloud::testing_util::MockRestResponse;
using ::google::cloud::testing_util::StatusIs;
using ::testing::_;
using ::testing::A;
using ::testing::ByMove;
using ::testing::Contains;
using ::testing::ElementsAreArray;
using ::testing::HasSubstr;
using ::testing::Not;
using ::testing::Return;

constexpr char kScopeForTest0[] =
    "https://www.googleapis.com/auth/devstorage.full_control";
constexpr char kScopeForTest1[] =
    "https://www.googleapis.com/auth/cloud-platform";
constexpr std::time_t kFixedJwtTimestamp = 1530060324;
constexpr char kGrantParamUnescaped[] =
    "urn:ietf:params:oauth:grant-type:jwt-bearer";
constexpr char kSubjectForGrant[] = "user@foo.bar";

auto constexpr kProjectId = "test-only-project-id";
auto constexpr kPrivateKeyId = "a1a111aa1111a11a11a11aa111a111a1a1111111";
// This is an invalidated private key. It was created using the Google Cloud
// Platform console, but then the key (and service account) were deleted.
auto constexpr kPrivateKey = R"""(-----BEGIN PRIVATE KEY-----
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
auto constexpr kClientEmail =
    "test-only-email@test-only-project-id.iam.gserviceaccount.com";
auto constexpr kClientId = "100000000000000000001";
auto constexpr kAuthUri = "https://accounts.google.com/o/oauth2/auth";
auto constexpr kTokenUri = "https://oauth2.googleapis.com/token";
auto constexpr kAuthProviderX509CerlUrl =
    "https://www.googleapis.com/oauth2/v1/certs";
auto constexpr kClientX509CertUrl =
    "https://www.googleapis.com/robot/v1/metadata/x509/"
    "foo-email%40foo-project.iam.gserviceaccount.com";

std::string MakeTestContents() {
  return nlohmann::json{
      {"type", "service_account"},
      {"project_id", kProjectId},
      {"private_key_id", kPrivateKeyId},
      {"private_key", kPrivateKey},
      {"client_email", kClientEmail},
      {"client_id", kClientId},
      {"auth_uri", kAuthUri},
      {"token_uri", kTokenUri},
      {"auth_provider_x509_cert_url", kAuthProviderX509CerlUrl},
      {"client_x509_cert_url", kClientX509CertUrl},
  }
      .dump();
}

void CheckInfoYieldsExpectedAssertion(std::unique_ptr<MockRestClient> mock,
                                      ServiceAccountCredentialsInfo const& info,
                                      std::string const& assertion) {
  std::string response = R"""({
      "token_type": "Type",
      "access_token": "access-token-value",
      "expires_in": 1234
  })""";

  auto* mock_response1 = new MockRestResponse();
  EXPECT_CALL(*mock_response1, StatusCode)
      .WillRepeatedly(Return(rest_internal::HttpStatusCode::kOk));
  EXPECT_CALL(std::move(*mock_response1), ExtractPayload).WillOnce([&] {
    auto mock_http_payload = absl::make_unique<MockHttpPayload>();
    EXPECT_CALL(*mock_http_payload, Read)
        .WillOnce([&](absl::Span<char> buffer) {
          std::copy(response.begin(), response.end(), buffer.begin());
          return response.size();
        })
        .WillOnce([](absl::Span<char>) { return 0; });
    return std::unique_ptr<HttpPayload>(std::move(mock_http_payload));
  });

  EXPECT_CALL(
      *mock,
      Post(_, A<std::vector<std::pair<std::string, std::string>> const&>()))
      .WillOnce(
          [&](rest_internal::RestRequest const&,
              std::vector<std::pair<std::string, std::string>> const& payload) {
            EXPECT_THAT(payload, Contains(std::pair<std::string, std::string>(
                                     "assertion", assertion)));
            EXPECT_THAT(payload, Contains(std::pair<std::string, std::string>(
                                     "grant_type", kGrantParamUnescaped)));
            return std::unique_ptr<rest_internal::RestResponse>(mock_response1);
          });

  ServiceAccountCredentials credentials(info, Options{}, std::move(mock),
                                        FakeClock::now);
  // Calls Refresh to obtain the access token for our authorization header.
  EXPECT_EQ(std::make_pair(std::string{"Authorization"},
                           std::string{"Type access-token-value"}),
            credentials.AuthorizationHeader().value());
}

TEST(ServiceAccountCredentialsTest, MakeSelfSignedJWT) {
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

  auto signature = internal::SignUsingSha256(
      components[0] + '.' + components[1], info->private_key);
  ASSERT_STATUS_OK(signature);
  EXPECT_THAT(*signature,
              ElementsAreArray(decoded[2].begin(), decoded[2].end()));
}

TEST(ServiceAccountCredentialsTest, MakeSelfSignedJWTWithScopes) {
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

  auto signature = internal::SignUsingSha256(
      components[0] + '.' + components[1], info->private_key);
  ASSERT_STATUS_OK(signature);
  EXPECT_THAT(*signature,
              ElementsAreArray(decoded[2].begin(), decoded[2].end()));
}

/// @test Verify that we can create service account credentials from a keyfile.
TEST(ServiceAccountCredentialsTest,
     RefreshingSendsCorrectRequestBodyAndParsesResponse) {
  auto info = ParseServiceAccountCredentials(MakeTestContents(), "test");
  ASSERT_STATUS_OK(info);
  EXPECT_EQ(info->client_email, kClientEmail);
  EXPECT_EQ(info->private_key_id, kPrivateKeyId);
  EXPECT_EQ(info->private_key, kPrivateKey);
  EXPECT_EQ(info->token_uri, kTokenUri);

  auto const expected_header =
      nlohmann::json{{"alg", "RS256"}, {"typ", "JWT"}, {"kid", kPrivateKeyId}};

  auto const iat = static_cast<std::intmax_t>(kFixedJwtTimestamp);
  auto const exp = iat + 3600;
  auto const expected_payload = nlohmann::json{
      {"iss", kClientEmail},
      {"scope", "https://www.googleapis.com/auth/cloud-platform"},
      {"aud", kTokenUri},
      {"iat", iat},
      {"exp", exp},
  };

  auto const assertion = MakeJWTAssertion(expected_header.dump(),
                                          expected_payload.dump(), kPrivateKey);
  CheckInfoYieldsExpectedAssertion(absl::make_unique<MockRestClient>(), *info,
                                   assertion);
}

/// @test Verify that we can create service account credentials from a keyfile.
TEST(ServiceAccountCredentialsTest,
     RefreshingSendsCorrectRequestBodyAndParsesResponseForNonDefaultVals) {
  auto info = ParseServiceAccountCredentials(MakeTestContents(), "test");
  ASSERT_STATUS_OK(info);
  info->scopes = {kScopeForTest0};
  info->subject = std::string(kSubjectForGrant);

  auto const expected_header =
      nlohmann::json{{"alg", "RS256"}, {"typ", "JWT"}, {"kid", kPrivateKeyId}};

  auto const iat = static_cast<std::intmax_t>(kFixedJwtTimestamp);
  auto const exp = iat + 3600;
  auto const expected_payload = nlohmann::json{
      {"iss", kClientEmail}, {"scope", kScopeForTest0},
      {"aud", kTokenUri},    {"iat", iat},
      {"exp", exp},          {"sub", kSubjectForGrant},
  };

  auto const assertion = MakeJWTAssertion(expected_header.dump(),
                                          expected_payload.dump(), kPrivateKey);
  CheckInfoYieldsExpectedAssertion(absl::make_unique<MockRestClient>(), *info,
                                   assertion);
}

TEST(ServiceAccountCredentialsTest, MultipleScopes) {
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
TEST(ServiceAccountCredentialsTest,
     RefreshCalledOnlyWhenAccessTokenIsMissingOrInvalid) {
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

  auto* mock_response1 = new MockRestResponse();
  EXPECT_CALL(*mock_response1, StatusCode)
      .WillRepeatedly(Return(rest_internal::HttpStatusCode::kOk));
  EXPECT_CALL(std::move(*mock_response1), ExtractPayload).WillOnce([&] {
    auto mock_http_payload = absl::make_unique<MockHttpPayload>();
    EXPECT_CALL(*mock_http_payload, Read)
        .WillOnce([&](absl::Span<char> buffer) {
          std::copy(r1.begin(), r1.end(), buffer.begin());
          return r1.size();
        })
        .WillOnce([](absl::Span<char>) { return 0; });
    return std::unique_ptr<HttpPayload>(std::move(mock_http_payload));
  });

  auto* mock_response2 = new MockRestResponse();
  EXPECT_CALL(*mock_response2, StatusCode)
      .WillRepeatedly(Return(rest_internal::HttpStatusCode::kOk));
  EXPECT_CALL(std::move(*mock_response2), ExtractPayload).WillOnce([&] {
    auto mock_http_payload = absl::make_unique<MockHttpPayload>();
    EXPECT_CALL(*mock_http_payload, Read)
        .WillOnce([&](absl::Span<char> buffer) {
          std::copy(r2.begin(), r2.end(), buffer.begin());
          return r2.size();
        })
        .WillOnce([](absl::Span<char>) { return 0; });
    return std::unique_ptr<HttpPayload>(std::move(mock_http_payload));
  });

  auto mock = absl::make_unique<MockRestClient>();
  EXPECT_CALL(
      *mock,
      Post(_, A<std::vector<std::pair<std::string, std::string>> const&>()))
      .WillOnce(
          [&](rest_internal::RestRequest const&,
              std::vector<std::pair<std::string, std::string>> const& payload) {
            EXPECT_THAT(payload, Contains(std::pair<std::string, std::string>(
                                     "grant_type", kGrantParamUnescaped)));
            return std::unique_ptr<rest_internal::RestResponse>(mock_response1);
          })
      .WillOnce(
          [&](rest_internal::RestRequest const&,
              std::vector<std::pair<std::string, std::string>> const& payload) {
            EXPECT_THAT(payload, Contains(std::pair<std::string, std::string>(
                                     "grant_type", kGrantParamUnescaped)));
            return std::unique_ptr<rest_internal::RestResponse>(mock_response2);
          });

  auto info = ParseServiceAccountCredentials(MakeTestContents(), "test");
  ASSERT_STATUS_OK(info);
  ServiceAccountCredentials credentials(*info, {}, std::move(mock));
  // Calls Refresh to obtain the access token for our authorization header.
  EXPECT_EQ(std::make_pair(std::string{"Authorization"},
                           std::string{"Type access-token-r1"}),
            credentials.AuthorizationHeader().value());
  // Token is expired, resulting in another call to Refresh.
  EXPECT_EQ(std::make_pair(std::string{"Authorization"},
                           std::string{"Type access-token-r2"}),
            credentials.AuthorizationHeader().value());
  // Token still valid; should return cached token instead of calling Refresh.
  EXPECT_EQ(std::make_pair(std::string{"Authorization"},
                           std::string{"Type access-token-r2"}),
            credentials.AuthorizationHeader().value());
}

/// @test Verify that `nlohmann::json::parse()` failures are reported as
/// is_discarded.
TEST(ServiceAccountCredentialsTest, ParseInvalidJson) {
  std::string config = R"""( not-a-valid-json-string )""";
  // The documentation for `nlohmann::json::parse()` is a bit ambiguous, so
  // wrote a little test to verify it works as I expected.
  auto parsed = nlohmann::json::parse(config, nullptr, false);
  EXPECT_TRUE(parsed.is_discarded());
  EXPECT_FALSE(parsed.is_null());
}

/// @test Verify that parsing a service account JSON string works.
TEST(ServiceAccountCredentialsTest, ParseSimple) {
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
TEST(ServiceAccountCredentialsTest, ParseUsesExplicitDefaultTokenUri) {
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
TEST(ServiceAccountCredentialsTest, ParseUsesImplicitDefaultTokenUri) {
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
TEST(ServiceAccountCredentialsTest, ParseInvalidContentsFails) {
  std::string config = R"""( not-a-valid-json-string )""";

  auto actual = ParseServiceAccountCredentials(config, "test-as-a-source");
  EXPECT_THAT(actual,
              StatusIs(Not(StatusCode::kOk),
                       AllOf(HasSubstr("Invalid ServiceAccountCredentials"),
                             HasSubstr("test-as-a-source"))));
}

/// @test Parsing a service account JSON string should detect empty fields.
TEST(ServiceAccountCredentialsTest, ParseEmptyFieldFails) {
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
TEST(ServiceAccountCredentialsTest, ParseMissingFieldFails) {
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
TEST(ServiceAccountCredentialsTest, ParseOptionalField) {
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

/// @test Verify that refreshing a credential updates the timestamps.
TEST(ServiceAccountCredentialsTest, RefreshingUpdatesTimestamps) {
  auto info = ParseServiceAccountCredentials(MakeTestContents(), "test");
  ASSERT_STATUS_OK(info);

  // NOLINTNEXTLINE(google-runtime-int)
  auto request_assertion =
      [&info](int timestamp,
              std::vector<std::pair<std::string, std::string>> const& p) {
        EXPECT_THAT(p, Contains(std::pair<std::string, std::string>(
                           "grant_type", kGrantParamUnescaped)));

        auto assertion =
            std::find_if(p.begin(), p.end(),
                         [](std::pair<std::string, std::string> const& e) {
                           return e.first == "assertion";
                         });
        ASSERT_NE(assertion, p.end());

        std::vector<std::string> const tokens =
            absl::StrSplit(assertion->second, '.');
        std::string const& encoded_header = tokens[0];
        std::string const& encoded_payload = tokens[1];

        auto header_bytes =
            internal::UrlsafeBase64Decode(encoded_header).value();
        std::string header_str{header_bytes.begin(), header_bytes.end()};
        auto payload_bytes =
            internal::UrlsafeBase64Decode(encoded_payload).value();
        std::string payload_str{payload_bytes.begin(), payload_bytes.end()};

        auto header = nlohmann::json::parse(header_str);
        EXPECT_EQ("RS256", header.value("alg", ""));
        EXPECT_EQ("JWT", header.value("typ", ""));
        EXPECT_EQ(info->private_key_id, header.value("kid", ""));

        auto payload = nlohmann::json::parse(payload_str);
        EXPECT_EQ(timestamp, payload.value("iat", 0));
        EXPECT_EQ(timestamp + 3600, payload.value("exp", 0));
        EXPECT_EQ(info->client_email, payload.value("iss", ""));
        EXPECT_EQ(info->token_uri, payload.value("aud", ""));
      };

  // Set up the mock request / response for the first Refresh().
  auto const clock_value_1 = 10000;
  auto const clock_value_2 = 20000;

  auto response_fn = [&](int timestamp, absl::Span<char>& buffer) {
    std::string token = "mock-token-value-" + std::to_string(timestamp);
    nlohmann::json response{{"token_type", "Mock-Type"},
                            {"access_token", token},
                            {"expires_in", 3600}};
    auto r = response.dump();
    std::copy(r.begin(), r.end(), buffer.begin());
    return r.size();
  };

  auto* mock_response1 = new MockRestResponse();
  EXPECT_CALL(*mock_response1, StatusCode)
      .WillRepeatedly(Return(rest_internal::HttpStatusCode::kOk));
  EXPECT_CALL(std::move(*mock_response1), ExtractPayload).WillOnce([&] {
    auto mock_http_payload = absl::make_unique<MockHttpPayload>();
    EXPECT_CALL(*mock_http_payload, Read)
        .WillOnce([&](absl::Span<char> buffer) {
          return response_fn(clock_value_1, buffer);
        })
        .WillOnce([](absl::Span<char>) { return 0; });
    return std::unique_ptr<HttpPayload>(std::move(mock_http_payload));
  });

  auto* mock_response2 = new MockRestResponse();
  EXPECT_CALL(*mock_response2, StatusCode)
      .WillRepeatedly(Return(rest_internal::HttpStatusCode::kOk));
  EXPECT_CALL(std::move(*mock_response2), ExtractPayload).WillOnce([&] {
    auto mock_http_payload = absl::make_unique<MockHttpPayload>();
    EXPECT_CALL(*mock_http_payload, Read)
        .WillOnce([&](absl::Span<char> buffer) {
          return response_fn(clock_value_2, buffer);
        })
        .WillOnce([](absl::Span<char>) { return 0; });
    return std::unique_ptr<HttpPayload>(std::move(mock_http_payload));
  });

  auto mock = absl::make_unique<MockRestClient>();
  EXPECT_CALL(
      *mock,
      Post(_, A<std::vector<std::pair<std::string, std::string>> const&>()))
      .WillOnce(
          [&](rest_internal::RestRequest const&,
              std::vector<std::pair<std::string, std::string>> const& payload) {
            request_assertion(clock_value_1, payload);
            return std::unique_ptr<rest_internal::RestResponse>(mock_response1);
          })
      .WillOnce(
          [&](rest_internal::RestRequest const&,
              std::vector<std::pair<std::string, std::string>> const& payload) {
            request_assertion(clock_value_2, payload);
            return std::unique_ptr<rest_internal::RestResponse>(mock_response2);
          });

  FakeClock::now_value_ = clock_value_1;
  ServiceAccountCredentials credentials(*info, {}, std::move(mock),
                                        FakeClock::now);
  // Call Refresh to obtain the access token for our authorization header.
  auto authorization_header = credentials.AuthorizationHeader();
  ASSERT_STATUS_OK(authorization_header);
  EXPECT_EQ(std::make_pair(std::string{"Authorization"},
                           std::string{"Mock-Type mock-token-value-10000"}),
            authorization_header.value());

  // Advance the clock past the expiration time of the token and then get a
  // new header.
  FakeClock::now_value_ = clock_value_2;
  EXPECT_GT(clock_value_2 - clock_value_1, 2 * 3600);
  authorization_header = credentials.AuthorizationHeader();
  ASSERT_STATUS_OK(authorization_header);
  EXPECT_EQ(std::make_pair(std::string{"Authorization"},
                           std::string{"Mock-Type mock-token-value-20000"}),
            authorization_header.value());
}

/// @test Verify that we can create sign blobs using a service account.
TEST(ServiceAccountCredentialsTest, SignBlob) {
  auto info = ParseServiceAccountCredentials(MakeTestContents(), "test");
  ASSERT_STATUS_OK(info);
  ServiceAccountCredentials credentials(*info);

  std::string blob = R"""(GET
rmYdCNHKFXam78uCt7xQLw==
text/plain
1388534400
x-goog-encryption-algorithm:AES256
x-goog-meta-foo:bar,baz
/bucket/objectname)""";

  auto actual = credentials.SignBlob(info->client_email, blob);
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
  internal::Base64Encoder encoder;
  for (auto const& c : *actual) {
    encoder.PushBack(c);
  }
  EXPECT_EQ(expected_signed, std::move(encoder).FlushAndPad());
}

/// @test Verify that signing blobs fails with invalid e-mail.
TEST(ServiceAccountCredentialsTest, SignBlobFailure) {
  auto info = ParseServiceAccountCredentials(MakeTestContents(), "test");
  ASSERT_STATUS_OK(info);
  ServiceAccountCredentials credentials(*info);

  auto actual = credentials.SignBlob("fake@fake.com", "test-blob");
  EXPECT_THAT(
      actual,
      StatusIs(StatusCode::kInvalidArgument,
               HasSubstr("The current_credentials cannot sign blobs for ")));
}

/// @test Verify that we can get the client id from a service account.
TEST(ServiceAccountCredentialsTest, ClientId) {
  auto info = ParseServiceAccountCredentials(MakeTestContents(), "test");
  ASSERT_STATUS_OK(info);
  ServiceAccountCredentials credentials(*info);

  EXPECT_EQ(kClientEmail, credentials.AccountEmail());
  EXPECT_EQ(kPrivateKeyId, credentials.KeyId());
}

/// @test Verify we can obtain JWT assertion components given the info parsed
/// from a keyfile.
TEST(ServiceAccountCredentialsTest, AssertionComponentsFromInfo) {
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
TEST(ServiceAccountCredentialsTest, MakeJWTAssertion) {
  auto info = ParseServiceAccountCredentials(MakeTestContents(), "test");
  ASSERT_STATUS_OK(info);
  FakeClock::reset_clock(kFixedJwtTimestamp);
  auto const tp = FakeClock::now();

  auto components = AssertionComponentsFromInfo(*info, tp);
  auto assertion =
      MakeJWTAssertion(components.first, components.second, info->private_key);

  std::vector<std::string> actual_tokens = absl::StrSplit(assertion, '.');
  ASSERT_EQ(actual_tokens.size(), 3);
  std::vector<std::vector<std::uint8_t>> decoded(actual_tokens.size());
  std::transform(
      actual_tokens.begin(), actual_tokens.end(), decoded.begin(),
      [](std::string const& e) { return UrlsafeBase64Decode(e).value(); });

  // Verify this is a valid key.
  auto const signature =
      SignUsingSha256(actual_tokens[0] + '.' + actual_tokens[1], kPrivateKey);
  ASSERT_STATUS_OK(signature);
  EXPECT_EQ(*signature, decoded[2]);

  // Verify the header and payloads are valid.
  auto const header =
      nlohmann::json::parse(decoded[0].begin(), decoded[0].end());
  auto const expected_header =
      nlohmann::json{{"alg", "RS256"}, {"typ", "JWT"}, {"kid", kPrivateKeyId}};
  EXPECT_EQ(header, expected_header);

  auto const payload = nlohmann::json::parse(decoded[1]);
  auto const iat = static_cast<std::intmax_t>(kFixedJwtTimestamp);
  auto const exp = iat + 3600;
  auto const expected_payload = nlohmann::json{
      {"iss", kClientEmail},
      {"scope", "https://www.googleapis.com/auth/cloud-platform"},
      {"aud", kTokenUri},
      {"iat", iat},
      {"exp", exp},
  };

  EXPECT_EQ(payload, expected_payload);
}

/// @test Verify we can construct a service account refresh payload given the
/// info parsed from a keyfile.
TEST(ServiceAccountCredentialsTest, CreateServiceAccountRefreshPayload) {
  auto info = ParseServiceAccountCredentials(MakeTestContents(), "test");
  ASSERT_STATUS_OK(info);
  FakeClock::reset_clock(kFixedJwtTimestamp);
  auto components = AssertionComponentsFromInfo(*info, FakeClock::now());
  auto assertion =
      MakeJWTAssertion(components.first, components.second, info->private_key);
  auto actual_payload =
      CreateServiceAccountRefreshPayload(*info, FakeClock::now());

  EXPECT_THAT(actual_payload, Contains(std::pair<std::string, std::string>(
                                  "assertion", assertion)));
  EXPECT_THAT(actual_payload, Contains(std::pair<std::string, std::string>(
                                  "grant_type", kGrantParamUnescaped)));
}

/// @test Parsing a refresh response with missing fields results in failure.
TEST(ServiceAccountCredentialsTest,
     ParseServiceAccountRefreshResponseMissingFields) {
  std::string r1 = R"""({})""";
  // Does not have access_token.
  std::string r2 = R"""({
    "token_type": "Type",
    "id_token": "id-token-value",
    "expires_in": 1000
})""";

  std::unique_ptr<MockHttpPayload> mock_http_payload1(new MockHttpPayload());
  EXPECT_CALL(*mock_http_payload1, Read)
      .WillOnce([&](absl::Span<char> buffer) {
        std::copy(r1.begin(), r1.end(), buffer.begin());
        return r1.size();
      })
      .WillOnce([](absl::Span<char>) { return 0; });

  auto mock_response1 = absl::make_unique<MockRestResponse>();
  EXPECT_CALL(*mock_response1, StatusCode)
      .WillRepeatedly(Return(rest_internal::HttpStatusCode::kBadRequest));
  EXPECT_CALL(std::move(*mock_response1), ExtractPayload)
      .WillOnce(Return(ByMove(std::move(mock_http_payload1))));

  std::unique_ptr<MockHttpPayload> mock_http_payload2(new MockHttpPayload());
  EXPECT_CALL(*mock_http_payload2, Read)
      .WillOnce([&](absl::Span<char> buffer) {
        std::copy(r2.begin(), r2.end(), buffer.begin());
        return r2.size();
      })
      .WillOnce([](absl::Span<char>) { return 0; });

  auto mock_response2 = absl::make_unique<MockRestResponse>();
  EXPECT_CALL(*mock_response2, StatusCode)
      .WillRepeatedly(Return(rest_internal::HttpStatusCode::kBadRequest));
  EXPECT_CALL(std::move(*mock_response2), ExtractPayload)
      .WillOnce(Return(ByMove(std::move(mock_http_payload2))));

  FakeClock::reset_clock(1000);
  auto status =
      ParseServiceAccountRefreshResponse(*mock_response1, FakeClock::now());
  EXPECT_THAT(status,
              StatusIs(StatusCode::kInvalidArgument,
                       HasSubstr("Could not find all required fields")));

  status =
      ParseServiceAccountRefreshResponse(*mock_response2, FakeClock::now());
  EXPECT_THAT(status,
              StatusIs(StatusCode::kInvalidArgument,
                       HasSubstr("Could not find all required fields")));
}

/// @test Parsing a refresh response yields a TemporaryToken.
TEST(ServiceAccountCredentialsTest, ParseServiceAccountRefreshResponse) {
  std::string r1 = R"""({
    "token_type": "Type",
    "access_token": "access-token-r1",
    "expires_in": 1000
})""";

  std::unique_ptr<MockHttpPayload> mock_http_payload1(new MockHttpPayload());
  EXPECT_CALL(*mock_http_payload1, Read)
      .WillOnce([&](absl::Span<char> buffer) {
        std::copy(r1.begin(), r1.end(), buffer.begin());
        return r1.size();
      })
      .WillOnce([](absl::Span<char>) { return 0; });

  auto mock_response1 = absl::make_unique<MockRestResponse>();
  EXPECT_CALL(*mock_response1, StatusCode)
      .WillRepeatedly(Return(rest_internal::HttpStatusCode::kOk));
  EXPECT_CALL(std::move(*mock_response1), ExtractPayload)
      .WillOnce(Return(ByMove(std::move(mock_http_payload1))));

  auto expires_in = 1000;
  FakeClock::reset_clock(2000);
  auto status =
      ParseServiceAccountRefreshResponse(*mock_response1, FakeClock::now());
  EXPECT_STATUS_OK(status);
  auto token = *status;
  EXPECT_EQ(
      std::chrono::time_point_cast<std::chrono::seconds>(token.expiration_time)
          .time_since_epoch()
          .count(),
      FakeClock::now_value_ + expires_in);
  EXPECT_EQ(token.token, std::make_pair(std::string{"Authorization"},
                                        std::string{"Type access-token-r1"}));
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace oauth2_internal
}  // namespace cloud
}  // namespace google
