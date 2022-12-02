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

#include "google/cloud/internal/external_account_token_source_url.h"
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

using ::google::cloud::rest_internal::HttpStatusCode;
using ::google::cloud::rest_internal::RestRequest;
using ::google::cloud::rest_internal::RestResponse;
using ::google::cloud::testing_util::MockHttpPayload;
using ::google::cloud::testing_util::MockRestClient;
using ::google::cloud::testing_util::MockRestResponse;
using ::google::cloud::testing_util::StatusIs;
using ::testing::AtMost;
using ::testing::ElementsAre;
using ::testing::HasSubstr;
using ::testing::IsSupersetOf;
using ::testing::Pair;
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
      .Times(AtMost(1))
      .WillRepeatedly([contents = std::move(contents)]() mutable {
        auto payload = absl::make_unique<MockHttpPayload>();
        // This is shared by the next two mocking functions.
        auto c = std::make_shared<std::string>(contents);
        EXPECT_CALL(*payload, HasUnreadData).WillRepeatedly([c] {
          return !c->empty();
        });
        EXPECT_CALL(*payload, Read)
            .WillRepeatedly([c](absl::Span<char> buffer) {
              auto const n = (std::min)(buffer.size(), c->size());
              std::copy(c->begin(), std::next(c->begin(), n), buffer.begin());
              c->assign(c->substr(n));
              return n;
            });
        return std::unique_ptr<rest_internal::HttpPayload>(std::move(payload));
      });
  return response;
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
  auto response = absl::make_unique<MockRestResponse>();
  EXPECT_CALL(*response, StatusCode)
      .WillRepeatedly(Return(HttpStatusCode::kNotFound));
  EXPECT_CALL(std::move(*response), ExtractPayload)
      .Times(AtMost(1))
      .WillRepeatedly([] {
        auto payload = absl::make_unique<MockHttpPayload>();
        // This is shared by the next two mocking functions.
        auto c = std::make_shared<std::string>(kErrorPayload);
        EXPECT_CALL(*payload, HasUnreadData).WillRepeatedly([c] {
          return !c->empty();
        });
        EXPECT_CALL(*payload, Read)
            .WillRepeatedly([c](absl::Span<char> buffer) {
              auto const n = (std::min)(buffer.size(), c->size());
              std::copy(c->begin(), std::next(c->begin(), n), buffer.begin());
              c->assign(c->substr(n));
              return n;
            });
        return std::unique_ptr<rest_internal::HttpPayload>(std::move(payload));
      });
  return response;
}

TEST(ExternalAccountTokenSource, WorkingPlainResponse) {
  auto const test_url = std::string{"https://test-only.example.com/foo/bar"};
  auto const token = std::string{"a-test-only-token"};
  MockClientFactory client_factory;
  EXPECT_CALL(client_factory, Call).WillOnce([&]() {
    auto mock = absl::make_unique<MockRestClient>();
    EXPECT_CALL(*mock, Get)
        .WillOnce([test_url, token](RestRequest const& request) {
          EXPECT_EQ(request.path(), test_url);
          return MakeMockResponseSuccess(token);
        });
    return mock;
  });

  auto const creds = nlohmann::json{{"url", test_url}};
  auto const source =
      MakeExternalAccountTokenSourceUrl(creds, MakeTestErrorContext());
  ASSERT_STATUS_OK(source);
  auto const actual = (*source)(client_factory.AsStdFunction(), Options{});
  ASSERT_STATUS_OK(actual);
  EXPECT_EQ(*actual, internal::SubjectToken{token});
}

TEST(ExternalAccountTokenSource, WorkingPlainResponseWithHeaders) {
  auto const test_url = std::string{"https://test-only.example.com/foo/bar"};
  auto const token = std::string{"a-test-only-token"};
  MockClientFactory client_factory;
  EXPECT_CALL(client_factory, Call).WillOnce([&]() {
    auto mock = absl::make_unique<MockRestClient>();
    EXPECT_CALL(*mock, Get)
        .WillOnce([test_url, token](RestRequest const& request) {
          EXPECT_EQ(request.path(), test_url);
          EXPECT_THAT(
              request.headers(),
              IsSupersetOf(
                  {Pair("authorization", ElementsAre("Bearer test-only")),
                   Pair("test-header", ElementsAre("test-value"))}));
          return MakeMockResponseSuccess(token);
        });
    return mock;
  });

  auto const creds = nlohmann::json{{"url", test_url},
                                    {"headers",
                                     {
                                         {"Authorization", "Bearer test-only"},
                                         {"Test-Header", "test-value"},
                                     }}};
  auto const source =
      MakeExternalAccountTokenSourceUrl(creds, MakeTestErrorContext());
  ASSERT_STATUS_OK(source);
  auto const actual = (*source)(client_factory.AsStdFunction(), Options{});
  ASSERT_STATUS_OK(actual);
  EXPECT_EQ(*actual, internal::SubjectToken{token});
}

