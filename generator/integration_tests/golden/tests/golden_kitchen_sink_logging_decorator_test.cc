// Copyright 2020 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
#include "google/cloud/log.h"
#include "google/cloud/testing_util/assert_ok.h"
#include "google/cloud/testing_util/scoped_log.h"
#include "generator/integration_tests/golden/internal/golden_kitchen_sink_logging_decorator.gcpcxx.pb.h"
#include <gmock/gmock.h>
#include <grpcpp/impl/codegen/status_code_enum.h>
#include <memory>

namespace google {
namespace cloud {
inline namespace GOOGLE_CLOUD_CPP_NS {
namespace golden_internal {
namespace {

using ::testing::_;
using ::testing::ByMove;
using ::testing::Contains;
using ::testing::HasSubstr;
using ::testing::Return;

class MockGoldenKitchenSinkStub
    : public google::cloud::golden_internal::GoldenKitchenSinkStub {
 public:
  ~MockGoldenKitchenSinkStub() override = default;
  MOCK_METHOD(
      StatusOr<
          ::google::test::admin::database::v1::GenerateAccessTokenResponse>,
      GenerateAccessToken,
      (grpc::ClientContext & context,
       ::google::test::admin::database::v1::GenerateAccessTokenRequest const&
           request),
      (override));
  MOCK_METHOD(
      StatusOr<::google::test::admin::database::v1::GenerateIdTokenResponse>,
      GenerateIdToken,
      (grpc::ClientContext & context,
       ::google::test::admin::database::v1::GenerateIdTokenRequest const&
           request),
      (override));
  MOCK_METHOD(
      StatusOr<::google::test::admin::database::v1::WriteLogEntriesResponse>,
      WriteLogEntries,
      (grpc::ClientContext & context,
       ::google::test::admin::database::v1::WriteLogEntriesRequest const&
           request),
      (override));
  MOCK_METHOD(
      StatusOr<::google::test::admin::database::v1::ListLogsResponse>, ListLogs,
      (grpc::ClientContext & context,
       ::google::test::admin::database::v1::ListLogsRequest const& request),
      (override));
  MOCK_METHOD(
      (std::unique_ptr<internal::StreamingReadRpc<
           ::google::test::admin::database::v1::TailLogEntriesResponse>>),
      TailLogEntries,
      (grpc::ClientContext & context,
       ::google::test::admin::database::v1::TailLogEntriesRequest const&
           request),
      (override));
};

class LoggingDecoratorTest : public ::testing::Test {
 protected:
  void SetUp() override {
    mock_ = std::make_shared<MockGoldenKitchenSinkStub>();
  }

  static Status TransientError() {
    return Status(StatusCode::kUnavailable, "try-again");
  }

  std::shared_ptr<MockGoldenKitchenSinkStub> mock_;
  testing_util::ScopedLog log_;
};

TEST_F(LoggingDecoratorTest, GenerateAccessToken) {
  ::google::test::admin::database::v1::GenerateAccessTokenResponse response;
  EXPECT_CALL(*mock_, GenerateAccessToken(_, _)).WillOnce(Return(response));
  GoldenKitchenSinkLogging stub(mock_, TracingOptions{}, {});
  grpc::ClientContext context;
  auto status = stub.GenerateAccessToken(
      context, google::test::admin::database::v1::GenerateAccessTokenRequest());
  EXPECT_STATUS_OK(status);

  auto const log_lines = log_.ExtractLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("GenerateAccessToken")));
}

TEST_F(LoggingDecoratorTest, GenerateAccessTokenError) {
  EXPECT_CALL(*mock_, GenerateAccessToken(_, _))
      .WillOnce(Return(TransientError()));
  GoldenKitchenSinkLogging stub(mock_, TracingOptions{}, {});
  grpc::ClientContext context;
  auto status = stub.GenerateAccessToken(
      context, google::test::admin::database::v1::GenerateAccessTokenRequest());
  EXPECT_EQ(TransientError(), status.status());

  auto const log_lines = log_.ExtractLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("GenerateAccessToken")));
  EXPECT_THAT(log_lines, Contains(HasSubstr(TransientError().message())));
}

TEST_F(LoggingDecoratorTest, GenerateIdToken) {
  ::google::test::admin::database::v1::GenerateIdTokenResponse response;
  EXPECT_CALL(*mock_, GenerateIdToken(_, _)).WillOnce(Return(response));
  GoldenKitchenSinkLogging stub(mock_, TracingOptions{}, {});
  grpc::ClientContext context;
  auto status = stub.GenerateIdToken(
      context, google::test::admin::database::v1::GenerateIdTokenRequest());
  EXPECT_STATUS_OK(status);

  auto const log_lines = log_.ExtractLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("GenerateIdToken")));
}

TEST_F(LoggingDecoratorTest, GenerateIdTokenError) {
  EXPECT_CALL(*mock_, GenerateIdToken(_, _)).WillOnce(Return(TransientError()));
  GoldenKitchenSinkLogging stub(mock_, TracingOptions{}, {});
  grpc::ClientContext context;
  auto status = stub.GenerateIdToken(
      context, google::test::admin::database::v1::GenerateIdTokenRequest());
  EXPECT_EQ(TransientError(), status.status());

  auto const log_lines = log_.ExtractLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("GenerateIdToken")));
  EXPECT_THAT(log_lines, Contains(HasSubstr(TransientError().message())));
}

