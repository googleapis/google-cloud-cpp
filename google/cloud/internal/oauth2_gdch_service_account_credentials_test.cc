// Copyright 2026 Google LLC
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

#include "google/cloud/internal/oauth2_gdch_service_account_credentials.h"
#include "google/cloud/credentials.h"
#include "google/cloud/internal/base64_transforms.h"
#include "google/cloud/internal/oauth2_universe_domain.h"
#include "google/cloud/internal/random.h"
#include "google/cloud/internal/sign_using_sha256.h"
#include "google/cloud/testing_util/chrono_output.h"
#include "google/cloud/testing_util/mock_http_payload.h"
#include "google/cloud/testing_util/mock_rest_client.h"
#include "google/cloud/testing_util/mock_rest_response.h"
#include "google/cloud/testing_util/status_matchers.h"
#include "absl/strings/str_cat.h"
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
using ::google::cloud::rest_internal::RestRequest;
using ::google::cloud::testing_util::IsOkAndHolds;
using ::google::cloud::testing_util::MakeMockHttpPayloadSuccess;
using ::google::cloud::testing_util::MockRestClient;
using ::google::cloud::testing_util::MockRestResponse;
using ::google::cloud::testing_util::StatusIs;
using ::testing::_;
using ::testing::AllOf;
using ::testing::ByMove;
using ::testing::Contains;
using ::testing::Eq;
using ::testing::HasSubstr;
using ::testing::MatcherCast;
using ::testing::Not;
using ::testing::Pair;
using ::testing::Property;
using ::testing::Return;

using MockHttpClientFactory =
    ::testing::MockFunction<std::unique_ptr<rest_internal::RestClient>(
        Options const&)>;

auto constexpr kFixedJwtTimestamp = 1530060324;
auto constexpr kAudience = "test-audience";
auto constexpr kProjectId = "test-project-id";
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
auto constexpr kServiceIdentityName = "test-service-identity";
auto constexpr kCaCertPath = "/test/ca.crt";
auto constexpr kTokenUri = "https://gdc.token.uri/v1/token";

nlohmann::json TestContents() {
  return nlohmann::json{
      {"project_id", kProjectId},    {"private_key_id", kPrivateKeyId},
      {"private_key", kPrivateKey},  {"name", kServiceIdentityName},
      {"ca_cert_path", kCaCertPath}, {"token_uri", kTokenUri},
  };
}

std::string MakeTestContents() { return TestContents().dump(); }

