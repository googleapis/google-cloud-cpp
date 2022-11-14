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

#include "generator/integration_tests/golden/internal/golden_kitchen_sink_rest_logging_decorator.h"
#include "google/cloud/internal/rest_context.h"
#include "google/cloud/testing_util/scoped_log.h"
#include "google/cloud/testing_util/status_matchers.h"
#include "absl/memory/memory.h"
#include "generator/integration_tests/tests/mock_golden_kitchen_sink_rest_stub.h"
#include <gmock/gmock.h>
#include <memory>

namespace google {
namespace cloud {
namespace golden_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::testing::Contains;
using ::testing::HasSubstr;
using ::testing::Return;

Status TransientError() {
  return Status(StatusCode::kUnavailable, "try-again");
}

TEST(LoggingDecoratorRestTest, GenerateAccessToken) {
  testing_util::ScopedLog log;
  auto mock = std::make_shared<MockGoldenKitchenSinkRestStub>();
  ::google::test::admin::database::v1::GenerateAccessTokenResponse response;
  EXPECT_CALL(*mock, GenerateAccessToken).WillOnce(Return(response));
  GoldenKitchenSinkRestLogging stub(mock, TracingOptions{}, {});
  rest_internal::RestContext context;
  auto status = stub.GenerateAccessToken(
      context, google::test::admin::database::v1::GenerateAccessTokenRequest());
  EXPECT_STATUS_OK(status);

  auto const log_lines = log.ExtractLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("GenerateAccessToken")));
}

TEST(LoggingDecoratorRestTest, GenerateAccessTokenError) {
  testing_util::ScopedLog log;
  auto mock = std::make_shared<MockGoldenKitchenSinkRestStub>();
  EXPECT_CALL(*mock, GenerateAccessToken).WillOnce(Return(TransientError()));
  GoldenKitchenSinkRestLogging stub(mock, TracingOptions{}, {});
  rest_internal::RestContext context;
  auto status = stub.GenerateAccessToken(
      context, google::test::admin::database::v1::GenerateAccessTokenRequest());
  EXPECT_EQ(TransientError(), status.status());

  auto const log_lines = log.ExtractLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("GenerateAccessToken")));
  EXPECT_THAT(log_lines, Contains(HasSubstr(TransientError().message())));
}

TEST(LoggingDecoratorRestTest, GenerateIdToken) {
  testing_util::ScopedLog log;
  auto mock = std::make_shared<MockGoldenKitchenSinkRestStub>();
  ::google::test::admin::database::v1::GenerateIdTokenResponse response;
  EXPECT_CALL(*mock, GenerateIdToken).WillOnce(Return(response));
  GoldenKitchenSinkRestLogging stub(mock, TracingOptions{}, {});
  rest_internal::RestContext context;
  auto status = stub.GenerateIdToken(
      context, google::test::admin::database::v1::GenerateIdTokenRequest());
  EXPECT_STATUS_OK(status);

  auto const log_lines = log.ExtractLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("GenerateIdToken")));
}

TEST(LoggingDecoratorRestTest, GenerateIdTokenError) {
  testing_util::ScopedLog log;
  auto mock = std::make_shared<MockGoldenKitchenSinkRestStub>();
  EXPECT_CALL(*mock, GenerateIdToken).WillOnce(Return(TransientError()));
  GoldenKitchenSinkRestLogging stub(mock, TracingOptions{}, {});
  rest_internal::RestContext context;
  auto status = stub.GenerateIdToken(
      context, google::test::admin::database::v1::GenerateIdTokenRequest());
  EXPECT_EQ(TransientError(), status.status());

  auto const log_lines = log.ExtractLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("GenerateIdToken")));
  EXPECT_THAT(log_lines, Contains(HasSubstr(TransientError().message())));
}

TEST(LoggingDecoratorRestTest, WriteLogEntries) {
  testing_util::ScopedLog log;
  auto mock = std::make_shared<MockGoldenKitchenSinkRestStub>();
  ::google::test::admin::database::v1::WriteLogEntriesResponse response;
  EXPECT_CALL(*mock, WriteLogEntries).WillOnce(Return(response));
  GoldenKitchenSinkRestLogging stub(mock, TracingOptions{}, {});
  rest_internal::RestContext context;
  auto status = stub.WriteLogEntries(
      context, google::test::admin::database::v1::WriteLogEntriesRequest());
  EXPECT_STATUS_OK(status);

  auto const log_lines = log.ExtractLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("WriteLogEntries")));
}

