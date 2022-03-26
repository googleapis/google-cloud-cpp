// Copyright 2019 Google LLC
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

#include "google/cloud/spanner/internal/logging_spanner_stub.h"
#include "google/cloud/spanner/testing/mock_spanner_stub.h"
#include "google/cloud/spanner/tracing_options.h"
#include "google/cloud/log.h"
#include "google/cloud/testing_util/scoped_log.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>
#include <grpcpp/grpcpp.h>

namespace google {
namespace cloud {
namespace spanner_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

namespace spanner_proto = ::google::spanner;

using ::testing::Contains;
using ::testing::HasSubstr;
using ::testing::Return;

class LoggingSpannerStubTest : public ::testing::Test {
 protected:
  void SetUp() override {
    mock_ = std::make_shared<spanner_testing::MockSpannerStub>();
  }

  static Status TransientError() {
    return Status(StatusCode::kUnavailable, "try-again");
  }

  std::shared_ptr<spanner_testing::MockSpannerStub> mock_;
  testing_util::ScopedLog log_;
};

/**
 * @test Verify that the LoggingSpannerStub logs request and responses.
 *
 * We only run the success case for this member function, because that provides
 * enough coverage. For the other member functions we just test with an error
 * result, because that makes the tests easier to write.
 */
TEST_F(LoggingSpannerStubTest, CreateSessionSuccess) {
  spanner_proto::v1::Session session;
  session.set_name("test-session-name");
  EXPECT_CALL(*mock_, CreateSession).WillOnce(Return(session));

  LoggingSpannerStub stub(mock_, TracingOptions{});
  grpc::ClientContext context;
  auto status =
      stub.CreateSession(context, spanner_proto::v1::CreateSessionRequest());
  EXPECT_STATUS_OK(status);

  auto const log_lines = log_.ExtractLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("CreateSession")));
  EXPECT_THAT(log_lines, Contains(HasSubstr("test-session-name")));
}

TEST_F(LoggingSpannerStubTest, CreateSession) {
  EXPECT_CALL(*mock_, CreateSession).WillOnce(Return(TransientError()));

  LoggingSpannerStub stub(mock_, TracingOptions{});
  grpc::ClientContext context;
  auto status =
      stub.CreateSession(context, spanner_proto::v1::CreateSessionRequest());
  EXPECT_EQ(TransientError(), status.status());

  auto const log_lines = log_.ExtractLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("CreateSession")));
  EXPECT_THAT(log_lines, Contains(HasSubstr(TransientError().message())));
}

TEST_F(LoggingSpannerStubTest, BatchCreateSessions) {
  EXPECT_CALL(*mock_, BatchCreateSessions).WillOnce(Return(TransientError()));

  LoggingSpannerStub stub(mock_, TracingOptions{});
  grpc::ClientContext context;
  auto status = stub.BatchCreateSessions(
      context, spanner_proto::v1::BatchCreateSessionsRequest());
  EXPECT_EQ(TransientError(), status.status());

  auto const log_lines = log_.ExtractLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("BatchCreateSessions")));
  EXPECT_THAT(log_lines, Contains(HasSubstr(TransientError().message())));
}

TEST_F(LoggingSpannerStubTest, GetSession) {
  EXPECT_CALL(*mock_, GetSession).WillOnce(Return(TransientError()));

  LoggingSpannerStub stub(mock_, TracingOptions{});
  grpc::ClientContext context;
  auto status =
      stub.GetSession(context, spanner_proto::v1::GetSessionRequest());
  EXPECT_EQ(TransientError(), status.status());

  auto const log_lines = log_.ExtractLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("GetSession")));
  EXPECT_THAT(log_lines, Contains(HasSubstr(TransientError().message())));
}

TEST_F(LoggingSpannerStubTest, ListSessions) {
  EXPECT_CALL(*mock_, ListSessions).WillOnce(Return(TransientError()));

  LoggingSpannerStub stub(mock_, TracingOptions{});
  grpc::ClientContext context;
  auto status =
      stub.ListSessions(context, spanner_proto::v1::ListSessionsRequest());
  EXPECT_EQ(TransientError(), status.status());

  auto const log_lines = log_.ExtractLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("ListSessions")));
  EXPECT_THAT(log_lines, Contains(HasSubstr(TransientError().message())));
}

TEST_F(LoggingSpannerStubTest, DeleteSession) {
  EXPECT_CALL(*mock_, DeleteSession).WillOnce(Return(TransientError()));

  LoggingSpannerStub stub(mock_, TracingOptions{});
  grpc::ClientContext context;
  auto status =
      stub.DeleteSession(context, spanner_proto::v1::DeleteSessionRequest());
  EXPECT_EQ(TransientError(), status);
  auto const log_lines = log_.ExtractLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("DeleteSession")));
  EXPECT_THAT(log_lines, Contains(HasSubstr(TransientError().message())));
}

TEST_F(LoggingSpannerStubTest, ExecuteSql) {
  EXPECT_CALL(*mock_, ExecuteSql).WillOnce(Return(TransientError()));

  LoggingSpannerStub stub(mock_, TracingOptions{});
  grpc::ClientContext context;
  auto status =
      stub.ExecuteSql(context, spanner_proto::v1::ExecuteSqlRequest());
  EXPECT_EQ(TransientError(), status.status());
  auto const log_lines = log_.ExtractLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("ExecuteSql")));
  EXPECT_THAT(log_lines, Contains(HasSubstr(TransientError().message())));
}