/// @test Verify that we can create service account credentials from a keyfile.
TEST(GDCHServiceAccountCredentialsTest,
     RefreshingSendsCorrectRequestBodyAndParsesResponse) {
  auto const post_response = std::string{R"""({
    "access_token":"access-token-value",
    "issued_token_type":"urn:ietf:params:oauth:token-type:access_token",
    "token_type":"Bearer",
    "expires_in":3599
  })"""};

  auto const expected_header =
      nlohmann::json{{"alg", "RS256"}, {"typ", "JWT"}, {"kid", kPrivateKeyId}};
  auto const iat = static_cast<std::intmax_t>(kFixedJwtTimestamp);
  auto const exp = iat + 3600;
  auto const iss_sub_value = absl::StrCat("system:serviceaccount:", kProjectId,
                                          ":", kServiceIdentityName);
  auto const expected_payload = nlohmann::json{
      {"iss", iss_sub_value}, {"sub", iss_sub_value}, {"aud", kTokenUri},
      {"iat", iat},           {"exp", exp},
  };
  auto const assertion = GDCHServiceAccountCredentials::MakeJWTAssertion(
      expected_header.dump(), expected_payload.dump(), kPrivateKey);

  auto token_client = [=] {
    using FormDataType = std::vector<std::pair<std::string, std::string>>;
    auto mock = std::make_unique<MockRestClient>();
    auto expected_request = Property(&RestRequest::path, kTokenUri);
    auto expected_form_data = MatcherCast<FormDataType const&>(AllOf(
        Contains(Pair("grant_type",
                      "urn:ietf:params:oauth:token-type:token-exchange")),
        Contains(Pair("audience", kAudience)),
        Contains(Pair("requested_token_type",
                      "urn:ietf:params:oauth:token-type:access_token")),
        Contains(Pair("subject_token", assertion)),
        Contains(Pair("subject_token_type",
                      "urn:k8s:params:oauth:token-type:serviceaccount"))));
    EXPECT_CALL(*mock, Post(_, expected_request, expected_form_data))
        .WillOnce([post_response]() {
          auto response = std::make_unique<MockRestResponse>();
          EXPECT_CALL(*response, StatusCode)
              .WillRepeatedly(Return(rest_internal::HttpStatusCode::kOk));
          EXPECT_CALL(std::move(*response), ExtractPayload)
              .WillOnce(
                  Return(ByMove(MakeMockHttpPayloadSuccess(post_response))));
          return std::unique_ptr<rest_internal::RestResponse>(
              std::move(response));
        });
    return mock;
  }();

  MockHttpClientFactory mock_client_factory;
  EXPECT_CALL(mock_client_factory, Call)
      .WillOnce(Return(ByMove(std::move(token_client))));

  auto credentials = GDCHServiceAccountCredentials::CreateFromJsonContents(
      MakeTestContents(), Options{}.set<AudienceOption>(kAudience),
      mock_client_factory.AsStdFunction());
  ASSERT_STATUS_OK(credentials);
  auto const tp = std::chrono::system_clock::from_time_t(kFixedJwtTimestamp);
  // Calls Refresh to obtain the access token for our authorization header.
  auto token = (*credentials)->GetToken(tp);
  ASSERT_STATUS_OK(token);
  EXPECT_THAT(token->token, Eq("access-token-value"));
  EXPECT_THAT(token->expiration, Eq(tp + std::chrono::seconds(3599)));

  EXPECT_THAT((*credentials)->AccountEmail(), Eq(kServiceIdentityName));
  EXPECT_THAT((*credentials)->KeyId(), Eq(kPrivateKeyId));
}

/// @test Verify that `nlohmann::json::parse()` failures are reported as
/// is_discarded.
TEST(GDCHServiceAccountCredentialsTest, ParseInvalidJson) {
  std::string config = R"""( not-a-valid-json-string )""";
  // The documentation for `nlohmann::json::parse()` is a bit ambiguous, so
  // wrote a little test to verify it works as I expected.
  auto parsed = nlohmann::json::parse(config, nullptr, false);
  EXPECT_TRUE(parsed.is_discarded());
  EXPECT_FALSE(parsed.is_null());
}

/// @test Verify that parsing a service account JSON string works.
TEST(GDCHServiceAccountCredentialsTest, ParseSimple) {
  std::string contents = R"""({
      "project_id": "test-project-id",
      "private_key_id": "not-a-key-id-just-for-testing",
      "private_key": "not-a-valid-key-just-for-testing",
      "name": "test-service-identity",
      "ca_cert_path": "/test/ca.crt",
      "token_uri": "https://gdc.token.uri/v1/token"
})""";

  auto actual = GDCHServiceAccountCredentials::Parse(contents, "test-data");
  ASSERT_STATUS_OK(actual);
  EXPECT_THAT(actual->project_id, Eq("test-project-id"));
  EXPECT_THAT(actual->private_key_id, Eq("not-a-key-id-just-for-testing"));
  EXPECT_THAT(actual->private_key, Eq("not-a-valid-key-just-for-testing"));
  EXPECT_THAT(actual->service_identity_name, Eq("test-service-identity"));
  EXPECT_THAT(actual->ca_cert_path, Eq("/test/ca.crt"));
  EXPECT_THAT(actual->token_uri, Eq("https://gdc.token.uri/v1/token"));
}

/// @test Verify that invalid contents result in a readable error.
TEST(GDCHServiceAccountCredentialsTest, ParseInvalidContentsFails) {
  std::string config = R"""( not-a-valid-json-string )""";

  auto actual =
      GDCHServiceAccountCredentials::Parse(config, "test-as-a-source");
  EXPECT_THAT(actual,
              StatusIs(Not(StatusCode::kOk),
                       AllOf(HasSubstr("Invalid GDCHServiceAccountCredentials"),
                             HasSubstr("test-as-a-source"))));
}

