// Copyright 2023 Google LLC
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

#include "generator/integration_tests/golden/v1/golden_kitchen_sink_rest_connection.h"
#include "generator/integration_tests/golden/v1/golden_kitchen_sink_connection.h"
#include "generator/integration_tests/golden/v1/golden_kitchen_sink_options.h"
#include "generator/integration_tests/golden/v1/internal/golden_kitchen_sink_option_defaults.h"
#include "generator/integration_tests/golden/v1/internal/golden_kitchen_sink_rest_connection_impl.h"
#include "generator/integration_tests/tests/mock_golden_kitchen_sink_rest_stub.h"
#include "google/cloud/common_options.h"
#include "google/cloud/grpc_options.h"
#include "google/cloud/polling_policy.h"
#include "google/cloud/testing_util/opentelemetry_matchers.h"
#include "google/cloud/testing_util/scoped_log.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>
#include <memory>

namespace google {
namespace cloud {
namespace golden_v1 {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::google::cloud::golden_v1_internal::GoldenKitchenSinkDefaultOptions;
using ::google::cloud::golden_v1_internal::GoldenKitchenSinkRestConnectionImpl;
using ::google::cloud::golden_v1_internal::GoldenKitchenSinkRestStub;
using ::google::cloud::golden_v1_internal::MockGoldenKitchenSinkRestStub;
using ::testing::AtLeast;
using ::testing::Contains;
using ::testing::ContainsRegex;
using ::testing::ElementsAre;
using ::testing::Return;

std::shared_ptr<GoldenKitchenSinkConnection> CreateTestingConnection(
    std::shared_ptr<GoldenKitchenSinkRestStub> mock) {
  GoldenKitchenSinkLimitedErrorCountRetryPolicy retry(
      /*maximum_failures=*/2);
  ExponentialBackoffPolicy backoff(
      /*initial_delay=*/std::chrono::microseconds(1),
      /*maximum_delay=*/std::chrono::microseconds(1),
      /*scaling=*/2.0);
  GenericPollingPolicy<GoldenKitchenSinkLimitedErrorCountRetryPolicy,
                       ExponentialBackoffPolicy>
      polling(retry, backoff);
  auto options = GoldenKitchenSinkDefaultOptions(
      Options{}
          .set<GoldenKitchenSinkRetryPolicyOption>(retry.clone())
          .set<GoldenKitchenSinkBackoffPolicyOption>(backoff.clone()));
  auto background = internal::MakeBackgroundThreadsFactory(options)();
  return std::make_shared<
      golden_v1_internal::GoldenKitchenSinkRestConnectionImpl>(
      std::move(background), std::move(mock), std::move(options));
}

TEST(GoldenKitchenSinkConnectionTest, GenerateAccessTokenSuccess) {
  auto mock = std::make_shared<MockGoldenKitchenSinkRestStub>();
  EXPECT_CALL(*mock, GenerateAccessToken)
      .WillOnce([](rest_internal::RestContext&, Options const&,
                   ::google::test::admin::database::v1::
                       GenerateAccessTokenRequest const&) {
        ::google::test::admin::database::v1::GenerateAccessTokenResponse
            response;
        return response;
      });
  auto conn = CreateTestingConnection(std::move(mock));
  ::google::test::admin::database::v1::GenerateAccessTokenRequest request;
  google::cloud::internal::OptionsSpan span(
      google::cloud::internal::MergeOptions(Options{}, conn->options()));
  auto response = conn->GenerateAccessToken(request);
  EXPECT_STATUS_OK(response);
}

TEST(GoldenKitchenSinkConnectionTest, GenerateAccessTokenPermanentError) {
  auto mock = std::make_shared<MockGoldenKitchenSinkRestStub>();
  EXPECT_CALL(*mock, GenerateAccessToken)
      .WillOnce(Return(Status(StatusCode::kPermissionDenied, "uh-oh")));
  auto conn = CreateTestingConnection(std::move(mock));
  ::google::test::admin::database::v1::GenerateAccessTokenRequest request;
  google::cloud::internal::OptionsSpan span(
      google::cloud::internal::MergeOptions(Options{}, conn->options()));
  auto response = conn->GenerateAccessToken(request);
  EXPECT_EQ(StatusCode::kPermissionDenied, response.status().code());
}

// method is NonIdempotent, so one transient is too many
TEST(GoldenKitchenSinkConnectionTest, GenerateAccessTokenTooManyTransients) {
  auto mock = std::make_shared<MockGoldenKitchenSinkRestStub>();
  EXPECT_CALL(*mock, GenerateAccessToken)
      .Times(AtLeast(1))
      .WillRepeatedly(Return(Status(StatusCode::kUnavailable, "try-again")));
  auto conn = CreateTestingConnection(std::move(mock));
  ::google::test::admin::database::v1::GenerateAccessTokenRequest request;
  google::cloud::internal::OptionsSpan span(
      google::cloud::internal::MergeOptions(Options{}, conn->options()));
  auto response = conn->GenerateAccessToken(request);
  EXPECT_EQ(StatusCode::kUnavailable, response.status().code());
}

TEST(GoldenKitchenSinkConnectionTest, GenerateIdTokenSuccess) {
  auto mock = std::make_shared<MockGoldenKitchenSinkRestStub>();
  EXPECT_CALL(*mock, GenerateIdToken)
      .WillOnce([](rest_internal::RestContext&, Options const&,
                   ::google::test::admin::database::v1::
                       GenerateIdTokenRequest const&) {
        ::google::test::admin::database::v1::GenerateIdTokenResponse response;
        return response;
      });
  auto conn = CreateTestingConnection(std::move(mock));
  ::google::test::admin::database::v1::GenerateIdTokenRequest request;
  google::cloud::internal::OptionsSpan span(
      google::cloud::internal::MergeOptions(Options{}, conn->options()));
  auto response = conn->GenerateIdToken(request);
  EXPECT_STATUS_OK(response);
}

TEST(GoldenKitchenSinkConnectionTest, GenerateIdTokenPermanentError) {
  auto mock = std::make_shared<MockGoldenKitchenSinkRestStub>();
  EXPECT_CALL(*mock, GenerateIdToken)
      .WillOnce(Return(Status(StatusCode::kPermissionDenied, "uh-oh")));
  auto conn = CreateTestingConnection(std::move(mock));
  ::google::test::admin::database::v1::GenerateIdTokenRequest request;
  google::cloud::internal::OptionsSpan span(
      google::cloud::internal::MergeOptions(Options{}, conn->options()));
  auto response = conn->GenerateIdToken(request);
  EXPECT_EQ(StatusCode::kPermissionDenied, response.status().code());
}

// method is NonIdempotent, so one transient is too many
TEST(GoldenKitchenSinkConnectionTest, GenerateIdTokenTooManyTransients) {
  auto mock = std::make_shared<MockGoldenKitchenSinkRestStub>();
  EXPECT_CALL(*mock, GenerateIdToken)
      .Times(AtLeast(1))
      .WillRepeatedly(Return(Status(StatusCode::kUnavailable, "try-again")));
  auto conn = CreateTestingConnection(std::move(mock));
  ::google::test::admin::database::v1::GenerateIdTokenRequest request;
  google::cloud::internal::OptionsSpan span(
      google::cloud::internal::MergeOptions(Options{}, conn->options()));
  auto response = conn->GenerateIdToken(request);
  EXPECT_EQ(StatusCode::kUnavailable, response.status().code());
}

TEST(GoldenKitchenSinkConnectionTest, WriteLogEntriesSuccess) {
  auto mock = std::make_shared<MockGoldenKitchenSinkRestStub>();
  EXPECT_CALL(*mock, WriteLogEntries)
      .WillOnce([](rest_internal::RestContext&, Options const&,
                   ::google::test::admin::database::v1::
                       WriteLogEntriesRequest const&) {
        ::google::test::admin::database::v1::WriteLogEntriesResponse response;
        return response;
      });
  auto conn = CreateTestingConnection(std::move(mock));
  ::google::test::admin::database::v1::WriteLogEntriesRequest request;
  google::cloud::internal::OptionsSpan span(
      google::cloud::internal::MergeOptions(Options{}, conn->options()));
  auto response = conn->WriteLogEntries(request);
  EXPECT_STATUS_OK(response);
}

TEST(GoldenKitchenSinkConnectionTest, WriteLogEntriesPermanentError) {
  auto mock = std::make_shared<MockGoldenKitchenSinkRestStub>();
  EXPECT_CALL(*mock, WriteLogEntries)
      .WillOnce(Return(Status(StatusCode::kPermissionDenied, "uh-oh")));
  auto conn = CreateTestingConnection(std::move(mock));
  ::google::test::admin::database::v1::WriteLogEntriesRequest request;
  google::cloud::internal::OptionsSpan span(
      google::cloud::internal::MergeOptions(Options{}, conn->options()));
  auto response = conn->WriteLogEntries(request);
  EXPECT_EQ(StatusCode::kPermissionDenied, response.status().code());
}

// method is NonIdempotent, so one transient is too many
TEST(GoldenKitchenSinkConnectionTest, WriteLogEntriesTooManyTransients) {
  auto mock = std::make_shared<MockGoldenKitchenSinkRestStub>();
  EXPECT_CALL(*mock, WriteLogEntries)
      .Times(AtLeast(1))
      .WillRepeatedly(Return(Status(StatusCode::kUnavailable, "try-again")));
  auto conn = CreateTestingConnection(std::move(mock));
  ::google::test::admin::database::v1::WriteLogEntriesRequest request;
  google::cloud::internal::OptionsSpan span(
      google::cloud::internal::MergeOptions(Options{}, conn->options()));
  auto response = conn->WriteLogEntries(request);
  EXPECT_EQ(StatusCode::kUnavailable, response.status().code());
}

TEST(GoldenKitchenSinkConnectionTest, ListLogsSuccess) {
  auto mock = std::make_shared<MockGoldenKitchenSinkRestStub>();
  std::string const expected_parent = "projects/my-project";
  EXPECT_CALL(*mock, ListLogs)
      .WillOnce([&expected_parent](
                    rest_internal::RestContext&, Options const&,
                    ::google::test::admin::database::v1::ListLogsRequest const&
                        request) {
        EXPECT_EQ(expected_parent, request.parent());
        EXPECT_TRUE(request.page_token().empty());
        ::google::test::admin::database::v1::ListLogsResponse page;
        page.set_next_page_token("page-1");
        *page.add_log_names() = "log1";
        return make_status_or(page);
      })
      .WillOnce([&expected_parent](
                    rest_internal::RestContext&, Options const&,
                    ::google::test::admin::database::v1::ListLogsRequest const&
                        request) {
        EXPECT_EQ(expected_parent, request.parent());
        EXPECT_EQ("page-1", request.page_token());
        ::google::test::admin::database::v1::ListLogsResponse page;
        page.set_next_page_token("page-2");
        *page.add_log_names() = "log2";
        return make_status_or(page);
      })
      .WillOnce([&expected_parent](
                    rest_internal::RestContext&, Options const&,
                    ::google::test::admin::database::v1::ListLogsRequest const&
                        request) {
        EXPECT_EQ(expected_parent, request.parent());
        EXPECT_EQ("page-2", request.page_token());
        ::google::test::admin::database::v1::ListLogsResponse page;
        page.clear_next_page_token();
        *page.add_log_names() = "log3";
        return make_status_or(page);
      });
  auto conn = CreateTestingConnection(std::move(mock));
  std::vector<std::string> actual_log_names;
  ::google::test::admin::database::v1::ListLogsRequest request;
  request.set_parent("projects/my-project");
  google::cloud::internal::OptionsSpan span(
      google::cloud::internal::MergeOptions(Options{}, conn->options()));
  for (auto const& log_name : conn->ListLogs(request)) {
    ASSERT_STATUS_OK(log_name);
    actual_log_names.push_back(*log_name);
  }
  EXPECT_THAT(actual_log_names, ElementsAre("log1", "log2", "log3"));
}

TEST(GoldenKitchenSinkConnectionTest, ListLogsPermanentError) {
  auto mock = std::make_shared<MockGoldenKitchenSinkRestStub>();
  EXPECT_CALL(*mock, ListLogs)
      .WillOnce(Return(Status(StatusCode::kPermissionDenied, "uh-oh")));
  auto conn = CreateTestingConnection(std::move(mock));
  ::google::test::admin::database::v1::ListLogsRequest request;
  request.set_parent("projects/my-project");
  google::cloud::internal::OptionsSpan span(
      google::cloud::internal::MergeOptions(Options{}, conn->options()));
  auto range = conn->ListLogs(request);
  auto begin = range.begin();
  ASSERT_NE(begin, range.end());
  EXPECT_EQ(StatusCode::kPermissionDenied, begin->status().code());
}

TEST(GoldenKitchenSinkConnectionTest, ListLogsTooManyTransients) {
  auto mock = std::make_shared<MockGoldenKitchenSinkRestStub>();
  EXPECT_CALL(*mock, ListLogs)
      .WillOnce(Return(Status(StatusCode::kUnavailable, "try-again")));
  auto conn = CreateTestingConnection(std::move(mock));
  ::google::test::admin::database::v1::ListLogsRequest request;
  request.set_parent("projects/my-project");
  google::cloud::internal::OptionsSpan span(
      google::cloud::internal::MergeOptions(Options{}, conn->options()));
  auto range = conn->ListLogs(request);
  auto begin = range.begin();
  ASSERT_NE(begin, range.end());
  EXPECT_EQ(StatusCode::kUnavailable, begin->status().code());
}

TEST(GoldenKitchenSinkConnectionTest, ListServiceAccountKeysSuccess) {
  auto mock = std::make_shared<MockGoldenKitchenSinkRestStub>();
  EXPECT_CALL(*mock, ListServiceAccountKeys)
      .WillOnce(Return(Status(StatusCode::kUnavailable, "try-again")))
      .WillOnce([](rest_internal::RestContext&, Options const&,
                   ::google::test::admin::database::v1::
                       ListServiceAccountKeysRequest const&) {
        ::google::test::admin::database::v1::ListServiceAccountKeysResponse
            response;
        return response;
      });
  auto conn = CreateTestingConnection(std::move(mock));
  ::google::test::admin::database::v1::ListServiceAccountKeysRequest request;
  google::cloud::internal::OptionsSpan span(
      google::cloud::internal::MergeOptions(Options{}, conn->options()));
  auto response = conn->ListServiceAccountKeys(request);
  EXPECT_STATUS_OK(response);
}

TEST(GoldenKitchenSinkConnectionTest, ListServiceAccountKeysTooManyTransients) {
  auto mock = std::make_shared<MockGoldenKitchenSinkRestStub>();
  EXPECT_CALL(*mock, ListServiceAccountKeys)
      .Times(AtLeast(2))
      .WillRepeatedly(Return(Status(StatusCode::kUnavailable, "try-again")));
  auto conn = CreateTestingConnection(std::move(mock));
  ::google::test::admin::database::v1::ListServiceAccountKeysRequest request;
  google::cloud::internal::OptionsSpan span(
      google::cloud::internal::MergeOptions(Options{}, conn->options()));
  auto response = conn->ListServiceAccountKeys(request);
  EXPECT_EQ(StatusCode::kUnavailable, response.status().code());
}

TEST(GoldenKitchenSinkConnectionTest, ListServiceAccountKeysPermanentError) {
  auto mock = std::make_shared<MockGoldenKitchenSinkRestStub>();
  EXPECT_CALL(*mock, ListServiceAccountKeys)
      .WillOnce(Return(Status(StatusCode::kPermissionDenied, "uh-oh")));
  auto conn = CreateTestingConnection(std::move(mock));
  ::google::test::admin::database::v1::ListServiceAccountKeysRequest request;
  google::cloud::internal::OptionsSpan span(
      google::cloud::internal::MergeOptions(Options{}, conn->options()));
  auto response = conn->ListServiceAccountKeys(request);
  EXPECT_EQ(StatusCode::kPermissionDenied, response.status().code());
}

TEST(GoldenKitchenSinkConnectionTest, CheckExpectedOptions) {
  struct UnexpectedOption {
    using Type = int;
  };
  testing_util::ScopedLog log;
  auto opts = Options{}.set<UnexpectedOption>({});
  auto conn = MakeGoldenKitchenSinkConnection(std::move(opts));
  EXPECT_THAT(log.ExtractLines(),
              Contains(ContainsRegex("Unexpected option.+UnexpectedOption")));
}

#ifdef GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY
using ::google::cloud::testing_util::DisableTracing;
using ::google::cloud::testing_util::EnableTracing;
using ::google::cloud::testing_util::SpanNamed;
using ::testing::Not;

TEST(GoldenKitchenSinkConnectionTest, TracingEnabled) {
  auto span_catcher = testing_util::InstallSpanCatcher();

  auto options = EnableTracing(
      Options{}
          .set<EndpointOption>("localhost:1")
          .set<GoldenKitchenSinkRetryPolicyOption>(
              GoldenKitchenSinkLimitedErrorCountRetryPolicy(0).clone()));
  auto conn = MakeGoldenKitchenSinkConnectionRest(std::move(options));
  google::cloud::internal::OptionsSpan span(
      google::cloud::internal::MergeOptions(Options{}, conn->options()));
  // Make a call, which should fail fast. The error itself is not important.
  (void)conn->DoNothing({});

  auto spans = span_catcher->GetSpans();
  EXPECT_THAT(
      spans,
      Contains(SpanNamed("golden_v1::GoldenKitchenSinkConnection::DoNothing")));
}

TEST(GoldenKitchenSinkConnectionTest, TracingDisabled) {
  auto span_catcher = testing_util::InstallSpanCatcher();

  auto options = DisableTracing(
      Options{}
          .set<EndpointOption>("localhost:1")
          .set<GoldenKitchenSinkRetryPolicyOption>(
              GoldenKitchenSinkLimitedErrorCountRetryPolicy(0).clone()));
  auto conn = MakeGoldenKitchenSinkConnectionRest(std::move(options));
  google::cloud::internal::OptionsSpan span(
      google::cloud::internal::MergeOptions(Options{}, conn->options()));
  // Make a call, which should fail fast. The error itself is not important.
  (void)conn->DoNothing({});

  auto spans = span_catcher->GetSpans();
  EXPECT_THAT(spans,
              Not(Contains(SpanNamed(
                  "golden_v1::GoldenKitchenSinkConnection::DoNothing"))));
}
#endif  // GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace golden_v1
}  // namespace cloud
}  // namespace google