TEST_F(LoggingSpannerStubTest, ExecuteStreamingSql) {
  EXPECT_CALL(*mock_, ExecuteStreamingSql)
      .WillOnce([](grpc::ClientContext&,
                   spanner_proto::v1::ExecuteSqlRequest const&) {
        return std::unique_ptr<
            grpc::ClientReaderInterface<spanner_proto::v1::PartialResultSet>>{};
      });

  LoggingSpannerStub stub(mock_, TracingOptions{});
  grpc::ClientContext context;
  auto status =
      stub.ExecuteStreamingSql(context, spanner_proto::v1::ExecuteSqlRequest());
  auto const log_lines = log_.ExtractLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("ExecuteStreamingSql")));
  EXPECT_THAT(log_lines, Contains(HasSubstr(" null stream")));
}

TEST_F(LoggingSpannerStubTest, ExecuteBatchDml) {
  EXPECT_CALL(*mock_, ExecuteBatchDml).WillOnce(Return(TransientError()));

  LoggingSpannerStub stub(mock_, TracingOptions{});
  grpc::ClientContext context;
  auto status = stub.ExecuteBatchDml(
      context, spanner_proto::v1::ExecuteBatchDmlRequest());
  EXPECT_EQ(TransientError(), status.status());
  auto const log_lines = log_.ExtractLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("ExecuteBatchDml")));
  EXPECT_THAT(log_lines, Contains(HasSubstr(TransientError().message())));
}

TEST_F(LoggingSpannerStubTest, StreamingRead) {
  EXPECT_CALL(*mock_, StreamingRead)
      .WillOnce([](grpc::ClientContext&,
                   spanner_proto::v1::ReadRequest const&) {
        return std::unique_ptr<
            grpc::ClientReaderInterface<spanner_proto::v1::PartialResultSet>>{};
      });

  LoggingSpannerStub stub(mock_, TracingOptions{});
  grpc::ClientContext context;
  auto status = stub.StreamingRead(context, spanner_proto::v1::ReadRequest());
  auto const log_lines = log_.ExtractLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("StreamingRead")));
  EXPECT_THAT(log_lines, Contains(HasSubstr("null stream")));
}

TEST_F(LoggingSpannerStubTest, BeginTransaction) {
  EXPECT_CALL(*mock_, BeginTransaction).WillOnce(Return(TransientError()));

  LoggingSpannerStub stub(mock_, TracingOptions{});
  grpc::ClientContext context;
  auto status = stub.BeginTransaction(
      context, spanner_proto::v1::BeginTransactionRequest());
  EXPECT_EQ(TransientError(), status.status());
  auto const log_lines = log_.ExtractLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("BeginTransaction")));
  EXPECT_THAT(log_lines, Contains(HasSubstr(TransientError().message())));
}

TEST_F(LoggingSpannerStubTest, Commit) {
  EXPECT_CALL(*mock_, Commit).WillOnce(Return(TransientError()));

  LoggingSpannerStub stub(mock_, TracingOptions{});
  grpc::ClientContext context;
  auto status = stub.Commit(context, spanner_proto::v1::CommitRequest());
  EXPECT_EQ(TransientError(), status.status());
  auto const log_lines = log_.ExtractLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("Commit")));
  EXPECT_THAT(log_lines, Contains(HasSubstr(TransientError().message())));
}

TEST_F(LoggingSpannerStubTest, Rollback) {
  EXPECT_CALL(*mock_, Rollback).WillOnce(Return(TransientError()));

  LoggingSpannerStub stub(mock_, TracingOptions{});
  grpc::ClientContext context;
  auto status = stub.Rollback(context, spanner_proto::v1::RollbackRequest());
  EXPECT_EQ(TransientError(), status);
  auto const log_lines = log_.ExtractLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("Rollback")));
  EXPECT_THAT(log_lines, Contains(HasSubstr(TransientError().message())));
}

TEST_F(LoggingSpannerStubTest, PartitionQuery) {
  EXPECT_CALL(*mock_, PartitionQuery).WillOnce(Return(TransientError()));

  LoggingSpannerStub stub(mock_, TracingOptions{});
  grpc::ClientContext context;
  auto status =
      stub.PartitionQuery(context, spanner_proto::v1::PartitionQueryRequest());
  EXPECT_EQ(TransientError(), status.status());
  auto const log_lines = log_.ExtractLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("PartitionQuery")));
  EXPECT_THAT(log_lines, Contains(HasSubstr(TransientError().message())));
}

TEST_F(LoggingSpannerStubTest, PartitionRead) {
  EXPECT_CALL(*mock_, PartitionRead).WillOnce(Return(TransientError()));

  LoggingSpannerStub stub(mock_, TracingOptions{});
  grpc::ClientContext context;
  auto status =
      stub.PartitionRead(context, spanner_proto::v1::PartitionReadRequest());
  EXPECT_EQ(TransientError(), status.status());
  auto const log_lines = log_.ExtractLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("PartitionRead")));
  EXPECT_THAT(log_lines, Contains(HasSubstr(TransientError().message())));
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace spanner_internal
}  // namespace cloud
}  // namespace google