TEST(ExternalAccountTokenSource, WorkingJsonResponse) {
  auto const test_url = std::string{"https://test-only.example.com/foo/bar"};
  auto const token = std::string{"a-test-only-token"};
  auto const contents =
      nlohmann::json{{"unusedField", "unused"}, {"subjectToken", token}}.dump();
  MockClientFactory client_factory;
  EXPECT_CALL(client_factory, Call).WillOnce([&]() {
    auto mock = absl::make_unique<MockRestClient>();
    EXPECT_CALL(*mock, Get)
        .WillOnce([test_url, contents](RestRequest const& request) {
          EXPECT_EQ(request.path(), test_url);
          return MakeMockResponseSuccess(contents);
        });
    return mock;
  });

  auto const creds = nlohmann::json{
      {"url", test_url},
      {"format",
       {{"type", "json"}, {"subject_token_field_name", "subjectToken"}}}};
  auto const source =
      MakeExternalAccountTokenSourceUrl(creds, MakeTestErrorContext());
  ASSERT_STATUS_OK(source);
  auto const actual = (*source)(client_factory.AsStdFunction(), Options{});
  ASSERT_STATUS_OK(actual);
  EXPECT_EQ(*actual, internal::SubjectToken{token});
}

TEST(ExternalAccountTokenSource, MissingUrlField) {
  auto const creds = nlohmann::json{
      {"url-but-wrong", "https://169.254.169.254/subject/token"},
      {"format", {{"type", "text"}}},
  };
  auto const source =
      MakeExternalAccountTokenSourceUrl(creds, MakeTestErrorContext());
  EXPECT_THAT(source, StatusIs(StatusCode::kInvalidArgument,
                               HasSubstr("cannot find `url` field")));
  EXPECT_THAT(source.status().error_info().metadata(),
              IsSupersetOf({Pair("filename", "my-credentials.json"),
                            Pair("key", "value")}));
}

TEST(ExternalAccountTokenSource, InvalidUrlField) {
  auto const creds = nlohmann::json{
      {"url", true},
      {"format", {{"type", "text"}}},
  };
  auto const source =
      MakeExternalAccountTokenSourceUrl(creds, MakeTestErrorContext());
  EXPECT_THAT(source, StatusIs(StatusCode::kInvalidArgument,
                               HasSubstr("invalid type for `url` field")));
  EXPECT_THAT(source.status().error_info().metadata(),
              IsSupersetOf({Pair("filename", "my-credentials.json"),
                            Pair("key", "value")}));
}

TEST(ExternalAccountTokenSource, UnknownFormatType) {
  auto const creds = nlohmann::json{
      {"url", "https://169.254.169.254/subject/token"},
      {"format", {{"type", "neither-json-nor-text"}}},
  };
  auto const source =
      MakeExternalAccountTokenSourceUrl(creds, MakeTestErrorContext());
  EXPECT_THAT(source,
              StatusIs(StatusCode::kInvalidArgument,
                       HasSubstr("invalid file type <neither-json-nor-text>")));
  EXPECT_THAT(source.status().error_info().metadata(),
              IsSupersetOf({Pair("credentials_source.type", "url"),
                            Pair("credentials_source.url.url",
                                 "https://169.254.169.254/subject/token"),
                            Pair("filename", "my-credentials.json"),
                            Pair("key", "value")}));
}

TEST(ExternalAccountTokenSource, InvalidHeaderType) {
  auto const creds = nlohmann::json{
      {"url", "https://169.254.169.254/subject/token"},
      {"headers",
       {
           {"Authorization", "Bearer test-only"},
           {"invalid-header", true},
       }},
      {"format", {{"type", "text"}}},
  };
  auto const source =
      MakeExternalAccountTokenSourceUrl(creds, MakeTestErrorContext());
  EXPECT_THAT(source, StatusIs(StatusCode::kInvalidArgument,
                               HasSubstr("invalid type for `invalid-header`")));
  EXPECT_THAT(source.status().error_info().metadata(),
              IsSupersetOf({Pair("credentials_source.type", "url"),
                            Pair("credentials_source.url.url",
                                 "https://169.254.169.254/subject/token"),
                            Pair("filename", "my-credentials.json"),
                            Pair("key", "value")}));
}

