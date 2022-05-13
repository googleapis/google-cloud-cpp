// Copyright 2020 Google LLC
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

#include "generator/integration_tests/golden/internal/golden_kitchen_sink_logging_decorator.h"
#include "google/cloud/log.h"
#include "google/cloud/internal/async_streaming_read_rpc_impl.h"
#include "google/cloud/testing_util/scoped_log.h"
#include "google/cloud/testing_util/status_matchers.h"
#include "absl/memory/memory.h"
#include "generator/integration_tests/golden/mocks/mock_golden_kitchen_sink_stub.h"
#include <gmock/gmock.h>
#include <memory>

namespace google {
namespace cloud {
namespace golden_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::google::cloud::golden_internal::MockTailLogEntriesStreamingReadRpc;
using ::google::cloud::golden_internal::MockWriteObjectStreamingWriteRpc;
using ::google::cloud::testing_util::IsOk;
using ::google::cloud::testing_util::StatusIs;
using ::google::test::admin::database::v1::TailLogEntriesRequest;
using ::google::test::admin::database::v1::TailLogEntriesResponse;
using ::google::test::admin::database::v1::WriteObjectRequest;
using ::google::test::admin::database::v1::WriteObjectResponse;
using ::testing::ByMove;
using ::testing::Contains;
using ::testing::HasSubstr;
using ::testing::Return;

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
  EXPECT_CALL(*mock_, GenerateAccessToken).WillOnce(Return(response));
  GoldenKitchenSinkLogging stub(mock_, TracingOptions{}, {});
  grpc::ClientContext context;
  auto status = stub.GenerateAccessToken(
      context, google::test::admin::database::v1::GenerateAccessTokenRequest());
  EXPECT_STATUS_OK(status);

  auto const log_lines = log_.ExtractLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("GenerateAccessToken")));
}

TEST_F(LoggingDecoratorTest, GenerateAccessTokenError) {
  EXPECT_CALL(*mock_, GenerateAccessToken).WillOnce(Return(TransientError()));
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
  EXPECT_CALL(*mock_, GenerateIdToken).WillOnce(Return(response));
  GoldenKitchenSinkLogging stub(mock_, TracingOptions{}, {});
  grpc::ClientContext context;
  auto status = stub.GenerateIdToken(
      context, google::test::admin::database::v1::GenerateIdTokenRequest());
  EXPECT_STATUS_OK(status);

  auto const log_lines = log_.ExtractLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("GenerateIdToken")));
}

TEST_F(LoggingDecoratorTest, GenerateIdTokenError) {
  EXPECT_CALL(*mock_, GenerateIdToken).WillOnce(Return(TransientError()));
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
  EXPECT_CALL(*mock_, WriteLogEntries).WillOnce(Return(response));
  GoldenKitchenSinkLogging stub(mock_, TracingOptions{}, {});
  grpc::ClientContext context;
  auto status = stub.WriteLogEntries(
      context, google::test::admin::database::v1::WriteLogEntriesRequest());
  EXPECT_STATUS_OK(status);

  auto const log_lines = log_.ExtractLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("WriteLogEntries")));
}

TEST_F(LoggingDecoratorTest, WriteLogEntriesError) {
  EXPECT_CALL(*mock_, WriteLogEntries).WillOnce(Return(TransientError()));
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
  EXPECT_CALL(*mock_, ListLogs).WillOnce(Return(response));
  GoldenKitchenSinkLogging stub(mock_, TracingOptions{}, {});
  grpc::ClientContext context;
  auto status = stub.ListLogs(
      context, google::test::admin::database::v1::ListLogsRequest());
  EXPECT_STATUS_OK(status);

  auto const log_lines = log_.ExtractLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("ListLogs")));
}

TEST_F(LoggingDecoratorTest, ListLogsError) {
  EXPECT_CALL(*mock_, ListLogs).WillOnce(Return(TransientError()));
  GoldenKitchenSinkLogging stub(mock_, TracingOptions{}, {});
  grpc::ClientContext context;
  auto status = stub.ListLogs(
      context, google::test::admin::database::v1::ListLogsRequest());
  EXPECT_EQ(TransientError(), status.status());

  auto const log_lines = log_.ExtractLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("ListLogs")));
  EXPECT_THAT(log_lines, Contains(HasSubstr(TransientError().message())));
}

