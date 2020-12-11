// Copyright 2019 Google LLC
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

#include "google/cloud/spanner/internal/logging_spanner_stub.h"
#include "google/cloud/spanner/testing/mock_spanner_stub.h"
#include "google/cloud/spanner/tracing_options.h"
#include "google/cloud/log.h"
#include "google/cloud/testing_util/assert_ok.h"
#include "google/cloud/testing_util/capture_log_lines_backend.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace spanner_internal {
inline namespace SPANNER_CLIENT_NS {
namespace {

using ::testing::_;
using ::testing::Contains;
using ::testing::HasSubstr;
using ::testing::Return;
namespace spanner_proto = ::google::spanner::v1;

class LoggingSpannerStubTest : public ::testing::Test {
 protected:
  void SetUp() override {
    backend_ =
        std::make_shared<google::cloud::testing_util::CaptureLogLinesBackend>();
    logger_id_ = google::cloud::LogSink::Instance().AddBackend(backend_);
    mock_ = std::make_shared<spanner_testing::MockSpannerStub>();
  }

  void TearDown() override {
    google::cloud::LogSink::Instance().RemoveBackend(logger_id_);
    logger_id_ = 0;
  }

  static Status TransientError() {
    return Status(StatusCode::kUnavailable, "try-again");
  }

  std::vector<std::string> ClearLogLines() { return backend_->ClearLogLines(); }

  std::shared_ptr<spanner_testing::MockSpannerStub> mock_;

 private:
  std::shared_ptr<google::cloud::testing_util::CaptureLogLinesBackend> backend_;
  long logger_id_ = 0;  // NOLINT
};

/**
 * @test Verify that the LoggingSpannerStub logs request and responses.
 *
 * We only run the success case for this member function, because that provides
 * enough coverage. For the other member functions we just test with an error
 * result, because that makes the tests easier to write.
 */
TEST_F(LoggingSpannerStubTest, CreateSessionSuccess) {
  spanner_proto::Session session;
  session.set_name("test-session-name");
  EXPECT_CALL(*mock_, CreateSession(_, _)).WillOnce(Return(session));

  LoggingSpannerStub stub(mock_, TracingOptions{});
  grpc::ClientContext context;
  auto status =
      stub.CreateSession(context, spanner_proto::CreateSessionRequest());
  EXPECT_STATUS_OK(status);

  auto const log_lines = ClearLogLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("CreateSession")));
  EXPECT_THAT(log_lines, Contains(HasSubstr("test-session-name")));
}

TEST_F(LoggingSpannerStubTest, CreateSession) {
  EXPECT_CALL(*mock_, CreateSession(_, _)).WillOnce(Return(TransientError()));

  LoggingSpannerStub stub(mock_, TracingOptions{});
  grpc::ClientContext context;
  auto status =
      stub.CreateSession(context, spanner_proto::CreateSessionRequest());
  EXPECT_EQ(TransientError(), status.status());

  auto const log_lines = ClearLogLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("CreateSession")));
  EXPECT_THAT(log_lines, Contains(HasSubstr(TransientError().message())));
}

TEST_F(LoggingSpannerStubTest, BatchCreateSessions) {
  EXPECT_CALL(*mock_, BatchCreateSessions(_, _))
      .WillOnce(Return(TransientError()));

  LoggingSpannerStub stub(mock_, TracingOptions{});
  grpc::ClientContext context;
  auto status = stub.BatchCreateSessions(
      context, spanner_proto::BatchCreateSessionsRequest());
  EXPECT_EQ(TransientError(), status.status());

  auto const log_lines = ClearLogLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("BatchCreateSessions")));
  EXPECT_THAT(log_lines, Contains(HasSubstr(TransientError().message())));
}

TEST_F(LoggingSpannerStubTest, GetSession) {
  EXPECT_CALL(*mock_, GetSession(_, _)).WillOnce(Return(TransientError()));

  LoggingSpannerStub stub(mock_, TracingOptions{});
  grpc::ClientContext context;
  auto status = stub.GetSession(context, spanner_proto::GetSessionRequest());
  EXPECT_EQ(TransientError(), status.status());

  auto const log_lines = ClearLogLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("GetSession")));
  EXPECT_THAT(log_lines, Contains(HasSubstr(TransientError().message())));
}

TEST_F(LoggingSpannerStubTest, ListSessions) {
  EXPECT_CALL(*mock_, ListSessions(_, _)).WillOnce(Return(TransientError()));

  LoggingSpannerStub stub(mock_, TracingOptions{});
  grpc::ClientContext context;
  auto status =
      stub.ListSessions(context, spanner_proto::ListSessionsRequest());
  EXPECT_EQ(TransientError(), status.status());

  auto const log_lines = ClearLogLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("ListSessions")));
  EXPECT_THAT(log_lines, Contains(HasSubstr(TransientError().message())));
}

TEST_F(LoggingSpannerStubTest, DeleteSession) {
  EXPECT_CALL(*mock_, DeleteSession(_, _)).WillOnce(Return(TransientError()));

  LoggingSpannerStub stub(mock_, TracingOptions{});
  grpc::ClientContext context;
  auto status =
      stub.DeleteSession(context, spanner_proto::DeleteSessionRequest());
  EXPECT_EQ(TransientError(), status);
  auto const log_lines = ClearLogLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("DeleteSession")));
  EXPECT_THAT(log_lines, Contains(HasSubstr(TransientError().message())));
}

