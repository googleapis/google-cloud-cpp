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

class LoggingDecoratorRestTest : public ::testing::Test {
 protected:
  void SetUp() override {
    mock_ = std::make_shared<MockGoldenKitchenSinkRestStub>();
  }

  static Status TransientError() {
    return Status(StatusCode::kUnavailable, "try-again");
  }

  std::shared_ptr<MockGoldenKitchenSinkRestStub> mock_;
  testing_util::ScopedLog log_;
};

TEST_F(LoggingDecoratorRestTest, GenerateAccessToken) {
  ::google::test::admin::database::v1::GenerateAccessTokenResponse response;
  EXPECT_CALL(*mock_, GenerateAccessToken).WillOnce(Return(response));
  GoldenKitchenSinkRestLogging stub(mock_, TracingOptions{}, {});
  rest_internal::RestContext context;
  auto status = stub.GenerateAccessToken(
      context, google::test::admin::database::v1::GenerateAccessTokenRequest());
  EXPECT_STATUS_OK(status);

  auto const log_lines = log_.ExtractLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("GenerateAccessToken")));
}

TEST_F(LoggingDecoratorRestTest, GenerateAccessTokenError) {
  EXPECT_CALL(*mock_, GenerateAccessToken).WillOnce(Return(TransientError()));
  GoldenKitchenSinkRestLogging stub(mock_, TracingOptions{}, {});
  rest_internal::RestContext context;
  auto status = stub.GenerateAccessToken(
      context, google::test::admin::database::v1::GenerateAccessTokenRequest());
  EXPECT_EQ(TransientError(), status.status());

  auto const log_lines = log_.ExtractLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("GenerateAccessToken")));
  EXPECT_THAT(log_lines, Contains(HasSubstr(TransientError().message())));
}

TEST_F(LoggingDecoratorRestTest, GenerateIdToken) {
  ::google::test::admin::database::v1::GenerateIdTokenResponse response;
  EXPECT_CALL(*mock_, GenerateIdToken).WillOnce(Return(response));
  GoldenKitchenSinkRestLogging stub(mock_, TracingOptions{}, {});
  rest_internal::RestContext context;
  auto status = stub.GenerateIdToken(
      context, google::test::admin::database::v1::GenerateIdTokenRequest());
  EXPECT_STATUS_OK(status);

  auto const log_lines = log_.ExtractLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("GenerateIdToken")));
}

TEST_F(LoggingDecoratorRestTest, GenerateIdTokenError) {
  EXPECT_CALL(*mock_, GenerateIdToken).WillOnce(Return(TransientError()));
  GoldenKitchenSinkRestLogging stub(mock_, TracingOptions{}, {});
  rest_internal::RestContext context;
  auto status = stub.GenerateIdToken(
      context, google::test::admin::database::v1::GenerateIdTokenRequest());
  EXPECT_EQ(TransientError(), status.status());

  auto const log_lines = log_.ExtractLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("GenerateIdToken")));
  EXPECT_THAT(log_lines, Contains(HasSubstr(TransientError().message())));
}

TEST_F(LoggingDecoratorRestTest, WriteLogEntries) {
  ::google::test::admin::database::v1::WriteLogEntriesResponse response;
  EXPECT_CALL(*mock_, WriteLogEntries).WillOnce(Return(response));
  GoldenKitchenSinkRestLogging stub(mock_, TracingOptions{}, {});
  rest_internal::RestContext context;
  auto status = stub.WriteLogEntries(
      context, google::test::admin::database::v1::WriteLogEntriesRequest());
  EXPECT_STATUS_OK(status);

  auto const log_lines = log_.ExtractLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("WriteLogEntries")));
}

TEST_F(LoggingDecoratorRestTest, WriteLogEntriesError) {
  EXPECT_CALL(*mock_, WriteLogEntries).WillOnce(Return(TransientError()));
  GoldenKitchenSinkRestLogging stub(mock_, TracingOptions{}, {});
  rest_internal::RestContext context;
  auto status = stub.WriteLogEntries(
      context, google::test::admin::database::v1::WriteLogEntriesRequest());
  EXPECT_EQ(TransientError(), status.status());

  auto const log_lines = log_.ExtractLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("WriteLogEntries")));
  EXPECT_THAT(log_lines, Contains(HasSubstr(TransientError().message())));
}

TEST_F(LoggingDecoratorRestTest, ListLogs) {
  ::google::test::admin::database::v1::ListLogsResponse response;
  EXPECT_CALL(*mock_, ListLogs).WillOnce(Return(response));
  GoldenKitchenSinkRestLogging stub(mock_, TracingOptions{}, {});
  rest_internal::RestContext context;
  auto status = stub.ListLogs(
      context, google::test::admin::database::v1::ListLogsRequest());
  EXPECT_STATUS_OK(status);

  auto const log_lines = log_.ExtractLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("ListLogs")));
}

TEST_F(LoggingDecoratorRestTest, ListLogsError) {
  EXPECT_CALL(*mock_, ListLogs).WillOnce(Return(TransientError()));
  GoldenKitchenSinkRestLogging stub(mock_, TracingOptions{}, {});
  rest_internal::RestContext context;
  auto status = stub.ListLogs(
      context, google::test::admin::database::v1::ListLogsRequest());
  EXPECT_EQ(TransientError(), status.status());

  auto const log_lines = log_.ExtractLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("ListLogs")));
  EXPECT_THAT(log_lines, Contains(HasSubstr(TransientError().message())));
}