TEST_F(LoggingDecoratorTest, TailLogEntriesRpcNoRpcStreams) {
  auto mock_response = absl::make_unique<MockTailLogEntriesStreamingReadRpc>();
  EXPECT_CALL(*mock_response, Read).WillOnce(Return(Status()));
  EXPECT_CALL(*mock_, TailLogEntries)
      .WillOnce(Return(ByMove(
          std::unique_ptr<google::cloud::internal::StreamingReadRpc<
              google::test::admin::database::v1::TailLogEntriesResponse>>(
              mock_response.release()))));
  GoldenKitchenSinkLogging stub(mock_, TracingOptions{}, {});
  auto response = stub.TailLogEntries(
      absl::make_unique<grpc::ClientContext>(),
      google::test::admin::database::v1::TailLogEntriesRequest());
  EXPECT_THAT(absl::get<Status>(response->Read()), IsOk());

  auto const log_lines = log_.ExtractLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("TailLogEntries")));
  EXPECT_THAT(log_lines, Contains(HasSubstr("null stream")));
  EXPECT_THAT(log_lines, Not(Contains(HasSubstr("Read"))));
}

TEST_F(LoggingDecoratorTest, TailLogEntriesRpcWithRpcStreams) {
  auto mock_response = absl::make_unique<MockTailLogEntriesStreamingReadRpc>();
  EXPECT_CALL(*mock_response, Read).WillOnce(Return(Status()));
  EXPECT_CALL(*mock_, TailLogEntries)
      .WillOnce(Return(ByMove(
          std::unique_ptr<google::cloud::internal::StreamingReadRpc<
              google::test::admin::database::v1::TailLogEntriesResponse>>(
              mock_response.release()))));
  GoldenKitchenSinkLogging stub(mock_, TracingOptions{}, {"rpc-streams"});
  auto response = stub.TailLogEntries(
      absl::make_unique<grpc::ClientContext>(),
      google::test::admin::database::v1::TailLogEntriesRequest());
  EXPECT_THAT(absl::get<Status>(response->Read()), IsOk());

  auto const log_lines = log_.ExtractLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("TailLogEntries")));
  EXPECT_THAT(log_lines, Contains(HasSubstr("null stream")));
  EXPECT_THAT(log_lines, Contains(HasSubstr("Read")));
}

TEST_F(LoggingDecoratorTest, ListServiceAccountKeys) {
  ::google::test::admin::database::v1::ListServiceAccountKeysResponse response;
  EXPECT_CALL(*mock_, ListServiceAccountKeys).WillOnce(Return(response));
  GoldenKitchenSinkLogging stub(mock_, TracingOptions{}, {});
  grpc::ClientContext context;
  auto status = stub.ListServiceAccountKeys(
      context,
      google::test::admin::database::v1::ListServiceAccountKeysRequest());
  EXPECT_STATUS_OK(status);

  auto const log_lines = log_.ExtractLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("ListServiceAccountKeys")));
}

TEST_F(LoggingDecoratorTest, ListServiceAccountKeysError) {
  EXPECT_CALL(*mock_, ListServiceAccountKeys)
      .WillOnce(Return(TransientError()));
  GoldenKitchenSinkLogging stub(mock_, TracingOptions{}, {});
  grpc::ClientContext context;
  auto status = stub.ListServiceAccountKeys(
      context,
      google::test::admin::database::v1::ListServiceAccountKeysRequest());
  EXPECT_EQ(TransientError(), status.status());

  auto const log_lines = log_.ExtractLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("ListServiceAccountKeys")));
  EXPECT_THAT(log_lines, Contains(HasSubstr(TransientError().message())));
}