TEST_F(LoggingSpannerStubTest, ExecuteSql) {
  EXPECT_CALL(*mock_, ExecuteSql(_, _)).WillOnce(Return(TransientError()));

  LoggingSpannerStub stub(mock_, TracingOptions{});
  grpc::ClientContext context;
  auto status = stub.ExecuteSql(context, spanner_proto::ExecuteSqlRequest());
  EXPECT_EQ(TransientError(), status.status());
  auto const log_lines = ClearLogLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("ExecuteSql")));
  EXPECT_THAT(log_lines, Contains(HasSubstr(TransientError().message())));
}

TEST_F(LoggingSpannerStubTest, ExecuteStreamingSql) {
  EXPECT_CALL(*mock_, ExecuteStreamingSql(_, _))
      .WillOnce(
          [](grpc::ClientContext&, spanner_proto::ExecuteSqlRequest const&) {
            return std::unique_ptr<
                grpc::ClientReaderInterface<spanner_proto::PartialResultSet>>{};
          });

  LoggingSpannerStub stub(mock_, TracingOptions{});
  grpc::ClientContext context;
  auto status =
      stub.ExecuteStreamingSql(context, spanner_proto::ExecuteSqlRequest());
  auto const log_lines = ClearLogLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("ExecuteStreamingSql")));
  EXPECT_THAT(log_lines, Contains(HasSubstr(" null stream")));
}

TEST_F(LoggingSpannerStubTest, ExecuteBatchDml) {
  EXPECT_CALL(*mock_, ExecuteBatchDml(_, _)).WillOnce(Return(TransientError()));

  LoggingSpannerStub stub(mock_, TracingOptions{});
  grpc::ClientContext context;
  auto status =
      stub.ExecuteBatchDml(context, spanner_proto::ExecuteBatchDmlRequest());
  EXPECT_EQ(TransientError(), status.status());
  auto const log_lines = ClearLogLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("ExecuteBatchDml")));
  EXPECT_THAT(log_lines, Contains(HasSubstr(TransientError().message())));
}

TEST_F(LoggingSpannerStubTest, StreamingRead) {
  EXPECT_CALL(*mock_, StreamingRead(_, _))
      .WillOnce([](grpc::ClientContext&, spanner_proto::ReadRequest const&) {
        return std::unique_ptr<
            grpc::ClientReaderInterface<spanner_proto::PartialResultSet>>{};
      });

  LoggingSpannerStub stub(mock_, TracingOptions{});
  grpc::ClientContext context;
  auto status = stub.StreamingRead(context, spanner_proto::ReadRequest());
  auto const log_lines = ClearLogLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("StreamingRead")));
  EXPECT_THAT(log_lines, Contains(HasSubstr("null stream")));
}

TEST_F(LoggingSpannerStubTest, BeginTransaction) {
  EXPECT_CALL(*mock_, BeginTransaction(_, _))
      .WillOnce(Return(TransientError()));

  LoggingSpannerStub stub(mock_, TracingOptions{});
  grpc::ClientContext context;
  auto status =
      stub.BeginTransaction(context, spanner_proto::BeginTransactionRequest());
  EXPECT_EQ(TransientError(), status.status());
  auto const log_lines = ClearLogLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("BeginTransaction")));
  EXPECT_THAT(log_lines, Contains(HasSubstr(TransientError().message())));
}

TEST_F(LoggingSpannerStubTest, Commit) {
  EXPECT_CALL(*mock_, Commit(_, _)).WillOnce(Return(TransientError()));

  LoggingSpannerStub stub(mock_, TracingOptions{});
  grpc::ClientContext context;
  auto status = stub.Commit(context, spanner_proto::CommitRequest());
  EXPECT_EQ(TransientError(), status.status());
  auto const log_lines = ClearLogLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("Commit")));
  EXPECT_THAT(log_lines, Contains(HasSubstr(TransientError().message())));
}

TEST_F(LoggingSpannerStubTest, Rollback) {
  EXPECT_CALL(*mock_, Rollback(_, _)).WillOnce(Return(TransientError()));

  LoggingSpannerStub stub(mock_, TracingOptions{});
  grpc::ClientContext context;
  auto status = stub.Rollback(context, spanner_proto::RollbackRequest());
  EXPECT_EQ(TransientError(), status);
  auto const log_lines = ClearLogLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("Rollback")));
  EXPECT_THAT(log_lines, Contains(HasSubstr(TransientError().message())));
}

TEST_F(LoggingSpannerStubTest, PartitionQuery) {
  EXPECT_CALL(*mock_, PartitionQuery(_, _)).WillOnce(Return(TransientError()));

  LoggingSpannerStub stub(mock_, TracingOptions{});
  grpc::ClientContext context;
  auto status =
      stub.PartitionQuery(context, spanner_proto::PartitionQueryRequest());
  EXPECT_EQ(TransientError(), status.status());
  auto const log_lines = ClearLogLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("PartitionQuery")));
  EXPECT_THAT(log_lines, Contains(HasSubstr(TransientError().message())));
}

TEST_F(LoggingSpannerStubTest, PartitionRead) {
  EXPECT_CALL(*mock_, PartitionRead(_, _)).WillOnce(Return(TransientError()));

  LoggingSpannerStub stub(mock_, TracingOptions{});
  grpc::ClientContext context;
  auto status =
      stub.PartitionRead(context, spanner_proto::PartitionReadRequest());
  EXPECT_EQ(TransientError(), status.status());
  auto const log_lines = ClearLogLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("PartitionRead")));
  EXPECT_THAT(log_lines, Contains(HasSubstr(TransientError().message())));
}

}  // namespace
}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner_internal
}  // namespace cloud
}  // namespace google