/// @test Parsing a service account JSON string should detect empty fields.
TEST(GDCHServiceAccountCredentialsTest, ParseEmptyFieldFails) {
  std::string contents = R"""({
      "project_id": "test-project-id",
      "private_key_id": "not-a-key-id-just-for-testing",
      "private_key": "not-a-valid-key-just-for-testing",
      "name": "test-service-identity",
      "ca_cert_path": "/test/ca.crt",
      "token_uri": "https://gdc.token.uri/v1/token"
})""";

  for (auto const& field : {"project_id", "private_key_id", "private_key",
                            "name", "ca_cert_path", "token_uri"}) {
    auto json = nlohmann::json::parse(contents);
    json[field] = "";
    auto actual =
        GDCHServiceAccountCredentials::Parse(json.dump(), "test-data");
    EXPECT_THAT(actual,
                StatusIs(Not(StatusCode::kOk),
                         AllOf(HasSubstr(field), HasSubstr(" field is empty"),
                               HasSubstr("test-data"))));
  }
}

/// @test Parsing a service account JSON string should detect invalid fields.
TEST(GDCHServiceAccountCredentialsTest, ParseInvalidTypeFieldFails) {
  std::string contents = R"""({
      "project_id": "test-project-id",
      "private_key_id": "not-a-key-id-just-for-testing",
      "private_key": "not-a-valid-key-just-for-testing",
      "name": "test-service-identity",
      "ca_cert_path": "/test/ca.crt",
      "token_uri": "https://gdc.token.uri/v1/token"
})""";

  for (auto const& field : {"project_id", "private_key_id", "private_key",
                            "name", "ca_cert_path", "token_uri"}) {
    auto json = nlohmann::json::parse(contents);
    json[field] = true;
    auto actual =
        GDCHServiceAccountCredentials::Parse(json.dump(), "test-data");
    EXPECT_THAT(
        actual,
        StatusIs(Not(StatusCode::kOk),
                 AllOf(HasSubstr(field),
                       HasSubstr(" field is present and is not a string"),
                       HasSubstr("test-data"))));
  }
}

/// @test Parsing a service account JSON string should detect missing fields.
TEST(GDCHServiceAccountCredentialsTest, ParseMissingFieldFails) {
  std::string contents = R"""({
      "project_id": "test-project-id",
      "private_key_id": "not-a-key-id-just-for-testing",
      "private_key": "not-a-valid-key-just-for-testing",
      "name": "test-service-identity",
      "ca_cert_path": "/test/ca.crt",
      "token_uri": "https://gdc.token.uri/v1/token"
})""";

  for (auto const& field : {"project_id", "private_key_id", "private_key",
                            "name", "ca_cert_path", "token_uri"}) {
    auto json = nlohmann::json::parse(contents);
    json.erase(field);
    auto actual =
        GDCHServiceAccountCredentials::Parse(json.dump(), "test-data");
    EXPECT_THAT(actual,
                StatusIs(Not(StatusCode::kOk),
                         AllOf(HasSubstr(field), HasSubstr(" field is missing"),
                               HasSubstr("test-data"))));
  }
}

TEST(GDCHServiceAccountCredentialsTest, ProjectIdDefined) {
  MockHttpClientFactory mock_http_client_factory;
  EXPECT_CALL(mock_http_client_factory, Call).Times(0);

  auto credentials = GDCHServiceAccountCredentials::CreateFromJsonContents(
      MakeTestContents(), Options{}.set<AudienceOption>(kAudience),
      mock_http_client_factory.AsStdFunction());
  ASSERT_STATUS_OK(credentials);
  EXPECT_THAT((*credentials)->project_id(), IsOkAndHolds(kProjectId));
  EXPECT_THAT((*credentials)->project_id({}), IsOkAndHolds(kProjectId));
}

TEST(GDCHServiceAccountCredentialsTest, MissingAudienceOption) {
  MockHttpClientFactory mock_http_client_factory;
  EXPECT_CALL(mock_http_client_factory, Call).Times(0);

  auto credentials = GDCHServiceAccountCredentials::CreateFromJsonContents(
      MakeTestContents(), Options{}, mock_http_client_factory.AsStdFunction());
  EXPECT_THAT(credentials,
              StatusIs(StatusCode::kInvalidArgument,
                       HasSubstr("requires the AudienceOption to be set")));
}