TEST(ExternalAccountTokenSource, ErrorInPlainResponse) {
  auto const test_url = std::string{"https://169.254.169.254/subject/token"};
  MockClientFactory client_factory;
  EXPECT_CALL(client_factory, Call).WillOnce([&]() {
    auto mock = absl::make_unique<MockRestClient>();
    EXPECT_CALL(*mock, Get).WillOnce([test_url](RestRequest const& request) {
      EXPECT_EQ(request.path(), test_url);
      return MakeMockResponseError();
    });
    return mock;
  });

  auto const creds = nlohmann::json{{"url", test_url}};
  auto const source =
      MakeExternalAccountTokenSourceUrl(creds, MakeTestErrorContext());
  ASSERT_STATUS_OK(source);
  auto const actual = (*source)(client_factory.AsStdFunction(), Options{});
  EXPECT_THAT(actual, StatusIs(StatusCode::kNotFound));
  EXPECT_EQ(actual.status().error_info().reason(), "HTTP REQUEST");
  EXPECT_THAT(
      actual.status().error_info().metadata(),
      IsSupersetOf(
          {Pair("credentials_source.type", "url"),
           Pair("credentials_source.url.url",
                "https://169.254.169.254/subject/token"),
           Pair("credentials_source.url.type", "text"), Pair("context", "GKE"),
           Pair("service", "metadata.google.internal"),
           Pair("http_status_code", "404"),
           Pair("filename", "my-credentials.json"), Pair("key", "value")}));
}

TEST(ExternalAccountTokenSource, ErrorInJsonResponse) {
  auto const test_url = std::string{"https://169.254.169.254/subject/token"};
  MockClientFactory client_factory;
  EXPECT_CALL(client_factory, Call).WillOnce([&]() {
    auto mock = absl::make_unique<MockRestClient>();
    EXPECT_CALL(*mock, Get).WillOnce([test_url](RestRequest const& request) {
      EXPECT_EQ(request.path(), test_url);
      return MakeMockResponseError();
    });
    return mock;
  });

  auto const creds = nlohmann::json{
      {"url", test_url},
      {"format",
       {{"type", "json"}, {"subject_token_field_name", "fieldName"}}}};
  auto const source =
      MakeExternalAccountTokenSourceUrl(creds, MakeTestErrorContext());
  ASSERT_STATUS_OK(source);
  auto const actual = (*source)(client_factory.AsStdFunction(), Options{});
  EXPECT_THAT(actual, StatusIs(StatusCode::kNotFound));
  EXPECT_EQ(actual.status().error_info().reason(), "HTTP REQUEST");
  EXPECT_THAT(
      actual.status().error_info().metadata(),
      IsSupersetOf(
          {Pair("credentials_source.type", "url"),
           Pair("credentials_source.url.url",
                "https://169.254.169.254/subject/token"),
           Pair("credentials_source.url.type", "json"),
           Pair("credentials_source.url.subject_token_field_name", "fieldName"),
           Pair("context", "GKE"), Pair("service", "metadata.google.internal"),
           Pair("http_status_code", "404"),
           Pair("filename", "my-credentials.json"), Pair("key", "value")}));
}

TEST(ExternalAccountTokenSource, JsonResponseIsNotJson) {
  auto const test_url = std::string{"https://test-only.example.com/"};
  auto const token = std::string{"a-test-only-token"};
  auto const contents = std::string{"not-a-json-object"};
  MockClientFactory client_factory;
  EXPECT_CALL(client_factory, Call).WillOnce([&]() {
    auto mock = absl::make_unique<MockRestClient>();
    EXPECT_CALL(*mock, Get)
        .WillOnce([test_url, contents](RestRequest const& request) {
          EXPECT_EQ(request.path(), test_url);
          return MakeMockResponseSuccess(contents);
        });
    return mock;
  });

  auto const creds = nlohmann::json{
      {"url", test_url},
      {"format",
       {{"type", "json"}, {"subject_token_field_name", "fieldName"}}}};
  auto const source =
      MakeExternalAccountTokenSourceUrl(creds, MakeTestErrorContext());
  ASSERT_STATUS_OK(source);
  auto const actual = (*source)(client_factory.AsStdFunction(), Options{});
  EXPECT_THAT(actual, StatusIs(StatusCode::kInvalidArgument,
                               HasSubstr("in JSON object retrieved from "
                                         "`https://test-only.example.com/`")));
  EXPECT_THAT(
      actual.status().error_info().metadata(),
      IsSupersetOf(
          {Pair("credentials_source.type", "url"),
           Pair("credentials_source.url.url", "https://test-only.example.com/"),
           Pair("credentials_source.url.type", "json"),
           Pair("credentials_source.url.subject_token_field_name", "fieldName"),
           Pair("filename", "my-credentials.json"), Pair("key", "value")}));
}