TEST(LoggingDecoratorRestTest, WriteLogEntriesError) {
  testing_util::ScopedLog log;
  auto mock = std::make_shared<MockGoldenKitchenSinkRestStub>();
  EXPECT_CALL(*mock, WriteLogEntries).WillOnce(Return(TransientError()));
  GoldenKitchenSinkRestLogging stub(mock, TracingOptions{}, {});
  rest_internal::RestContext context;
  auto status = stub.WriteLogEntries(
      context, google::test::admin::database::v1::WriteLogEntriesRequest());
  EXPECT_EQ(TransientError(), status.status());

  auto const log_lines = log.ExtractLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("WriteLogEntries")));
  EXPECT_THAT(log_lines, Contains(HasSubstr(TransientError().message())));
}

TEST(LoggingDecoratorRestTest, ListLogs) {
  testing_util::ScopedLog log;
  auto mock = std::make_shared<MockGoldenKitchenSinkRestStub>();
  ::google::test::admin::database::v1::ListLogsResponse response;
  EXPECT_CALL(*mock, ListLogs).WillOnce(Return(response));
  GoldenKitchenSinkRestLogging stub(mock, TracingOptions{}, {});
  rest_internal::RestContext context;
  auto status = stub.ListLogs(
      context, google::test::admin::database::v1::ListLogsRequest());
  EXPECT_STATUS_OK(status);

  auto const log_lines = log.ExtractLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("ListLogs")));
}

TEST(LoggingDecoratorRestTest, ListLogsError) {
  testing_util::ScopedLog log;
  auto mock = std::make_shared<MockGoldenKitchenSinkRestStub>();
  EXPECT_CALL(*mock, ListLogs).WillOnce(Return(TransientError()));
  GoldenKitchenSinkRestLogging stub(mock, TracingOptions{}, {});
  rest_internal::RestContext context;
  auto status = stub.ListLogs(
      context, google::test::admin::database::v1::ListLogsRequest());
  EXPECT_EQ(TransientError(), status.status());

  auto const log_lines = log.ExtractLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("ListLogs")));
  EXPECT_THAT(log_lines, Contains(HasSubstr(TransientError().message())));
}

TEST(LoggingDecoratorRestTest, ListServiceAccountKeys) {
  testing_util::ScopedLog log;
  auto mock = std::make_shared<MockGoldenKitchenSinkRestStub>();
  ::google::test::admin::database::v1::ListServiceAccountKeysResponse response;
  EXPECT_CALL(*mock, ListServiceAccountKeys).WillOnce(Return(response));
  GoldenKitchenSinkRestLogging stub(mock, TracingOptions{}, {});
  rest_internal::RestContext context;
  auto status = stub.ListServiceAccountKeys(
      context,
      google::test::admin::database::v1::ListServiceAccountKeysRequest());
  EXPECT_STATUS_OK(status);

  auto const log_lines = log.ExtractLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("ListServiceAccountKeys")));
}

TEST(LoggingDecoratorRestTest, ListServiceAccountKeysError) {
  testing_util::ScopedLog log;
  auto mock = std::make_shared<MockGoldenKitchenSinkRestStub>();
  EXPECT_CALL(*mock, ListServiceAccountKeys).WillOnce(Return(TransientError()));
  GoldenKitchenSinkRestLogging stub(mock, TracingOptions{}, {});
  rest_internal::RestContext context;
  auto status = stub.ListServiceAccountKeys(
      context,
      google::test::admin::database::v1::ListServiceAccountKeysRequest());
  EXPECT_EQ(TransientError(), status.status());

  auto const log_lines = log.ExtractLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("ListServiceAccountKeys")));
  EXPECT_THAT(log_lines, Contains(HasSubstr(TransientError().message())));
}