/// @test Verify we can obtain JWT assertion components given the info parsed
/// from a keyfile.
TEST(GDCHServiceAccountCredentialsTest, AssertionComponentsFromInfo) {
  auto info = GDCHServiceAccountCredentials::Parse(MakeTestContents(), "test");
  ASSERT_STATUS_OK(info);
  auto const now = std::chrono::system_clock::now();
  auto components =
      GDCHServiceAccountCredentials::AssertionComponentsFromInfo(*info, now);

  auto header = nlohmann::json::parse(components.first);
  EXPECT_THAT(header.value("alg", ""), Eq("RS256"));
  EXPECT_THAT(header.value("typ", ""), Eq("JWT"));
  EXPECT_THAT(header.value("kid", ""), Eq(info->private_key_id));

  auto payload = nlohmann::json::parse(components.second);
  EXPECT_THAT(payload.value("iat", 0),
              Eq(std::chrono::system_clock::to_time_t(now)));
  EXPECT_THAT(payload.value("exp", 0), Eq(std::chrono::system_clock::to_time_t(
                                           now + std::chrono::seconds(3600))));
  auto const iss_sub_value = absl::StrCat("system:serviceaccount:", kProjectId,
                                          ":", kServiceIdentityName);
  EXPECT_THAT(payload.value("iss", ""), Eq(iss_sub_value));
  EXPECT_THAT(payload.value("sub", ""), Eq(iss_sub_value));
  EXPECT_THAT(payload.value("aud", ""), Eq(info->token_uri));
}

/// @test Verify we can construct a JWT assertion given the info parsed from a
/// keyfile.
TEST(GDCHServiceAccountCredentialsTest, MakeGDCHJWTAssertion) {
  auto info = GDCHServiceAccountCredentials::Parse(MakeTestContents(), "test");
  ASSERT_STATUS_OK(info);

  auto const tp = std::chrono::system_clock::from_time_t(kFixedJwtTimestamp);
  auto components =
      GDCHServiceAccountCredentials::AssertionComponentsFromInfo(*info, tp);
  auto assertion = GDCHServiceAccountCredentials::MakeJWTAssertion(
      components.first, components.second, info->private_key);

  std::vector<std::string> actual_tokens = absl::StrSplit(assertion, '.');
  ASSERT_THAT(actual_tokens.size(), Eq(3));
  std::vector<std::vector<std::uint8_t>> decoded(actual_tokens.size());
  std::transform(
      actual_tokens.begin(), actual_tokens.end(), decoded.begin(),
      [](std::string const& e) { return UrlsafeBase64Decode(e).value(); });

  // Verify this is a valid key.
  auto const signature =
      SignUsingSha256(actual_tokens[0] + '.' + actual_tokens[1], kPrivateKey);
  ASSERT_STATUS_OK(signature);
  EXPECT_THAT(*signature, Eq(decoded[2]));

  // Verify the header and payloads are valid.
  auto const header =
      nlohmann::json::parse(decoded[0].begin(), decoded[0].end());
  auto const expected_header =
      nlohmann::json{{"alg", "RS256"}, {"typ", "JWT"}, {"kid", kPrivateKeyId}};
  EXPECT_THAT(header, Eq(expected_header));

  auto const payload = nlohmann::json::parse(decoded[1]);
  auto const iat = static_cast<std::intmax_t>(kFixedJwtTimestamp);
  auto const exp = iat + 3600;
  auto const iss_sub_value = absl::StrCat("system:serviceaccount:", kProjectId,
                                          ":", kServiceIdentityName);
  auto const expected_payload = nlohmann::json{
      {"iss", iss_sub_value}, {"sub", iss_sub_value}, {"aud", kTokenUri},
      {"iat", iat},           {"exp", exp},
  };

  EXPECT_THAT(payload, Eq(expected_payload));
}