TEST_F(LoggingDecoratorRestTest, ListServiceAccountKeys) {
  ::google::test::admin::database::v1::ListServiceAccountKeysResponse response;
  EXPECT_CALL(*mock_, ListServiceAccountKeys).WillOnce(Return(response));
  GoldenKitchenSinkRestLogging stub(mock_, TracingOptions{}, {});
  rest_internal::RestContext context;
  auto status = stub.ListServiceAccountKeys(
      context,
      google::test::admin::database::v1::ListServiceAccountKeysRequest());
  EXPECT_STATUS_OK(status);

  auto const log_lines = log_.ExtractLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("ListServiceAccountKeys")));
}

TEST_F(LoggingDecoratorRestTest, ListServiceAccountKeysError) {
  EXPECT_CALL(*mock_, ListServiceAccountKeys)
      .WillOnce(Return(TransientError()));
  GoldenKitchenSinkRestLogging stub(mock_, TracingOptions{}, {});
  rest_internal::RestContext context;
  auto status = stub.ListServiceAccountKeys(
      context,
      google::test::admin::database::v1::ListServiceAccountKeysRequest());
  EXPECT_EQ(TransientError(), status.status());

  auto const log_lines = log_.ExtractLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("ListServiceAccountKeys")));
  EXPECT_THAT(log_lines, Contains(HasSubstr(TransientError().message())));
}

TEST_F(LoggingDecoratorRestTest, DoNothing) {
  EXPECT_CALL(*mock_, DoNothing).WillOnce(Return(Status{}));
  GoldenKitchenSinkRestLogging stub(mock_, TracingOptions{}, {});
  rest_internal::RestContext context;
  auto status = stub.DoNothing(context, google::protobuf::Empty());
  EXPECT_STATUS_OK(status);

  auto const log_lines = log_.ExtractLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("DoNothing")));
}

TEST_F(LoggingDecoratorRestTest, DoNothingError) {
  EXPECT_CALL(*mock_, DoNothing).WillOnce(Return(TransientError()));
  GoldenKitchenSinkRestLogging stub(mock_, TracingOptions{}, {});
  rest_internal::RestContext context;
  auto status = stub.DoNothing(context, google::protobuf::Empty());
  EXPECT_EQ(TransientError(), status);

  auto const log_lines = log_.ExtractLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("DoNothing")));
  EXPECT_THAT(log_lines, Contains(HasSubstr(TransientError().message())));
}

TEST_F(LoggingDecoratorRestTest, ExplicitRouting1) {
  EXPECT_CALL(*mock_, ExplicitRouting1).WillOnce(Return(Status{}));
  GoldenKitchenSinkRestLogging stub(mock_, TracingOptions{}, {});
  rest_internal::RestContext context;
  auto status = stub.ExplicitRouting1(
      context, google::test::admin::database::v1::ExplicitRoutingRequest());
  EXPECT_STATUS_OK(status);

  auto const log_lines = log_.ExtractLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("ExplicitRouting1")));
}

TEST_F(LoggingDecoratorRestTest, ExplicitRouting1Error) {
  EXPECT_CALL(*mock_, ExplicitRouting1).WillOnce(Return(TransientError()));
  GoldenKitchenSinkRestLogging stub(mock_, TracingOptions{}, {});
  rest_internal::RestContext context;
  auto status = stub.ExplicitRouting1(
      context, google::test::admin::database::v1::ExplicitRoutingRequest());
  EXPECT_EQ(TransientError(), status);

  auto const log_lines = log_.ExtractLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("ExplicitRouting1")));
  EXPECT_THAT(log_lines, Contains(HasSubstr(TransientError().message())));
}

TEST_F(LoggingDecoratorRestTest, ExplicitRouting2) {
  EXPECT_CALL(*mock_, ExplicitRouting2).WillOnce(Return(Status{}));
  GoldenKitchenSinkRestLogging stub(mock_, TracingOptions{}, {});
  rest_internal::RestContext context;
  auto status = stub.ExplicitRouting2(
      context, google::test::admin::database::v1::ExplicitRoutingRequest());
  EXPECT_STATUS_OK(status);

  auto const log_lines = log_.ExtractLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("ExplicitRouting2")));
}

TEST_F(LoggingDecoratorRestTest, ExplicitRouting2Error) {
  EXPECT_CALL(*mock_, ExplicitRouting2).WillOnce(Return(TransientError()));
  GoldenKitchenSinkRestLogging stub(mock_, TracingOptions{}, {});
  rest_internal::RestContext context;
  auto status = stub.ExplicitRouting2(
      context, google::test::admin::database::v1::ExplicitRoutingRequest());
  EXPECT_EQ(TransientError(), status);

  auto const log_lines = log_.ExtractLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("ExplicitRouting2")));
  EXPECT_THAT(log_lines, Contains(HasSubstr(TransientError().message())));
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace golden_internal
}  // namespace cloud
}  // namespace google
