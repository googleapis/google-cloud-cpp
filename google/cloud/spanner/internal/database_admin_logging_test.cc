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

#include "google/cloud/spanner/internal/database_admin_logging.h"
#include "google/cloud/spanner/testing/mock_database_admin_stub.h"
#include "google/cloud/log.h"
#include "google/cloud/testing_util/assert_ok.h"
#include "google/cloud/testing_util/capture_log_lines_backend.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace spanner {
inline namespace SPANNER_CLIENT_NS {
namespace internal {
namespace {

using ::testing::_;
using ::testing::ByMove;
using ::testing::Return;
namespace gcsa = google::spanner::admin::database::v1;

class DatabaseAdminLoggingTest : public ::testing::Test {
 protected:
  void SetUp() override {
    backend_ =
        std::make_shared<google::cloud::testing_util::CaptureLogLinesBackend>();
    logger_id_ = google::cloud::LogSink::Instance().AddBackend(backend_);
    mock_ = std::make_shared<spanner_testing::MockDatabaseAdminStub>();
  }

  void TearDown() override {
    google::cloud::LogSink::Instance().RemoveBackend(logger_id_);
    logger_id_ = 0;
  }

  Status TransientError() {
    return Status(StatusCode::kUnavailable, "try-again");
  }

  void HasLogLineWith(std::string const& contents) {
    auto count =
        std::count_if(backend_->log_lines.begin(), backend_->log_lines.end(),
                      [&contents](std::string const& line) {
                        return std::string::npos != line.find(contents);
                      });
    EXPECT_NE(0, count) << "expected to find line with " << contents;
  }

  std::shared_ptr<spanner_testing::MockDatabaseAdminStub> mock_;

 private:
  std::shared_ptr<google::cloud::testing_util::CaptureLogLinesBackend> backend_;
  long logger_id_ = 0;  // NOLINT
};

TEST_F(DatabaseAdminLoggingTest, CreateDatabase) {
  EXPECT_CALL(*mock_, CreateDatabase(_, _)).WillOnce(Return(TransientError()));

  DatabaseAdminLogging stub(mock_);

  grpc::ClientContext context;
  auto status = stub.CreateDatabase(context, gcsa::CreateDatabaseRequest{});
  EXPECT_EQ(TransientError(), status.status());

  HasLogLineWith("CreateDatabase");
  HasLogLineWith(TransientError().message());
}

TEST_F(DatabaseAdminLoggingTest, GetDatabase) {
  EXPECT_CALL(*mock_, GetDatabase(_, _)).WillOnce(Return(TransientError()));

  DatabaseAdminLogging stub(mock_);

  grpc::ClientContext context;
  auto response = stub.GetDatabase(context, gcsa::GetDatabaseRequest{});
  EXPECT_EQ(TransientError(), response.status());

  HasLogLineWith("GetDatabase");
  HasLogLineWith(TransientError().message());
}

TEST_F(DatabaseAdminLoggingTest, GetDatabaseDdl) {
  EXPECT_CALL(*mock_, GetDatabaseDdl(_, _)).WillOnce(Return(TransientError()));

  DatabaseAdminLogging stub(mock_);

  grpc::ClientContext context;
  auto response = stub.GetDatabaseDdl(context, gcsa::GetDatabaseDdlRequest{});
  EXPECT_EQ(TransientError(), response.status());

  HasLogLineWith("GetDatabaseDdl");
  HasLogLineWith(TransientError().message());
}

TEST_F(DatabaseAdminLoggingTest, AwaitCreateDatabase) {
  auto result = google::cloud::make_ready_future(
      StatusOr<gcsa::Database>(TransientError()));
  EXPECT_CALL(*mock_, AwaitCreateDatabase(_))
      .WillOnce(Return(ByMove(std::move(result))));

  DatabaseAdminLogging stub(mock_);

  auto fut = stub.AwaitCreateDatabase(google::longrunning::Operation{});
  HasLogLineWith("AwaitCreateDatabase");
  HasLogLineWith("a ready future");
}

TEST_F(DatabaseAdminLoggingTest, AwaitCreateDatabase_Pending) {
  promise<StatusOr<gcsa::Database>> promise;
  auto result = promise.get_future();
  EXPECT_CALL(*mock_, AwaitCreateDatabase(_))
      .WillOnce(Return(ByMove(std::move(result))));

  DatabaseAdminLogging stub(mock_);

  auto fut = stub.AwaitCreateDatabase(google::longrunning::Operation{});
  HasLogLineWith("AwaitCreateDatabase");
  HasLogLineWith("an unsatisfied future");
}

TEST_F(DatabaseAdminLoggingTest, UpdateDatabase) {
  EXPECT_CALL(*mock_, UpdateDatabase(_, _)).WillOnce(Return(TransientError()));

  DatabaseAdminLogging stub(mock_);

  grpc::ClientContext context;
  auto status = stub.UpdateDatabase(context, gcsa::UpdateDatabaseDdlRequest{});
  EXPECT_EQ(TransientError(), status.status());

  HasLogLineWith("UpdateDatabase");
  HasLogLineWith(TransientError().message());
}

TEST_F(DatabaseAdminLoggingTest, AwaitUpdateDatabase) {
  auto result = google::cloud::make_ready_future(
      StatusOr<gcsa::UpdateDatabaseDdlMetadata>(TransientError()));
  EXPECT_CALL(*mock_, AwaitUpdateDatabase(_))
      .WillOnce(Return(ByMove(std::move(result))));

  DatabaseAdminLogging stub(mock_);

  auto fut = stub.AwaitUpdateDatabase(google::longrunning::Operation{});
  HasLogLineWith("AwaitUpdateDatabase");
  HasLogLineWith("a ready future");
}

TEST_F(DatabaseAdminLoggingTest, DropDatabase) {
  EXPECT_CALL(*mock_, DropDatabase(_, _)).WillOnce(Return(TransientError()));

  DatabaseAdminLogging stub(mock_);

  grpc::ClientContext context;
  auto status = stub.DropDatabase(context, gcsa::DropDatabaseRequest{});
  EXPECT_EQ(TransientError(), status);

  HasLogLineWith("DropDatabase");
  HasLogLineWith(TransientError().message());
}

TEST_F(DatabaseAdminLoggingTest, ListDatabases) {
  EXPECT_CALL(*mock_, ListDatabases(_, _)).WillOnce(Return(TransientError()));

  DatabaseAdminLogging stub(mock_);

  grpc::ClientContext context;
  auto response = stub.ListDatabases(context, gcsa::ListDatabasesRequest{});
  EXPECT_EQ(TransientError(), response.status());

  HasLogLineWith("ListDatabases");
  HasLogLineWith(TransientError().message());
}

TEST_F(DatabaseAdminLoggingTest, GetOperation) {
  EXPECT_CALL(*mock_, GetOperation(_, _)).WillOnce(Return(TransientError()));

  DatabaseAdminLogging stub(mock_);

  grpc::ClientContext context;
  auto status =
      stub.GetOperation(context, google::longrunning::GetOperationRequest{});
  EXPECT_EQ(TransientError(), status.status());

  HasLogLineWith("GetOperation");
  HasLogLineWith(TransientError().message());
}

}  // namespace
}  // namespace internal
}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner
}  // namespace cloud
}  // namespace google
