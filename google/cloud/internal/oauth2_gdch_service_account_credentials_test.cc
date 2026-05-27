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
using ::testing::Eq;
using ::testing::HasSubstr;
using ::testing::IsEmpty;
using ::testing::MatcherCast;
using ::testing::Not;
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
auto constexpr kPrivateKey = R"""(-----BEGIN EC PRIVATE KEY-----
MHcCAQEEIDGD4hNeIBG3lo4BKS1k4jpYhbnJSZwAuUwyK8wEiOP5oAoGCCqGSM49
AwEHoUQDQgAEWK7gDAGAAzOfl6pHhpmvjbTeUPyclQk7+HgAWE6uGUtox/U8/sQQ
X3IM7YomoAWiNKWwBVskpXWj7L9dLkqhyQ==
-----END EC PRIVATE KEY-----
)""";
auto constexpr kServiceIdentityName = "test-service-identity";
auto constexpr kCaCertPath = "/test/ca.crt";
auto constexpr kTokenUri = "https://gdc.token.uri/v1/token";

nlohmann::json TestContents() {
  return nlohmann::json{
      {"project", kProjectId},       {"private_key_id", kPrivateKeyId},
      {"private_key", kPrivateKey},  {"name", kServiceIdentityName},
      {"ca_cert_path", kCaCertPath}, {"token_uri", kTokenUri},
  };
}

std::string MakeTestContents() { return TestContents().dump(); }

MATCHER_P(JsonPayloadIs, payload, "JSON payload is") {
  if ((arg.empty() && !payload.empty()) || (!arg.empty() && payload.empty())) {
    return false;
  }
  if (arg.empty() && payload.empty()) return true;

  auto json_arg =
      nlohmann::json::parse(std::string{arg[0].data(), arg[0].size()});
  if (json_arg.is_discarded() || !json_arg.is_object()) return false;

  // The value of the subject_token is based on a random key, so just check if
  // it is present.
  if (json_arg.erase("subject_token") != 1) return false;
  if (json_arg.size() != payload.size()) return false;

  // Compare the remaining items.
  auto const& items = payload.items();
  return std::all_of(items.begin(), items.end(), [&json_arg](auto const& p) {
    return p.value() == json_arg.value(p.key(), "");
  });
  return true;
}

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
      nlohmann::json{{"alg", "ES256"}, {"typ", "JWT"}, {"kid", kPrivateKeyId}};
  auto const iat = static_cast<std::intmax_t>(kFixedJwtTimestamp);
  auto const exp = iat + 3600;
  auto const iss_sub_value = absl::StrCat("system:serviceaccount:", kProjectId,
                                          ":", kServiceIdentityName);
  auto const expected_payload = nlohmann::json{
      {"iss", iss_sub_value}, {"sub", iss_sub_value}, {"aud", kTokenUri},
      {"iat", iat},           {"exp", exp},
  };

  auto const expected_json_payload = nlohmann::json{
      {"grant_type", "urn:ietf:params:oauth:token-type:token-exchange"},
      {"audience", kAudience},
      {"requested_token_type", "urn:ietf:params:oauth:token-type:access_token"},
      {"subject_token_type", "urn:k8s:params:oauth:token-type:serviceaccount"}};

  auto token_client = [=] {
    using FormDataType = std::vector<absl::Span<char const>>;
    auto mock = std::make_unique<MockRestClient>();
    auto expected_request = Property(&RestRequest::path, kTokenUri);
    auto expected_form_data =
        MatcherCast<FormDataType const&>(JsonPayloadIs(expected_json_payload));

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
      MakeTestContents(), kAudience, Options{},
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
      "project": "test-project-id",
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
      "project": "test-project-id",
      "private_key_id": "not-a-key-id-just-for-testing",
      "private_key": "not-a-valid-key-just-for-testing",
      "name": "test-service-identity",
      "ca_cert_path": "/test/ca.crt",
      "token_uri": "https://gdc.token.uri/v1/token"
})""";

  for (auto const& field :
       {"project", "private_key_id", "private_key", "name", "token_uri"}) {
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
      "project": "test-project-id",
      "private_key_id": "not-a-key-id-just-for-testing",
      "private_key": "not-a-valid-key-just-for-testing",
      "name": "test-service-identity",
      "ca_cert_path": "/test/ca.crt",
      "token_uri": "https://gdc.token.uri/v1/token"
})""";

  for (auto const& field : {"project", "private_key_id", "private_key", "name",
                            "ca_cert_path", "token_uri"}) {
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
      "project": "test-project-id",
      "private_key_id": "not-a-key-id-just-for-testing",
      "private_key": "not-a-valid-key-just-for-testing",
      "name": "test-service-identity",
      "token_uri": "https://gdc.token.uri/v1/token"
})""";

  for (auto const& field :
       {"project", "private_key_id", "private_key", "name", "token_uri"}) {
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
      MakeTestContents(), kAudience, Options{},
      mock_http_client_factory.AsStdFunction());
  ASSERT_STATUS_OK(credentials);
  EXPECT_THAT((*credentials)->project_id(), IsOkAndHolds(kProjectId));
  EXPECT_THAT((*credentials)->project_id({}), IsOkAndHolds(kProjectId));
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
  EXPECT_THAT(header.value("alg", ""), Eq("ES256"));
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
  ASSERT_STATUS_OK(assertion);

  std::vector<std::string> actual_tokens = absl::StrSplit(*assertion, '.');
  ASSERT_THAT(actual_tokens.size(), Eq(3));
  std::vector<std::vector<std::uint8_t>> decoded(actual_tokens.size());
  std::transform(
      actual_tokens.begin(), actual_tokens.end(), decoded.begin(),
      [](std::string const& e) { return UrlsafeBase64Decode(e).value(); });

  // Verify the header and payloads are valid.
  // We cannot verify the signature in this same fashion as ECDSA relies on a
  // random ephemeral key.
  auto const header =
      nlohmann::json::parse(decoded[0].begin(), decoded[0].end());
  auto const expected_header =
      nlohmann::json{{"alg", "ES256"}, {"typ", "JWT"}, {"kid", kPrivateKeyId}};
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
  ASSERT_STATUS_OK(assertion);

  auto actual_payload =
      GDCHServiceAccountCredentials::CreateRefreshPayload(*info, now);
  ASSERT_STATUS_OK(actual_payload);
  EXPECT_THAT(actual_payload->value("grant_type", ""),
              Eq("urn:ietf:params:oauth:token-type:token-exchange"));
  EXPECT_THAT(actual_payload->value("audience", ""), Eq(kAudience));
  EXPECT_THAT(actual_payload->value("requested_token_type", ""),
              Eq("urn:ietf:params:oauth:token-type:access_token"));
  EXPECT_THAT(actual_payload->value("subject_token", ""), Not(IsEmpty()));
  EXPECT_THAT(actual_payload->value("subject_token_type", ""),
              Eq("urn:k8s:params:oauth:token-type:serviceaccount"));
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