TEST_F(LoggingDecoratorTest, WriteLogEntries) {
  ::google::test::admin::database::v1::WriteLogEntriesResponse response;
  EXPECT_CALL(*mock_, WriteLogEntries(_, _)).WillOnce(Return(response));
  GoldenKitchenSinkLogging stub(mock_, TracingOptions{}, {});
  grpc::ClientContext context;
  auto status = stub.WriteLogEntries(
      context, google::test::admin::database::v1::WriteLogEntriesRequest());
  EXPECT_STATUS_OK(status);

  auto const log_lines = log_.ExtractLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("WriteLogEntries")));
}

TEST_F(LoggingDecoratorTest, WriteLogEntriesError) {
  EXPECT_CALL(*mock_, WriteLogEntries(_, _)).WillOnce(Return(TransientError()));
  GoldenKitchenSinkLogging stub(mock_, TracingOptions{}, {});
  grpc::ClientContext context;
  auto status = stub.WriteLogEntries(
      context, google::test::admin::database::v1::WriteLogEntriesRequest());
  EXPECT_EQ(TransientError(), status.status());

  auto const log_lines = log_.ExtractLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("WriteLogEntries")));
  EXPECT_THAT(log_lines, Contains(HasSubstr(TransientError().message())));
}

TEST_F(LoggingDecoratorTest, ListLogs) {
  ::google::test::admin::database::v1::ListLogsResponse response;
  EXPECT_CALL(*mock_, ListLogs(_, _)).WillOnce(Return(response));
  GoldenKitchenSinkLogging stub(mock_, TracingOptions{}, {});
  grpc::ClientContext context;
  auto status = stub.ListLogs(
      context, google::test::admin::database::v1::ListLogsRequest());
  EXPECT_STATUS_OK(status);

  auto const log_lines = log_.ExtractLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("ListLogs")));
}

TEST_F(LoggingDecoratorTest, ListLogsError) {
  EXPECT_CALL(*mock_, ListLogs(_, _)).WillOnce(Return(TransientError()));
  GoldenKitchenSinkLogging stub(mock_, TracingOptions{}, {});
  grpc::ClientContext context;
  auto status = stub.ListLogs(
      context, google::test::admin::database::v1::ListLogsRequest());
  EXPECT_EQ(TransientError(), status.status());

  auto const log_lines = log_.ExtractLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("ListLogs")));
  EXPECT_THAT(log_lines, Contains(HasSubstr(TransientError().message())));
}

class MockTailLogEntriesStreamingReadRpc
    : public internal::StreamingReadRpc<
          google::test::admin::database::v1::TailLogEntriesResponse> {
 public:
  MOCK_METHOD(void, Cancel, (), (override));
  MOCK_METHOD(
      (absl::variant<
          Status, google::test::admin::database::v1::TailLogEntriesResponse>),
      Read, (), (override));
};

TEST_F(LoggingDecoratorTest, TailLogEntriesRpcNoRpcStreams) {
  auto mock_response = new MockTailLogEntriesStreamingReadRpc;
  EXPECT_CALL(*mock_response, Read).WillOnce(Return(Status()));
  EXPECT_CALL(*mock_, TailLogEntries(_, _))
      .WillOnce(Return(ByMove(
          std::unique_ptr<internal::StreamingReadRpc<
              google::test::admin::database::v1::TailLogEntriesResponse>>(
              mock_response))));
  GoldenKitchenSinkLogging stub(mock_, TracingOptions{}, {});
  grpc::ClientContext context;
  auto response = stub.TailLogEntries(
      context, google::test::admin::database::v1::TailLogEntriesRequest());
  EXPECT_TRUE(absl::get<Status>(response->Read()).ok());

  auto const log_lines = log_.ExtractLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("TailLogEntries")));
  EXPECT_THAT(log_lines, Contains(HasSubstr("null stream")));
  EXPECT_THAT(log_lines, Not(Contains(HasSubstr("Read"))));
}

TEST_F(LoggingDecoratorTest, TailLogEntriesRpcWithRpcStreams) {
  auto mock_response = new MockTailLogEntriesStreamingReadRpc;
  EXPECT_CALL(*mock_response, Read).WillOnce(Return(Status()));
  EXPECT_CALL(*mock_, TailLogEntries(_, _))
      .WillOnce(Return(ByMove(
          std::unique_ptr<internal::StreamingReadRpc<
              google::test::admin::database::v1::TailLogEntriesResponse>>(
              mock_response))));
  GoldenKitchenSinkLogging stub(mock_, TracingOptions{}, {"rpc-streams"});
  grpc::ClientContext context;
  auto response = stub.TailLogEntries(
      context, google::test::admin::database::v1::TailLogEntriesRequest());
  EXPECT_TRUE(absl::get<Status>(response->Read()).ok());

  auto const log_lines = log_.ExtractLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("TailLogEntries")));
  EXPECT_THAT(log_lines, Contains(HasSubstr("null stream")));
  EXPECT_THAT(log_lines, Contains(HasSubstr("Read")));
}

}  // namespace
}  // namespace golden_internal
}  // namespace GOOGLE_CLOUD_CPP_NS
}  // namespace cloud
}  // namespace google