TEST(ExternalAccountTokenSource, JsonResponseIsNotJsonObject) {
  auto const test_url = std::string{"https://test-only.example.com/"};
  auto const token = std::string{"a-test-only-token"};
  auto const contents =
      nlohmann::json{{"array0", "array1", "array2", "array3"}}.dump();
  MockClientFactory client_factory;
  EXPECT_CALL(client_factory, Call).WillOnce([&]() {
    auto mock = absl::make_unique<MockRestClient>();
    EXPECT_CALL(*mock, Get)
        .WillOnce([test_url, contents](RestRequest const& request) {
          EXPECT_EQ(request.path(), test_url);
          return MakeMockResponseSuccess(contents);
        });
    return mock;
  });

  auto const creds = nlohmann::json{
      {"url", test_url},
      {"format",
       {{"type", "json"}, {"subject_token_field_name", "fieldName"}}}};
  auto const source =
      MakeExternalAccountTokenSourceUrl(creds, MakeTestErrorContext());
  ASSERT_STATUS_OK(source);
  auto const actual = (*source)(client_factory.AsStdFunction(), Options{});
  EXPECT_THAT(actual, StatusIs(StatusCode::kInvalidArgument,
                               HasSubstr("in JSON object retrieved from "
                                         "`https://test-only.example.com/`")));
  EXPECT_THAT(
      actual.status().error_info().metadata(),
      IsSupersetOf(
          {Pair("credentials_source.type", "url"),
           Pair("credentials_source.url.url", "https://test-only.example.com/"),
           Pair("credentials_source.url.type", "json"),
           Pair("credentials_source.url.subject_token_field_name", "fieldName"),
           Pair("filename", "my-credentials.json"), Pair("key", "value")}));
}

TEST(ExternalAccountTokenSource, JsonResponseMissingField) {
  auto const test_url = std::string{"https://test-only.example.com/"};
  auto const token = std::string{"a-test-only-token"};
  auto const contents =
      nlohmann::json{{"wrongName", token}, {"unusedField", "unused"}}.dump();
  MockClientFactory client_factory;
  EXPECT_CALL(client_factory, Call).WillOnce([&]() {
    auto mock = absl::make_unique<MockRestClient>();
    EXPECT_CALL(*mock, Get)
        .WillOnce([test_url, contents](RestRequest const& request) {
          EXPECT_EQ(request.path(), test_url);
          return MakeMockResponseSuccess(contents);
        });
    return mock;
  });

  auto const creds = nlohmann::json{
      {"url", test_url},
      {"format",
       {{"type", "json"}, {"subject_token_field_name", "fieldName"}}}};
  auto const source =
      MakeExternalAccountTokenSourceUrl(creds, MakeTestErrorContext());
  ASSERT_STATUS_OK(source);
  auto const actual = (*source)(client_factory.AsStdFunction(), Options{});
  EXPECT_THAT(actual, StatusIs(StatusCode::kInvalidArgument,
                               HasSubstr("in JSON object retrieved from "
                                         "`https://test-only.example.com/`")));
  EXPECT_THAT(
      actual.status().error_info().metadata(),
      IsSupersetOf(
          {Pair("credentials_source.type", "url"),
           Pair("credentials_source.url.url", "https://test-only.example.com/"),
           Pair("credentials_source.url.type", "json"),
           Pair("credentials_source.url.subject_token_field_name", "fieldName"),
           Pair("filename", "my-credentials.json"), Pair("key", "value")}));
}

TEST(ExternalAccountTokenSource, JsonResponseInvalidField) {
  auto const test_url = std::string{"https://test-only.example.com/"};
  auto const token = std::string{"a-test-only-token"};
  auto const contents =
      nlohmann::json{{"unusedField", "unused"}, {"fieldName", false}}.dump();
  MockClientFactory client_factory;
  EXPECT_CALL(client_factory, Call).WillOnce([&]() {
    auto mock = absl::make_unique<MockRestClient>();
    EXPECT_CALL(*mock, Get)
        .WillOnce([test_url, contents](RestRequest const& request) {
          EXPECT_EQ(request.path(), test_url);
          return MakeMockResponseSuccess(contents);
        });
    return mock;
  });

  auto const creds = nlohmann::json{
      {"url", test_url},
      {"format",
       {{"type", "json"}, {"subject_token_field_name", "fieldName"}}}};
  auto const source =
      MakeExternalAccountTokenSourceUrl(creds, MakeTestErrorContext());
  ASSERT_STATUS_OK(source);
  auto const actual = (*source)(client_factory.AsStdFunction(), Options{});
  EXPECT_THAT(actual, StatusIs(StatusCode::kInvalidArgument,
                               HasSubstr("in JSON object retrieved from "
                                         "`https://test-only.example.com/`")));
  EXPECT_THAT(
      actual.status().error_info().metadata(),
      IsSupersetOf(
          {Pair("credentials_source.type", "url"),
           Pair("credentials_source.url.url", "https://test-only.example.com/"),
           Pair("credentials_source.url.type", "json"),
           Pair("credentials_source.url.subject_token_field_name", "fieldName"),
           Pair("filename", "my-credentials.json"), Pair("key", "value")}));
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace oauth2_internal
}  // namespace cloud
}  // namespace google
