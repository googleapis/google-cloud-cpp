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
#include "google/cloud/testing_util/chrono_output.h"
#include "google/cloud/testing_util/mock_http_payload.h"
#include "google/cloud/testing_util/mock_rest_client.h"
#include "google/cloud/testing_util/mock_rest_response.h"
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
using ::google::cloud::testing_util::StatusIs;
using ::testing::_;
using ::testing::An;
using ::testing::AtMost;
using ::testing::HasSubstr;
using ::testing::Pair;
using ::testing::Return;
using ::testing::UnorderedElementsAre;

using MockClientFactory =
    ::testing::MockFunction<std::unique_ptr<rest_internal::RestClient>(
        Options const&)>;

std::unique_ptr<RestResponse> MakeMockResponse(HttpStatusCode code,
                                               std::string contents) {
  auto response = absl::make_unique<MockRestResponse>();
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
  auto response = absl::make_unique<MockRestResponse>();
  EXPECT_CALL(*response, StatusCode)
      .WillRepeatedly(Return(HttpStatusCode::kOk));
  EXPECT_CALL(std::move(*response), ExtractPayload)
      .Times(AtMost(1))
      .WillRepeatedly([partial = std::move(partial)]() mutable {
        auto payload = absl::make_unique<MockHttpPayload>();
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
  auto const info = ExternalAccountInfo{
      "test-audience",
      "test-subject-token-type",
      test_url,
      mock_source,
  };
  MockClientFactory client_factory;
  EXPECT_CALL(client_factory, Call).WillOnce([&]() {
    auto mock = absl::make_unique<MockRestClient>();
    EXPECT_CALL(*mock, Post(_, An<FormDataType const&>()))
        .WillOnce(
            [test_url, info, response = json_response.dump()](
                RestRequest const& request, auto const& form_data) mutable {
              EXPECT_THAT(
                  form_data,
                  UnorderedElementsAre(
                      Pair("grant_type",
                           "urn:ietf:params:oauth:grant-type:token-exchange"),
                      Pair("requested_token_type",
                           "urn:ietf:params:oauth:token-type:access_token"),
                      Pair("scope",
                           "https://www.googleapis.com/auth/cloud-platform"),
                      Pair("audience", "test-audience"),
                      Pair("subject_token_type", "test-subject-token-type"),
                      Pair("subject_token", "test-subject-token")));
              EXPECT_EQ(request.path(), test_url);
              return MakeMockResponseSuccess(std::move(response));
            });
    return mock;
  });

  auto credentials =
      ExternalAccountCredentials(info, client_factory.AsStdFunction());
  auto const now = std::chrono::system_clock::now();
  auto access_token = credentials.GetToken(now);
  ASSERT_STATUS_OK(access_token);
  EXPECT_EQ(access_token->expiration, now + expected_expires_in);
  EXPECT_EQ(access_token->token, expected_access_token);
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
  auto const info = ExternalAccountInfo{
      "test-audience",
      "test-subject-token-type",
      test_url,
      mock_source,
  };
  MockClientFactory client_factory;
  EXPECT_CALL(client_factory, Call).WillOnce([&]() {
    auto mock = absl::make_unique<MockRestClient>();
    EXPECT_CALL(*mock, Post(_, An<FormDataType const&>()))
        .WillOnce([test_url, info](RestRequest const& request,
                                   auto const& form_data) {
          EXPECT_THAT(
              form_data,
              UnorderedElementsAre(
                  Pair("grant_type",
                       "urn:ietf:params:oauth:grant-type:token-exchange"),
                  Pair("requested_token_type",
                       "urn:ietf:params:oauth:token-type:access_token"),
                  Pair("scope",
                       "https://www.googleapis.com/auth/cloud-platform"),
                  Pair("audience", "test-audience"),
                  Pair("subject_token_type", "test-subject-token-type"),
                  Pair("subject_token", "test-subject-token")));
          EXPECT_EQ(request.path(), test_url);
          return MakeMockResponseError();
        });
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
  auto const info = ExternalAccountInfo{
      "test-audience",
      "test-subject-token-type",
      test_url,
      mock_source,
  };
  MockClientFactory client_factory;
  EXPECT_CALL(client_factory, Call).WillOnce([&]() {
    auto mock = absl::make_unique<MockRestClient>();
    EXPECT_CALL(*mock, Post(_, An<FormDataType const&>()))
        .WillOnce([test_url, info, response](RestRequest const& request,
                                             auto const& form_data) mutable {
          EXPECT_THAT(
              form_data,
              UnorderedElementsAre(
                  Pair("grant_type",
                       "urn:ietf:params:oauth:grant-type:token-exchange"),
                  Pair("requested_token_type",
                       "urn:ietf:params:oauth:token-type:access_token"),
                  Pair("scope",
                       "https://www.googleapis.com/auth/cloud-platform"),
                  Pair("audience", "test-audience"),
                  Pair("subject_token_type", "test-subject-token-type"),
                  Pair("subject_token", "test-subject-token")));
          EXPECT_EQ(request.path(), test_url);
          return MakeMockResponsePartialError(std::move(response));
        });
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
  auto const info = ExternalAccountInfo{
      "test-audience",
      "test-subject-token-type",
      test_url,
      mock_source,
  };
  MockClientFactory client_factory;
  EXPECT_CALL(client_factory, Call).WillOnce([&]() {
    auto mock = absl::make_unique<MockRestClient>();
    EXPECT_CALL(*mock, Post(_, An<FormDataType const&>()))
        .WillOnce([test_url, info, p = payload](RestRequest const& request,
                                                auto const& form_data) mutable {
          EXPECT_THAT(
              form_data,
              UnorderedElementsAre(
                  Pair("grant_type",
                       "urn:ietf:params:oauth:grant-type:token-exchange"),
                  Pair("requested_token_type",
                       "urn:ietf:params:oauth:token-type:access_token"),
                  Pair("scope",
                       "https://www.googleapis.com/auth/cloud-platform"),
                  Pair("audience", "test-audience"),
                  Pair("subject_token_type", "test-subject-token-type"),
                  Pair("subject_token", "test-subject-token")));
          EXPECT_EQ(request.path(), test_url);
          return MakeMockResponseSuccess(std::move(p));
        });
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
  auto const info = ExternalAccountInfo{
      "test-audience",
      "test-subject-token-type",
      test_url,
      mock_source,
  };
  MockClientFactory client_factory;
  EXPECT_CALL(client_factory, Call).WillOnce([&]() {
    auto mock = absl::make_unique<MockRestClient>();
    EXPECT_CALL(*mock, Post(_, An<FormDataType const&>()))
        .WillOnce([test_url, info, p = payload](RestRequest const& request,
                                                auto const& form_data) mutable {
          EXPECT_THAT(
              form_data,
              UnorderedElementsAre(
                  Pair("grant_type",
                       "urn:ietf:params:oauth:grant-type:token-exchange"),
                  Pair("requested_token_type",
                       "urn:ietf:params:oauth:token-type:access_token"),
                  Pair("scope",
                       "https://www.googleapis.com/auth/cloud-platform"),
                  Pair("audience", "test-audience"),
                  Pair("subject_token_type", "test-subject-token-type"),
                  Pair("subject_token", "test-subject-token")));
          EXPECT_EQ(request.path(), test_url);
          return MakeMockResponseSuccess(std::move(p));
        });
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
  auto const info = ExternalAccountInfo{
      "test-audience",
      "test-subject-token-type",
      test_url,
      mock_source,
  };
  MockClientFactory client_factory;
  EXPECT_CALL(client_factory, Call).WillOnce([&]() {
    auto mock = absl::make_unique<MockRestClient>();
    EXPECT_CALL(*mock, Post(_, An<FormDataType const&>()))
        .WillOnce([test_url, info, response = json_response.dump()](
                      auto const&, auto const&) {
          return MakeMockResponseSuccess(response);
        });
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
  auto const info = ExternalAccountInfo{
      "test-audience",
      "test-subject-token-type",
      test_url,
      mock_source,
  };
  MockClientFactory client_factory;
  EXPECT_CALL(client_factory, Call).WillOnce([&]() {
    auto mock = absl::make_unique<MockRestClient>();
    EXPECT_CALL(*mock, Post(_, An<FormDataType const&>()))
        .WillOnce([test_url, info, response = json_response.dump()](
                      auto const&, auto const&) {
          return MakeMockResponseSuccess(response);
        });
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
  auto const info = ExternalAccountInfo{
      "test-audience",
      "test-subject-token-type",
      test_url,
      mock_source,
  };
  MockClientFactory client_factory;
  EXPECT_CALL(client_factory, Call).WillOnce([&]() {
    auto mock = absl::make_unique<MockRestClient>();
    EXPECT_CALL(*mock, Post(_, An<FormDataType const&>()))
        .WillOnce([test_url, info, response = json_response.dump()](
                      auto const&, auto const&) {
          return MakeMockResponseSuccess(response);
        });
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
  auto const info = ExternalAccountInfo{
      "test-audience",
      "test-subject-token-type",
      test_url,
      mock_source,
  };
  MockClientFactory client_factory;
  EXPECT_CALL(client_factory, Call).WillOnce([&]() {
    auto mock = absl::make_unique<MockRestClient>();
    EXPECT_CALL(*mock, Post(_, An<FormDataType const&>()))
        .WillOnce([test_url, info, response = json_response.dump()](
                      auto const&, auto const&) {
          return MakeMockResponseSuccess(response);
        });
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
  auto const info = ExternalAccountInfo{
      "test-audience",
      "test-subject-token-type",
      test_url,
      mock_source,
  };
  MockClientFactory client_factory;
  EXPECT_CALL(client_factory, Call).WillOnce([&]() {
    auto mock = absl::make_unique<MockRestClient>();
    EXPECT_CALL(*mock, Post(_, An<FormDataType const&>()))
        .WillOnce([test_url, info, response = json_response.dump()](
                      auto const&, auto const&) {
          return MakeMockResponseSuccess(response);
        });
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
  auto const info = ExternalAccountInfo{
      "test-audience",
      "test-subject-token-type",
      test_url,
      mock_source,
  };
  MockClientFactory client_factory;
  EXPECT_CALL(client_factory, Call).WillOnce([&]() {
    auto mock = absl::make_unique<MockRestClient>();
    EXPECT_CALL(*mock, Post(_, An<FormDataType const&>()))
        .WillOnce([test_url, info, response = json_response.dump()](
                      auto const&, auto const&) {
          return MakeMockResponseSuccess(response);
        });
    return mock;
  });

  auto credentials =
      ExternalAccountCredentials(info, client_factory.AsStdFunction());
  auto const now = std::chrono::system_clock::now();
  auto access_token = credentials.GetToken(now);
  EXPECT_THAT(access_token,
              StatusIs(StatusCode::kInvalidArgument,
                       HasSubstr("expected a numeric `expires_in`")));
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
  auto const info = ExternalAccountInfo{
      "test-audience",
      "test-subject-token-type",
      test_url,
      mock_source,
  };
  MockClientFactory client_factory;
  EXPECT_CALL(client_factory, Call).WillOnce([&]() {
    auto mock = absl::make_unique<MockRestClient>();
    EXPECT_CALL(*mock, Post(_, An<FormDataType const&>()))
        .WillOnce([test_url, info, response = json_response.dump()](
                      auto const&, auto const&) {
          return MakeMockResponseSuccess(response);
        });
    return mock;
  });

  auto credentials =
      ExternalAccountCredentials(info, client_factory.AsStdFunction());
  auto const now = std::chrono::system_clock::now();
  auto access_token = credentials.GetToken(now);
  EXPECT_THAT(access_token,
              StatusIs(StatusCode::kInvalidArgument,
                       HasSubstr("expected a numeric `expires_in`")));
  EXPECT_THAT(access_token.status().error_info().domain(), "gcloud-cpp");
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace oauth2_internal
}  // namespace cloud
}  // namespace google
