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

#include "google/cloud/internal/oauth2_compute_engine_credentials.h"
#include "google/cloud/internal/absl_str_cat_quiet.h"
#include "google/cloud/internal/backoff_policy.h"
#include "google/cloud/internal/compute_engine_util.h"
#include "google/cloud/internal/make_status.h"
#include "google/cloud/internal/retry_policy_impl.h"
#include "google/cloud/testing_util/mock_http_payload.h"
#include "google/cloud/testing_util/mock_rest_client.h"
#include "google/cloud/testing_util/mock_rest_response.h"
#include "google/cloud/testing_util/status_matchers.h"
#include "google/cloud/universe_domain_options.h"
#include <gmock/gmock.h>
#include <chrono>

namespace google {
namespace cloud {
namespace oauth2_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::google::cloud::rest_internal::HttpStatusCode;
using ::google::cloud::rest_internal::RestContext;
using ::google::cloud::rest_internal::RestRequest;
using ::google::cloud::rest_internal::RestResponse;
using ::google::cloud::testing_util::IsOk;
using ::google::cloud::testing_util::IsOkAndHolds;
using ::google::cloud::testing_util::MakeMockHttpPayloadSuccess;
using ::google::cloud::testing_util::MockRestClient;
using ::google::cloud::testing_util::MockRestResponse;
using ::google::cloud::testing_util::StatusIs;
using ::testing::_;
using ::testing::AllOf;
using ::testing::ByMove;
using ::testing::Contains;
using ::testing::HasSubstr;
using ::testing::Not;
using ::testing::Pair;
using ::testing::Property;
using ::testing::Return;
using ::testing::UnorderedElementsAre;
using ::testing::UnorderedElementsAreArray;

using MockHttpClientFactory =
    ::testing::MockFunction<std::unique_ptr<rest_internal::RestClient>(
        Options const&)>;

auto expect_service_config = [](std::string const& account) {
  auto const expected_path = absl::StrCat(
      internal::GceMetadataScheme(), "://", internal::GceMetadataHostname(),
      "/computeMetadata/v1/instance/service-accounts/", account, "/");
  return AllOf(
      Property(&RestRequest::path, expected_path),
      Property(&RestRequest::headers,
               Contains(Pair("metadata-flavor", Contains("Google")))),
      Property(&RestRequest::parameters, Contains(Pair("recursive", "true"))));
};

auto expect_token = [](std::string const& account) {
  auto const expected_path = absl::StrCat(
      internal::GceMetadataScheme(), "://", internal::GceMetadataHostname(),
      "/computeMetadata/v1/instance/service-accounts/", account, "/token");
  return AllOf(Property(&RestRequest::path, expected_path),
               Property(&RestRequest::headers,
                        Contains(Pair("metadata-flavor", Contains("Google")))),
               Property(&RestRequest::parameters,
                        Not(Contains(Pair("recursive", "true")))));
};

/// @test Verify that we can create and refresh ComputeEngineCredentials.
TEST(ComputeEngineCredentialsTest,
     RefreshingSendsCorrectRequestBodyAndParsesResponse) {
  auto const alias = std::string{"default"};
  auto const email = std::string{"foo@bar.baz"};
  auto const svc_acct_info_resp = std::string{R"""({
      "email": ")""" + email + R"""(",
      "scopes": ["scope1","scope2"]
  })"""};
  auto const token_info_resp = std::string{R"""({
      "access_token": "mysupersecrettoken",
      "expires_in": 3600,
      "token_type": "tokentype"
  })"""};

  auto mock_metadata_client = [&]() {
    auto response = std::make_unique<MockRestResponse>();
    EXPECT_CALL(*response, StatusCode)
        .WillRepeatedly(Return(HttpStatusCode::kOk));
    EXPECT_CALL(std::move(*response), ExtractPayload)
        .WillOnce(
            Return(ByMove(MakeMockHttpPayloadSuccess(svc_acct_info_resp))));

    auto mock = std::make_unique<MockRestClient>();
    EXPECT_CALL(*mock, Get(_, expect_service_config(alias)))
        .WillOnce(
            Return(ByMove(std::unique_ptr<RestResponse>(std::move(response)))));
    return mock;
  }();

  auto mock_token_client = [&]() {
    auto response = std::make_unique<MockRestResponse>();
    EXPECT_CALL(*response, StatusCode)
        .WillRepeatedly(Return(HttpStatusCode::kOk));
    EXPECT_CALL(std::move(*response), ExtractPayload)
        .WillOnce(Return(ByMove(MakeMockHttpPayloadSuccess(token_info_resp))));

    auto mock = std::make_unique<MockRestClient>();
    EXPECT_CALL(*mock, Get(_, expect_token(email)))
        .WillOnce(
            Return(ByMove(std::unique_ptr<RestResponse>(std::move(response)))));
    return mock;
  }();

  MockHttpClientFactory mock_http_client_factory;
  EXPECT_CALL(mock_http_client_factory, Call)
      .WillOnce(Return(ByMove(std::move(mock_metadata_client))))
      .WillOnce(Return(ByMove(std::move(mock_token_client))));
  ComputeEngineCredentials credentials(
      alias, Options{}, mock_http_client_factory.AsStdFunction());
  // Calls Refresh to obtain the access token for our authorization header.
  auto const now = std::chrono::system_clock::now();
  auto const expected_token =
      AccessToken{"mysupersecrettoken", now + std::chrono::seconds(3600)};
  EXPECT_EQ(expected_token, credentials.GetToken(now).value());
  // Make sure we obtain the scopes and email from the metadata server.
  EXPECT_EQ(email, credentials.service_account_email());
  EXPECT_THAT(credentials.scopes(), UnorderedElementsAre("scope1", "scope2"));
}

/// @test Parsing a refresh response with missing fields results in failure.
TEST(ComputeEngineCredentialsTest,
     ParseComputeEngineRefreshResponseMissingFields) {
  std::string token_info_resp = R"""({})""";
  // Does not have access_token.
  std::string token_info_resp2 = R"""({
      "expires_in": 3600,
      "token_type": "tokentype"
)""";

  auto mock_response1 = std::make_unique<MockRestResponse>();
  EXPECT_CALL(*mock_response1, StatusCode)
      .WillRepeatedly(Return(HttpStatusCode::kBadRequest));
  EXPECT_CALL(std::move(*mock_response1), ExtractPayload)
      .WillOnce(Return(ByMove(MakeMockHttpPayloadSuccess(token_info_resp))));

  auto mock_response2 = std::make_unique<MockRestResponse>();
  EXPECT_CALL(*mock_response2, StatusCode)
      .WillRepeatedly(Return(HttpStatusCode::kBadRequest));
  EXPECT_CALL(std::move(*mock_response2), ExtractPayload)
      .WillOnce(Return(ByMove(MakeMockHttpPayloadSuccess(token_info_resp2))));

  auto status = ParseComputeEngineRefreshResponse(
      *mock_response1, std::chrono::system_clock::from_time_t(1000));
  EXPECT_THAT(status,
              StatusIs(StatusCode::kInvalidArgument,
                       HasSubstr("Could not find all required fields")));

  status = ParseComputeEngineRefreshResponse(
      *mock_response2, std::chrono::system_clock::from_time_t(1000));
  EXPECT_THAT(status,
              StatusIs(StatusCode::kInvalidArgument,
                       HasSubstr("Could not find all required fields")));
}

/// @test Parsing a refresh response yields an access token.
TEST(ComputeEngineCredentialsTest, ParseComputeEngineRefreshResponse) {
  auto const token_info_resp = std::string{R"""({
      "access_token": "mysupersecrettoken",
      "expires_in": 3600,
      "token_type": "tokentype"})"""};

  auto mock_http_payload = MakeMockHttpPayloadSuccess(token_info_resp);

  auto mock_response = std::make_unique<MockRestResponse>();
  EXPECT_CALL(*mock_response, StatusCode)
      .WillRepeatedly(Return(HttpStatusCode::kBadRequest));
  EXPECT_CALL(std::move(*mock_response), ExtractPayload)
      .WillOnce(Return(ByMove(std::move(mock_http_payload))));

  auto const now = std::chrono::system_clock::now();
  auto const expires_in = std::chrono::seconds(3600);

  auto status = ParseComputeEngineRefreshResponse(*mock_response, now);
  EXPECT_STATUS_OK(status);
  auto token = *status;
  EXPECT_EQ(token.expiration, now + expires_in);
  EXPECT_EQ(token.token, "mysupersecrettoken");
}

/// @test Parsing a metadata server response yields a ServiceAccountMetadata.
TEST(ComputeEngineCredentialsTest, ParseMetadataServerResponse) {
  struct TestCase {
    std::string payload;
    ServiceAccountMetadata expected;
  } cases[] = {
      {R"js({"email": "foo@bar.baz", "scopes": ["scope1", "scope2"], "universe_domain": "test-ud.net"})js",
       ServiceAccountMetadata{
           {"scope1", "scope2"}, "foo@bar.baz", "test-ud.net"}},
      {R"js({"email": "foo@bar.baz", "scopes": "scope1\nscope2\n"})js",
       ServiceAccountMetadata{
           {"scope1", "scope2"}, "foo@bar.baz", "googleapis.com"}},
      // Ignore invalid formats
      {R"js({"email": ["1", "2"], "scopes": ["scope1", "scope2"], "universe_domain": true})js",
       ServiceAccountMetadata{{"scope1", "scope2"}, "", {}}},
      {R"js({"email": "foo@bar", "scopes": {"foo": "bar"}, "universe_domain": 42})js",
       ServiceAccountMetadata{{}, "foo@bar", {}}},
      // Ignore missing fields
      {R"js({"scopes": ["scope1", "scope2"]})js",
       ServiceAccountMetadata{{"scope1", "scope2"}, "", "googleapis.com"}},
      {R"js({"email": "foo@bar.baz"})js",
       ServiceAccountMetadata{{}, "foo@bar.baz", "googleapis.com"}},
      {R"js({})js", ServiceAccountMetadata{{}, "", "googleapis.com"}},
  };

  for (auto const& test : cases) {
    SCOPED_TRACE("testing with " + test.payload);

    auto mock_response = std::make_unique<MockRestResponse>();
    EXPECT_CALL(*mock_response, StatusCode)
        .WillRepeatedly(Return(HttpStatusCode::kOk));
    EXPECT_CALL(std::move(*mock_response), ExtractPayload)
        .WillOnce(Return(ByMove(MakeMockHttpPayloadSuccess(test.payload))));

    auto status = ParseMetadataServerResponse(*mock_response);
    ASSERT_STATUS_OK(status);
    auto metadata = *status;
    EXPECT_EQ(metadata.email, test.expected.email);
    EXPECT_THAT(metadata.scopes,
                UnorderedElementsAreArray(test.expected.scopes));
    EXPECT_EQ(metadata.universe_domain, test.expected.universe_domain);
  }
}

/// @test Mock a failed refresh response during RetrieveServiceAccountInfo.
TEST(ComputeEngineCredentialsTest, FailedRetrieveServiceAccountInfo) {
  auto const alias = std::string{"default"};
  auto const email = std::string{"foo@bar.baz"};
  auto const token_info_resp = std::string{R"""({
      "access_token": "mysupersecrettoken",
      "expires_in": 3600,
      "token_type": "tokentype"})"""};

  auto mock_metadata_client_get_error = [&]() {
    auto mock = std::make_unique<MockRestClient>();
    EXPECT_CALL(*mock, Get(_, expect_service_config(alias))).WillOnce([]() {
      return Status{StatusCode::kAborted, "Fake Curl error"};
    });
    return mock;
  }();

  auto mock_metadata_client_response_error = [&]() {
    auto mock = std::make_unique<MockRestClient>();
    EXPECT_CALL(*mock, Get(_, expect_service_config(alias))).WillOnce([] {
      auto response = std::make_unique<MockRestResponse>();
      EXPECT_CALL(*response, StatusCode)
          .WillRepeatedly(Return(HttpStatusCode::kBadRequest));
      return std::unique_ptr<RestResponse>(std::move(response));
    });
    return mock;
  }();

  MockHttpClientFactory mock_http_client_factory;
  EXPECT_CALL(mock_http_client_factory, Call)
      .WillOnce(Return(ByMove(std::move(mock_metadata_client_get_error))))
      .WillOnce(Return(ByMove(std::move(mock_metadata_client_response_error))));

  ComputeEngineCredentials credentials(
      alias, Options{}, mock_http_client_factory.AsStdFunction());
  // Response 1
  auto actual = credentials.AccountEmail();
  EXPECT_EQ(actual, alias);
  // Response 2
  actual = credentials.AccountEmail();
  EXPECT_EQ(actual, alias);
}

/// @test Mock a failed refresh response.
TEST(ComputeEngineCredentialsTest, FailedRefresh) {
  auto const alias = std::string{"default"};
  auto const email = std::string{"foo@bar.baz"};
  auto const svc_acct_info_resp = std::string{R"""({
      "email": "foo@bar.baz",
      "scopes": ["scope1","scope2"]
  })"""};
  // Note this response is missing a field.
  auto const token_info_resp = std::string{R"""({
      "expires_in": 3600,
      "token_type": "tokentype"
  })"""};

  // Fail the first call to RetrieveServiceAccountInfo immediately.
  auto metadata_aborted = [&]() {
    auto client = std::make_unique<MockRestClient>();
    EXPECT_CALL(*client, Get(_, expect_service_config(alias)))
        .WillOnce([&](RestContext&, RestRequest const&) {
          return Status{StatusCode::kAborted, "Fake Curl error / info", {}};
        });
    return client;
  }();
  // Then fail the token request immediately.
  auto token_aborted = [&]() {
    auto client = std::make_unique<MockRestClient>();
    EXPECT_CALL(*client, Get(_, expect_token(alias)))
        .WillOnce([&](RestContext&, RestRequest const&) {
          return Status{StatusCode::kAborted, "Fake Curl error / token", {}};
        });
    return client;
  }();
  // Since the service config request failed, it will be attempted again. This
  // time have it succeed.
  auto metadata_success = [&]() {
    auto response = std::make_unique<MockRestResponse>();
    EXPECT_CALL(*response, StatusCode)
        .WillRepeatedly(Return(HttpStatusCode::kOk));
    EXPECT_CALL(std::move(*response), ExtractPayload).WillOnce([&]() {
      return MakeMockHttpPayloadSuccess(svc_acct_info_resp);
    });

    auto client = std::make_unique<MockRestClient>();
    EXPECT_CALL(*client, Get(_, expect_service_config(alias)))
        .WillOnce(
            Return(ByMove(std::unique_ptr<RestResponse>(std::move(response)))));
    return client;
  }();
  // Make the token request fail. Now with a bad HTTP error code.
  auto token_bad_http = [&]() {
    auto response = std::make_unique<MockRestResponse>();
    EXPECT_CALL(*response, StatusCode)
        .WillRepeatedly(Return(HttpStatusCode::kBadRequest));
    EXPECT_CALL(std::move(*response), ExtractPayload).WillOnce([&] {
      return MakeMockHttpPayloadSuccess(std::string{});
    });

    auto client = std::make_unique<MockRestClient>();
    EXPECT_CALL(*client, Get(_, expect_token(email)))
        .WillOnce(
            Return(ByMove(std::unique_ptr<RestResponse>(std::move(response)))));
    return client;
  }();
  // And fail again, now with an incomplete response.
  auto token_incomplete = [&]() {
    auto response = std::make_unique<MockRestResponse>();
    EXPECT_CALL(*response, StatusCode)
        .WillRepeatedly(Return(HttpStatusCode::kOk));
    EXPECT_CALL(std::move(*response), ExtractPayload).WillOnce([&] {
      return MakeMockHttpPayloadSuccess(token_info_resp);
    });

    auto client = std::make_unique<MockRestClient>();
    EXPECT_CALL(*client, Get(_, expect_token(email)))
        .WillOnce(
            Return(ByMove(std::unique_ptr<RestResponse>(std::move(response)))));
    return client;
  }();

  MockHttpClientFactory client_factory;
  EXPECT_CALL(client_factory, Call)
      .WillOnce(Return(ByMove(std::move(metadata_aborted))))
      .WillOnce(Return(ByMove(std::move(token_aborted))))
      .WillOnce(Return(ByMove(std::move(metadata_success))))
      .WillOnce(Return(ByMove(std::move(token_bad_http))))
      .WillOnce(Return(ByMove(std::move(token_incomplete))));
  ComputeEngineCredentials credentials(alias, Options{},
                                       client_factory.AsStdFunction());
  auto const now = std::chrono::system_clock::now();
  auto status = credentials.GetToken(now);
  EXPECT_THAT(status, StatusIs(StatusCode::kAborted,
                               HasSubstr("Fake Curl error / token")));
  status = credentials.GetToken(now);
  EXPECT_THAT(status, Not(IsOk()));
  status = credentials.GetToken(now);
  EXPECT_THAT(status,
              StatusIs(Not(StatusCode::kOk),
                       HasSubstr("Could not find all required fields")));
}

/// @test Verify that we can force a refresh of the service account email.
TEST(ComputeEngineCredentialsTest, AccountEmail) {
  auto const alias = std::string{"default"};
  auto const email = std::string{"foo@bar.baz"};
  auto const svc_acct_info_resp = std::string{R"""({
      "email": ")""" + email + R"""(",
      "scopes": ["scope1","scope2"]
  })"""};

  auto client = std::make_unique<MockRestClient>();
  EXPECT_CALL(*client, Get(_, expect_service_config(alias)))
      .WillOnce([&](RestContext&, RestRequest const&) {
        auto response = std::make_unique<MockRestResponse>();
        EXPECT_CALL(*response, StatusCode)
            .WillRepeatedly(Return(HttpStatusCode::kOk));
        EXPECT_CALL(std::move(*response), ExtractPayload).WillOnce([&] {
          return MakeMockHttpPayloadSuccess(svc_acct_info_resp);
        });
        return std::unique_ptr<RestResponse>(std::move(response));
      });

  MockHttpClientFactory client_factory;
  EXPECT_CALL(client_factory, Call).WillOnce(Return(ByMove(std::move(client))));
  ComputeEngineCredentials credentials(alias, Options{},
                                       client_factory.AsStdFunction());
  EXPECT_EQ(credentials.service_account_email(), alias);
  auto refreshed_email = credentials.AccountEmail();
  EXPECT_EQ(email, refreshed_email);
  EXPECT_EQ(credentials.service_account_email(), refreshed_email);
}

auto expected_universe_domain_request = []() {
  auto const expected_path = absl::StrCat(
      internal::GceMetadataScheme(), "://", internal::GceMetadataHostname(),
      "/computeMetadata/v1/universe/universe-domain");
  return AllOf(
      Property(&RestRequest::path, expected_path),
      Property(&RestRequest::headers,
               Contains(Pair("metadata-flavor", Contains("Google")))),
      Property(&RestRequest::parameters, Contains(Pair("recursive", "true"))));
};

TEST(ComputeEngineCredentialsTest, UniverseDomainSuccess) {
  auto const universe_domain_resp = std::string{R"""(my-ud.net)"""};

  auto client = std::make_unique<MockRestClient>();
  EXPECT_CALL(*client, Get(_, expected_universe_domain_request()))
      .WillOnce([&](RestContext&, RestRequest const&) {
        return internal::UnavailableError("Transient Error");
      })
      .WillOnce([&](RestContext&, RestRequest const&) {
        auto response = std::make_unique<MockRestResponse>();
        EXPECT_CALL(*response, StatusCode)
            .WillRepeatedly(Return(HttpStatusCode::kOk));
        EXPECT_CALL(std::move(*response), ExtractPayload).WillOnce([&] {
          return MakeMockHttpPayloadSuccess(universe_domain_resp);
        });
        return std::unique_ptr<RestResponse>(std::move(response));
      });

  MockHttpClientFactory client_factory;
  EXPECT_CALL(client_factory, Call).WillOnce(Return(ByMove(std::move(client))));
  ComputeEngineCredentials credentials(Options{},
                                       client_factory.AsStdFunction());
  EXPECT_THAT(credentials.universe_domain(), IsOkAndHolds("my-ud.net"));
}

TEST(ComputeEngineCredentialsTest, UniverseDomainPermanentFailure) {
  auto client = std::make_unique<MockRestClient>();
  EXPECT_CALL(*client, Get(_, expected_universe_domain_request()))
      .WillOnce([&](RestContext&, RestRequest const&) {
        return internal::UnavailableError("Transient Error");
      })
      .WillOnce([&](RestContext&, RestRequest const&) {
        return internal::NotFoundError("Permanent Error");
      });

  MockHttpClientFactory client_factory;
  EXPECT_CALL(client_factory, Call).WillOnce(Return(ByMove(std::move(client))));
  ComputeEngineCredentials credentials(Options{},
                                       client_factory.AsStdFunction());
  EXPECT_THAT(credentials.universe_domain(), StatusIs(StatusCode::kNotFound));
}

TEST(ComputeEngineCredentialsTest, UniverseDomainMDSResourceNotFound) {
  auto client = std::make_unique<MockRestClient>();
  EXPECT_CALL(*client, Get(_, expected_universe_domain_request()))
      .WillOnce([&](RestContext&, RestRequest const&) {
        return internal::UnavailableError("Transient Error");
      })
      .WillOnce([&](RestContext&, RestRequest const&) {
        auto response = std::make_unique<MockRestResponse>();
        EXPECT_CALL(*response, StatusCode)
            .WillRepeatedly(Return(HttpStatusCode::kNotFound));
        return std::unique_ptr<RestResponse>(std::move(response));
      });

  MockHttpClientFactory client_factory;
  EXPECT_CALL(client_factory, Call).WillOnce(Return(ByMove(std::move(client))));
  ComputeEngineCredentials credentials(Options{},
                                       client_factory.AsStdFunction());
  EXPECT_THAT(credentials.universe_domain(), IsOkAndHolds("googleapis.com"));
}

struct TestUniverseDomainRetryTraits {
  static bool IsPermanentFailure(Status const& status) {
    return !status.ok() && status.code() != StatusCode::kUnavailable;
  }
};

class TestUniverseDomainRetryPolicy
    : public internal::UniverseDomainRetryPolicy {
 public:
  ~TestUniverseDomainRetryPolicy() override = default;

  explicit TestUniverseDomainRetryPolicy(int maximum_failures)
      : impl_(maximum_failures) {}
  TestUniverseDomainRetryPolicy(TestUniverseDomainRetryPolicy&& rhs) noexcept
      : TestUniverseDomainRetryPolicy(rhs.maximum_failures()) {}
  TestUniverseDomainRetryPolicy(
      TestUniverseDomainRetryPolicy const& rhs) noexcept
      : TestUniverseDomainRetryPolicy(rhs.maximum_failures()) {}

  bool OnFailure(Status const& status) override {
    return impl_.OnFailure(status);
  }
  bool IsExhausted() const override { return impl_.IsExhausted(); }
  bool IsPermanentFailure(Status const& status) const override {
    return impl_.IsPermanentFailure(status);
  }
  int maximum_failures() const { return impl_.maximum_failures(); }
  std::unique_ptr<google::cloud::RetryPolicy> clone() const override {
    return std::make_unique<TestUniverseDomainRetryPolicy>(maximum_failures());
  }

 private:
  internal::LimitedErrorCountRetryPolicy<TestUniverseDomainRetryTraits> impl_;
};

TEST(ComputeEngineCredentialsTest,
     UniverseDomainCredentialsOptionsCustomRetryPolicy) {
  auto client = std::make_unique<MockRestClient>();
  EXPECT_CALL(*client, Get(_, expected_universe_domain_request()))
      .Times(3)
      .WillRepeatedly([&](RestContext&, RestRequest const&) {
        return internal::UnavailableError("Transient Error");
      });

  auto credentials_options =
      Options{}.set<internal::UniverseDomainRetryPolicyOption>(
          std::make_unique<TestUniverseDomainRetryPolicy>(2)->clone());
  MockHttpClientFactory client_factory;
  EXPECT_CALL(client_factory, Call).WillOnce(Return(ByMove(std::move(client))));
  ComputeEngineCredentials credentials(credentials_options,
                                       client_factory.AsStdFunction());
  EXPECT_THAT(credentials.universe_domain(),
              StatusIs(StatusCode::kUnavailable));
}

TEST(ComputeEngineCredentialsTest, UniverseDomainCallOptionsCustomRetryPolicy) {
  auto client = std::make_unique<MockRestClient>();
  EXPECT_CALL(*client, Get(_, expected_universe_domain_request()))
      .Times(4)
      .WillRepeatedly([&](RestContext&, RestRequest const&) {
        return internal::UnavailableError("Transient Error");
      });

  auto call_options = Options{}.set<internal::UniverseDomainRetryPolicyOption>(
      std::make_unique<TestUniverseDomainRetryPolicy>(3)->clone());
  auto credentials_options =
      Options{}.set<internal::UniverseDomainRetryPolicyOption>(
          std::make_unique<TestUniverseDomainRetryPolicy>(2)->clone());
  MockHttpClientFactory client_factory;
  EXPECT_CALL(client_factory, Call).WillOnce(Return(ByMove(std::move(client))));
  ComputeEngineCredentials credentials(credentials_options,
                                       client_factory.AsStdFunction());
  EXPECT_THAT(credentials.universe_domain(call_options),
              StatusIs(StatusCode::kUnavailable));
}

TEST(ComputeEngineCredentialsTest, ProjectIdSuccess) {
  auto expected_request = []() {
    auto const expected_path = absl::StrCat(
        internal::GceMetadataScheme(), "://", internal::GceMetadataHostname(),
        "/computeMetadata/v1/project/project-id");
    return AllOf(
        Property(&RestRequest::path, expected_path),
        Property(&RestRequest::headers,
                 Contains(Pair("metadata-flavor", Contains("Google")))));
  };

  auto const expected = std::string{"test-only-project-id"};

  MockHttpClientFactory client_factory;
  EXPECT_CALL(client_factory, Call)
      .WillOnce([&] {
        auto client = std::make_unique<MockRestClient>();
        EXPECT_CALL(*client, Get(_, expected_request())).WillOnce([] {
          return internal::UnavailableError("Transient Error");
        });
        return client;
      })
      .WillOnce([&] {
        auto client = std::make_unique<MockRestClient>();
        EXPECT_CALL(*client, Get(_, expected_request())).WillOnce([&] {
          auto response = std::make_unique<MockRestResponse>();
          EXPECT_CALL(*response, StatusCode)
              .WillRepeatedly(Return(HttpStatusCode::kOk));
          EXPECT_CALL(std::move(*response), ExtractPayload).WillOnce([&] {
            return MakeMockHttpPayloadSuccess(expected);
          });
          return std::unique_ptr<RestResponse>(std::move(response));
        });
        return client;
      });
  ComputeEngineCredentials credentials(Options{},
                                       client_factory.AsStdFunction());
  // The first attempt fails, no retry policies for project id, so the error
  // should be returned to the caller.
  EXPECT_THAT(credentials.project_id(), StatusIs(StatusCode::kUnavailable));
  // The error is not cached, a second request may succeed.
  EXPECT_THAT(credentials.project_id(), IsOkAndHolds(expected));
  // Verify the value is cached and further lookups do not create requests.
  EXPECT_THAT(credentials.project_id(), IsOkAndHolds(expected));
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace oauth2_internal
}  // namespace cloud
}  // namespace google