/// @test Verify we can construct a service account refresh payload given the
/// info parsed from a keyfile.
TEST(GDCHServiceAccountCredentialsTest,
     CreateGDCHServiceAccountRefreshPayload) {
  auto info = GDCHServiceAccountCredentials::Parse(MakeTestContents(), "test");
  ASSERT_STATUS_OK(info);
  info->audience = kAudience;
  auto const now = std::chrono::system_clock::now();
  auto components =
      GDCHServiceAccountCredentials::AssertionComponentsFromInfo(*info, now);
  auto assertion = GDCHServiceAccountCredentials::MakeJWTAssertion(
      components.first, components.second, info->private_key);
  auto actual_payload =
      GDCHServiceAccountCredentials::CreateRefreshPayload(*info, now);

  EXPECT_THAT(
      actual_payload,
      Contains(Pair("grant_type",
                    "urn:ietf:params:oauth:token-type:token-exchange")));
  EXPECT_THAT(actual_payload, Contains(Pair("audience", kAudience)));
  EXPECT_THAT(actual_payload,
              Contains(Pair("requested_token_type",
                            "urn:ietf:params:oauth:token-type:access_token")));
  EXPECT_THAT(actual_payload, Contains(Pair("subject_token", assertion)));
  EXPECT_THAT(actual_payload,
              Contains(Pair("subject_token_type",
                            "urn:k8s:params:oauth:token-type:serviceaccount")));
}

/// @test Parsing a refresh response with missing fields results in failure.
TEST(GDCHServiceAccountCredentialsTest,
     ParseGDCHServiceAccountRefreshResponseMissingFields) {
  std::string r1 = R"""({})""";
  // Does not have access_token.
  std::string r2 = R"""({
    "issued_token_type":"urn:ietf:params:oauth:token-type:access_token",
    "token_type":"Bearer",
    "expires_in":3599
})""";

  auto mock_response1 = std::make_unique<MockRestResponse>();
  EXPECT_CALL(*mock_response1, StatusCode)
      .WillRepeatedly(Return(rest_internal::HttpStatusCode::kBadRequest));
  EXPECT_CALL(std::move(*mock_response1), ExtractPayload)
      .WillOnce(Return(ByMove(MakeMockHttpPayloadSuccess(r1))));

  auto mock_response2 = std::make_unique<MockRestResponse>();
  EXPECT_CALL(*mock_response2, StatusCode)
      .WillRepeatedly(Return(rest_internal::HttpStatusCode::kBadRequest));
  EXPECT_CALL(std::move(*mock_response2), ExtractPayload)
      .WillOnce(Return(ByMove(MakeMockHttpPayloadSuccess(r2))));

  auto const now = std::chrono::system_clock::now();
  auto status =
      GDCHServiceAccountCredentials::ParseRefreshResponse(*mock_response1, now);
  EXPECT_THAT(status,
              StatusIs(StatusCode::kInvalidArgument,
                       HasSubstr("Could not find all required fields")));

  status =
      GDCHServiceAccountCredentials::ParseRefreshResponse(*mock_response2, now);
  EXPECT_THAT(status,
              StatusIs(StatusCode::kInvalidArgument,
                       HasSubstr("Could not find all required fields")));
}

/// @test Parsing a refresh response yields an access token.
TEST(GDCHServiceAccountCredentialsTest,
     ParseGDCHServiceAccountRefreshResponse) {
  auto const expires_in = std::chrono::seconds(1000);
  std::string r1 = R"""({
    "access_token":"access-token-r1",
    "issued_token_type":"urn:ietf:params:oauth:token-type:access_token",
    "token_type":"Bearer",
    "expires_in":1000
})""";

  auto mock_response1 = std::make_unique<MockRestResponse>();
  EXPECT_CALL(*mock_response1, StatusCode)
      .WillRepeatedly(Return(rest_internal::HttpStatusCode::kOk));
  EXPECT_CALL(std::move(*mock_response1), ExtractPayload)
      .WillOnce(Return(ByMove(MakeMockHttpPayloadSuccess(r1))));

  auto const now = std::chrono::system_clock::now();
  auto token =
      GDCHServiceAccountCredentials::ParseRefreshResponse(*mock_response1, now);
  ASSERT_STATUS_OK(token);
  EXPECT_THAT(token->expiration, Eq(now + expires_in));
  EXPECT_THAT(token->token, Eq("access-token-r1"));
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace oauth2_internal
}  // namespace cloud
}  // namespace google