TEST(LoggingDecoratorRestTest, DoNothing) {
  testing_util::ScopedLog log;
  auto mock = std::make_shared<MockGoldenKitchenSinkRestStub>();
  EXPECT_CALL(*mock, DoNothing).WillOnce(Return(Status{}));
  GoldenKitchenSinkRestLogging stub(mock, TracingOptions{}, {});
  rest_internal::RestContext context;
  auto status = stub.DoNothing(context, google::protobuf::Empty());
  EXPECT_STATUS_OK(status);

  auto const log_lines = log.ExtractLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("DoNothing")));
}

TEST(LoggingDecoratorRestTest, DoNothingError) {
  testing_util::ScopedLog log;
  auto mock = std::make_shared<MockGoldenKitchenSinkRestStub>();
  EXPECT_CALL(*mock, DoNothing).WillOnce(Return(TransientError()));
  GoldenKitchenSinkRestLogging stub(mock, TracingOptions{}, {});
  rest_internal::RestContext context;
  auto status = stub.DoNothing(context, google::protobuf::Empty());
  EXPECT_EQ(TransientError(), status);

  auto const log_lines = log.ExtractLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("DoNothing")));
  EXPECT_THAT(log_lines, Contains(HasSubstr(TransientError().message())));
}

TEST(LoggingDecoratorRestTest, ExplicitRouting1) {
  testing_util::ScopedLog log;
  auto mock = std::make_shared<MockGoldenKitchenSinkRestStub>();
  EXPECT_CALL(*mock, ExplicitRouting1).WillOnce(Return(Status{}));
  GoldenKitchenSinkRestLogging stub(mock, TracingOptions{}, {});
  rest_internal::RestContext context;
  auto status = stub.ExplicitRouting1(
      context, google::test::admin::database::v1::ExplicitRoutingRequest());
  EXPECT_STATUS_OK(status);

  auto const log_lines = log.ExtractLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("ExplicitRouting1")));
}

TEST(LoggingDecoratorRestTest, ExplicitRouting1Error) {
  testing_util::ScopedLog log;
  auto mock = std::make_shared<MockGoldenKitchenSinkRestStub>();
  EXPECT_CALL(*mock, ExplicitRouting1).WillOnce(Return(TransientError()));
  GoldenKitchenSinkRestLogging stub(mock, TracingOptions{}, {});
  rest_internal::RestContext context;
  auto status = stub.ExplicitRouting1(
      context, google::test::admin::database::v1::ExplicitRoutingRequest());
  EXPECT_EQ(TransientError(), status);

  auto const log_lines = log.ExtractLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("ExplicitRouting1")));
  EXPECT_THAT(log_lines, Contains(HasSubstr(TransientError().message())));
}

TEST(LoggingDecoratorRestTest, ExplicitRouting2) {
  testing_util::ScopedLog log;
  auto mock = std::make_shared<MockGoldenKitchenSinkRestStub>();
  EXPECT_CALL(*mock, ExplicitRouting2).WillOnce(Return(Status{}));
  GoldenKitchenSinkRestLogging stub(mock, TracingOptions{}, {});
  rest_internal::RestContext context;
  auto status = stub.ExplicitRouting2(
      context, google::test::admin::database::v1::ExplicitRoutingRequest());
  EXPECT_STATUS_OK(status);

  auto const log_lines = log.ExtractLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("ExplicitRouting2")));
}

TEST(LoggingDecoratorRestTest, ExplicitRouting2Error) {
  testing_util::ScopedLog log;
  auto mock = std::make_shared<MockGoldenKitchenSinkRestStub>();
  EXPECT_CALL(*mock, ExplicitRouting2).WillOnce(Return(TransientError()));
  GoldenKitchenSinkRestLogging stub(mock, TracingOptions{}, {});
  rest_internal::RestContext context;
  auto status = stub.ExplicitRouting2(
      context, google::test::admin::database::v1::ExplicitRoutingRequest());
  EXPECT_EQ(TransientError(), status);

  auto const log_lines = log.ExtractLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("ExplicitRouting2")));
  EXPECT_THAT(log_lines, Contains(HasSubstr(TransientError().message())));
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace golden_internal
}  // namespace cloud
}  // namespace google