TEST_F(LoggingDecoratorTest, WriteObject) {
  EXPECT_CALL(*mock_, WriteObject)
      .WillOnce([](std::unique_ptr<grpc::ClientContext>) {
        auto stream = absl::make_unique<MockWriteObjectStreamingWriteRpc>();
        EXPECT_CALL(*stream, Write)
            .WillOnce(Return(true))
            .WillOnce(Return(false));
        auto response = WriteObjectResponse{};
        response.set_response("test-only");
        EXPECT_CALL(*stream, Close).WillOnce(Return(make_status_or(response)));
        return stream;
      });
  GoldenKitchenSinkLogging stub(mock_, TracingOptions{}, {});
  auto stream = stub.WriteObject(absl::make_unique<grpc::ClientContext>());
  EXPECT_TRUE(stream->Write(WriteObjectRequest{}, grpc::WriteOptions()));
  EXPECT_FALSE(stream->Write(WriteObjectRequest{}, grpc::WriteOptions()));
  auto response = stream->Close();
  ASSERT_STATUS_OK(response);
  ASSERT_STATUS_OK(response);
  EXPECT_EQ(response->response(), "test-only");

  auto const log_lines = log_.ExtractLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("WriteObject")));
  // The calls in the stream are not logged by default
  EXPECT_THAT(log_lines, Not(Contains(HasSubstr("Write("))));
  EXPECT_THAT(log_lines, Not(Contains(HasSubstr("Close("))));
}

TEST_F(LoggingDecoratorTest, WriteObjectFullTracing) {
  EXPECT_CALL(*mock_, WriteObject)
      .WillOnce([](std::unique_ptr<grpc::ClientContext>) {
        auto stream = absl::make_unique<MockWriteObjectStreamingWriteRpc>();
        EXPECT_CALL(*stream, Write)
            .WillOnce(Return(true))
            .WillOnce(Return(false));
        auto response = WriteObjectResponse{};
        response.set_response("test-only");
        EXPECT_CALL(*stream, Close).WillOnce(Return(make_status_or(response)));
        return stream;
      });
  GoldenKitchenSinkLogging stub(mock_, TracingOptions{}, {"rpc-streams"});
  auto stream = stub.WriteObject(absl::make_unique<grpc::ClientContext>());
  EXPECT_TRUE(stream->Write(WriteObjectRequest{}, grpc::WriteOptions()));
  EXPECT_FALSE(stream->Write(WriteObjectRequest{}, grpc::WriteOptions()));
  auto response = stream->Close();
  ASSERT_STATUS_OK(response);
  ASSERT_STATUS_OK(response);
  EXPECT_EQ(response->response(), "test-only");

  auto const log_lines = log_.ExtractLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("WriteObject")));
  EXPECT_THAT(log_lines, Contains(HasSubstr("Write(")));
  EXPECT_THAT(log_lines, Contains(HasSubstr("Close(")));
}

TEST_F(LoggingDecoratorTest, AsyncTailLogEntries) {
  using ErrorStream = ::google::cloud::internal::AsyncStreamingReadRpcError<
      TailLogEntriesResponse>;
  EXPECT_CALL(*mock_, AsyncTailLogEntries)
      .WillOnce(Return(ByMove(absl::make_unique<ErrorStream>(
          Status(StatusCode::kAborted, "uh-oh")))));

  GoldenKitchenSinkLogging stub(mock_, TracingOptions{}, {"rpc-streams"});

  google::cloud::CompletionQueue cq;
  ::google::test::admin::database::v1::TailLogEntriesRequest request;
  auto stream = stub.AsyncTailLogEntries(
      cq, absl::make_unique<grpc::ClientContext>(), request);

  auto start = stream->Start().get();
  EXPECT_FALSE(start);
  auto finish = stream->Finish().get();
  EXPECT_THAT(finish, StatusIs(StatusCode::kAborted));

  auto const log_lines = log_.ExtractLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("AsyncTailLogEntries")));
  EXPECT_THAT(log_lines, Contains(HasSubstr("Start(")));
  EXPECT_THAT(log_lines, Contains(HasSubstr("Finish(")));
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace golden_internal
}  // namespace cloud
}  // namespace google
