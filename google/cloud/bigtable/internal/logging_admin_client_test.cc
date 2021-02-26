// Copyright 2020 Google Inc.
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

#include "google/cloud/bigtable/internal/logging_admin_client.h"
#include "google/cloud/bigtable/admin_client.h"
#include "google/cloud/bigtable/testing/mock_admin_client.h"
#include "google/cloud/testing_util/assert_ok.h"
#include "google/cloud/testing_util/scoped_log.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace bigtable {
inline namespace BIGTABLE_CLIENT_NS {
namespace {

using ::testing::Contains;
using ::testing::HasSubstr;
using ::testing::Return;

namespace btadmin = google::bigtable::admin::v2;

class LoggingAdminClientTest : public ::testing::Test {
 protected:
  static Status TransientError() {
    return Status(StatusCode::kUnavailable, "try-again");
  }

  testing_util::ScopedLog log_;
};

TEST_F(LoggingAdminClientTest, CreateTable) {
  auto mock = std::make_shared<testing::MockAdminClient>();

  EXPECT_CALL(*mock, CreateTable).WillOnce(Return(grpc::Status()));

  internal::LoggingAdminClient stub(
      mock, TracingOptions{}.SetOptions("single_line_mode"));

  grpc::ClientContext context;
  btadmin::CreateTableRequest request;
  btadmin::Table response;

  auto status = stub.CreateTable(&context, request, &response);

  EXPECT_TRUE(status.ok());
  EXPECT_THAT(log_.ExtractLines(), Contains(HasSubstr("CreateTable")));
}

TEST_F(LoggingAdminClientTest, ListTables) {
  auto mock = std::make_shared<testing::MockAdminClient>();

  EXPECT_CALL(*mock, ListTables).WillOnce(Return(grpc::Status()));

  internal::LoggingAdminClient stub(
      mock, TracingOptions{}.SetOptions("single_line_mode"));

  grpc::ClientContext context;
  btadmin::ListTablesRequest request;
  btadmin::ListTablesResponse response;

  auto status = stub.ListTables(&context, request, &response);

  EXPECT_TRUE(status.ok());
  EXPECT_THAT(log_.ExtractLines(), Contains(HasSubstr("ListTable")));
}

TEST_F(LoggingAdminClientTest, GetTable) {
  auto mock = std::make_shared<testing::MockAdminClient>();

  EXPECT_CALL(*mock, GetTable).WillOnce(Return(grpc::Status()));

  internal::LoggingAdminClient stub(
      mock, TracingOptions{}.SetOptions("single_line_mode"));

  grpc::ClientContext context;
  btadmin::GetTableRequest request;
  btadmin::Table response;

  auto status = stub.GetTable(&context, request, &response);

  EXPECT_TRUE(status.ok());
  EXPECT_THAT(log_.ExtractLines(), Contains(HasSubstr("GetTable")));
}

TEST_F(LoggingAdminClientTest, DeleteTable) {
  auto mock = std::make_shared<testing::MockAdminClient>();

  EXPECT_CALL(*mock, DeleteTable).WillOnce(Return(grpc::Status()));

  internal::LoggingAdminClient stub(
      mock, TracingOptions{}.SetOptions("single_line_mode"));

  grpc::ClientContext context;
  btadmin::DeleteTableRequest request;
  google::protobuf::Empty response;

  auto status = stub.DeleteTable(&context, request, &response);

  EXPECT_TRUE(status.ok());
  EXPECT_THAT(log_.ExtractLines(), Contains(HasSubstr("DeleteTable")));
}

TEST_F(LoggingAdminClientTest, CreateBackup) {
  auto mock = std::make_shared<testing::MockAdminClient>();

  EXPECT_CALL(*mock, CreateBackup).WillOnce(Return(grpc::Status()));

  internal::LoggingAdminClient stub(
      mock, TracingOptions{}.SetOptions("single_line_mode"));

  grpc::ClientContext context;
  btadmin::CreateBackupRequest request;
  google::longrunning::Operation response;

  auto status = stub.CreateBackup(&context, request, &response);

  EXPECT_TRUE(status.ok());
  EXPECT_THAT(log_.ExtractLines(), Contains(HasSubstr("CreateBackup")));
}

TEST_F(LoggingAdminClientTest, GetBackup) {
  auto mock = std::make_shared<testing::MockAdminClient>();

  EXPECT_CALL(*mock, GetBackup).WillOnce(Return(grpc::Status()));

  internal::LoggingAdminClient stub(
      mock, TracingOptions{}.SetOptions("single_line_mode"));

  grpc::ClientContext context;
  btadmin::GetBackupRequest request;
  btadmin::Backup response;

  auto status = stub.GetBackup(&context, request, &response);

  EXPECT_TRUE(status.ok());
  EXPECT_THAT(log_.ExtractLines(), Contains(HasSubstr("GetBackup")));
}

TEST_F(LoggingAdminClientTest, UpdateBackup) {
  auto mock = std::make_shared<testing::MockAdminClient>();

  EXPECT_CALL(*mock, UpdateBackup).WillOnce(Return(grpc::Status()));

  internal::LoggingAdminClient stub(
      mock, TracingOptions{}.SetOptions("single_line_mode"));

  grpc::ClientContext context;
  btadmin::UpdateBackupRequest request;
  btadmin::Backup response;

  auto status = stub.UpdateBackup(&context, request, &response);

  EXPECT_TRUE(status.ok());
  EXPECT_THAT(log_.ExtractLines(), Contains(HasSubstr("UpdateBackup")));
}

TEST_F(LoggingAdminClientTest, DeleteBackup) {
  auto mock = std::make_shared<testing::MockAdminClient>();

  EXPECT_CALL(*mock, DeleteBackup).WillOnce(Return(grpc::Status()));

  internal::LoggingAdminClient stub(
      mock, TracingOptions{}.SetOptions("single_line_mode"));

  grpc::ClientContext context;
  btadmin::DeleteBackupRequest request;
  google::protobuf::Empty response;

  auto status = stub.DeleteBackup(&context, request, &response);

  EXPECT_TRUE(status.ok());
  EXPECT_THAT(log_.ExtractLines(), Contains(HasSubstr("DeleteBackup")));
}

TEST_F(LoggingAdminClientTest, ListBackups) {
  auto mock = std::make_shared<testing::MockAdminClient>();

  EXPECT_CALL(*mock, ListBackups).WillOnce(Return(grpc::Status()));

  internal::LoggingAdminClient stub(
      mock, TracingOptions{}.SetOptions("single_line_mode"));

  grpc::ClientContext context;
  btadmin::ListBackupsRequest request;
  btadmin::ListBackupsResponse response;

  auto status = stub.ListBackups(&context, request, &response);

  EXPECT_TRUE(status.ok());
  EXPECT_THAT(log_.ExtractLines(), Contains(HasSubstr("ListBackups")));
}

TEST_F(LoggingAdminClientTest, RestoreTable) {
  auto mock = std::make_shared<testing::MockAdminClient>();

  EXPECT_CALL(*mock, RestoreTable).WillOnce(Return(grpc::Status()));

  internal::LoggingAdminClient stub(
      mock, TracingOptions{}.SetOptions("single_line_mode"));

  grpc::ClientContext context;
  btadmin::RestoreTableRequest request;
  google::longrunning::Operation response;

  auto status = stub.RestoreTable(&context, request, &response);

  EXPECT_TRUE(status.ok());
  EXPECT_THAT(log_.ExtractLines(), Contains(HasSubstr("RestoreTable")));
}

TEST_F(LoggingAdminClientTest, ModifyColumnFamilies) {
  auto mock = std::make_shared<testing::MockAdminClient>();

  EXPECT_CALL(*mock, ModifyColumnFamilies).WillOnce(Return(grpc::Status()));

  internal::LoggingAdminClient stub(
      mock, TracingOptions{}.SetOptions("single_line_mode"));

  grpc::ClientContext context;
  btadmin::ModifyColumnFamiliesRequest request;
  btadmin::Table response;

  auto status = stub.ModifyColumnFamilies(&context, request, &response);

  EXPECT_TRUE(status.ok());
  EXPECT_THAT(log_.ExtractLines(), Contains(HasSubstr("ModifyColumnFamilies")));
}

TEST_F(LoggingAdminClientTest, DropRowRange) {
  auto mock = std::make_shared<testing::MockAdminClient>();

  EXPECT_CALL(*mock, DropRowRange).WillOnce(Return(grpc::Status()));

  internal::LoggingAdminClient stub(
      mock, TracingOptions{}.SetOptions("single_line_mode"));

  grpc::ClientContext context;
  btadmin::DropRowRangeRequest request;
  google::protobuf::Empty response;

  auto status = stub.DropRowRange(&context, request, &response);

  EXPECT_TRUE(status.ok());
  EXPECT_THAT(log_.ExtractLines(), Contains(HasSubstr("DropRowRange")));
}

TEST_F(LoggingAdminClientTest, GenerateConsistencyToken) {
  auto mock = std::make_shared<testing::MockAdminClient>();

  EXPECT_CALL(*mock, GenerateConsistencyToken).WillOnce(Return(grpc::Status()));

  internal::LoggingAdminClient stub(
      mock, TracingOptions{}.SetOptions("single_line_mode"));

  grpc::ClientContext context;
  btadmin::GenerateConsistencyTokenRequest request;
  btadmin::GenerateConsistencyTokenResponse response;

  auto status = stub.GenerateConsistencyToken(&context, request, &response);

  EXPECT_TRUE(status.ok());
  EXPECT_THAT(log_.ExtractLines(),
              Contains(HasSubstr("GenerateConsistencyToken")));
}

TEST_F(LoggingAdminClientTest, CheckConsistency) {
  auto mock = std::make_shared<testing::MockAdminClient>();

  EXPECT_CALL(*mock, CheckConsistency).WillOnce(Return(grpc::Status()));

  internal::LoggingAdminClient stub(
      mock, TracingOptions{}.SetOptions("single_line_mode"));

  grpc::ClientContext context;
  btadmin::CheckConsistencyRequest request;
  btadmin::CheckConsistencyResponse response;

  auto status = stub.CheckConsistency(&context, request, &response);

  EXPECT_TRUE(status.ok());
  EXPECT_THAT(log_.ExtractLines(), Contains(HasSubstr("CheckConsistency")));
}

TEST_F(LoggingAdminClientTest, GetOperation) {
  auto mock = std::make_shared<testing::MockAdminClient>();

  EXPECT_CALL(*mock, GetOperation).WillOnce(Return(grpc::Status()));

  internal::LoggingAdminClient stub(
      mock, TracingOptions{}.SetOptions("single_line_mode"));

  grpc::ClientContext context;
  google::longrunning::GetOperationRequest request;
  google::longrunning::Operation response;

  auto status = stub.GetOperation(&context, request, &response);

  EXPECT_TRUE(status.ok());
  EXPECT_THAT(log_.ExtractLines(), Contains(HasSubstr("GetOperation")));
}

TEST_F(LoggingAdminClientTest, GetIamPolicy) {
  auto mock = std::make_shared<testing::MockAdminClient>();

  EXPECT_CALL(*mock, GetIamPolicy).WillOnce(Return(grpc::Status()));

  internal::LoggingAdminClient stub(
      mock, TracingOptions{}.SetOptions("single_line_mode"));

  grpc::ClientContext context;
  google::iam::v1::GetIamPolicyRequest request;
  google::iam::v1::Policy response;

  auto status = stub.GetIamPolicy(&context, request, &response);

  EXPECT_TRUE(status.ok());
  EXPECT_THAT(log_.ExtractLines(), Contains(HasSubstr("GetIamPolicy")));
}

TEST_F(LoggingAdminClientTest, SetIamPolicy) {
  auto mock = std::make_shared<testing::MockAdminClient>();

  EXPECT_CALL(*mock, SetIamPolicy).WillOnce(Return(grpc::Status()));

  internal::LoggingAdminClient stub(
      mock, TracingOptions{}.SetOptions("single_line_mode"));

  grpc::ClientContext context;
  google::iam::v1::SetIamPolicyRequest request;
  google::iam::v1::Policy response;

  auto status = stub.SetIamPolicy(&context, request, &response);

  EXPECT_TRUE(status.ok());
  EXPECT_THAT(log_.ExtractLines(), Contains(HasSubstr("SetIamPolicy")));
}

TEST_F(LoggingAdminClientTest, TestIamPermissions) {
  auto mock = std::make_shared<testing::MockAdminClient>();

  EXPECT_CALL(*mock, TestIamPermissions).WillOnce(Return(grpc::Status()));

  internal::LoggingAdminClient stub(
      mock, TracingOptions{}.SetOptions("single_line_mode"));

  grpc::ClientContext context;
  google::iam::v1::TestIamPermissionsRequest request;
  google::iam::v1::TestIamPermissionsResponse response;

  auto status = stub.TestIamPermissions(&context, request, &response);

  EXPECT_TRUE(status.ok());
  EXPECT_THAT(log_.ExtractLines(), Contains(HasSubstr("TestIamPermissions")));
}

}  // namespace
}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable
}  // namespace cloud
}  // namespace google
