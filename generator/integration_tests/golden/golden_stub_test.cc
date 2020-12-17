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

#include "generator/integration_tests/golden/internal/database_admin_stub.gcpcxx.pb.h"
#include <gmock/gmock.h>
#include <memory>

using ::testing::_;
using ::testing::Return;

namespace google {
namespace cloud {
inline namespace GOOGLE_CLOUD_CPP_NS {
namespace golden_internal {
namespace {

class MockGrpcDatabaseAdminStub
    : public ::google::test::admin::database::v1::DatabaseAdmin::StubInterface {
 public:
  ~MockGrpcDatabaseAdminStub() override = default;
  MOCK_METHOD(
      ::grpc::Status, ListDatabases,
      (::grpc::ClientContext * context,
       const ::google::test::admin::database::v1::ListDatabasesRequest& request,
       ::google::test::admin::database::v1::ListDatabasesResponse* response),
      (override));
  MOCK_METHOD(::grpc::Status, CreateDatabase,
              (::grpc::ClientContext * context,
               const ::google::test::admin::database::v1::CreateDatabaseRequest&
                   request,
               ::google::longrunning::Operation* response),
              (override));
  MOCK_METHOD(
      ::grpc::Status, GetDatabase,
      (::grpc::ClientContext * context,
       const ::google::test::admin::database::v1::GetDatabaseRequest& request,
       ::google::test::admin::database::v1::Database* response),
      (override));
  MOCK_METHOD(
      ::grpc::Status, UpdateDatabaseDdl,
      (::grpc::ClientContext * context,
       const ::google::test::admin::database::v1::UpdateDatabaseDdlRequest&
           request,
       ::google::longrunning::Operation* response),
      (override));
  MOCK_METHOD(
      ::grpc::Status, DropDatabase,
      (::grpc::ClientContext * context,
       const ::google::test::admin::database::v1::DropDatabaseRequest& request,
       ::google::protobuf::Empty* response),
      (override));
  MOCK_METHOD(
      ::grpc::Status, GetDatabaseDdl,
      (::grpc::ClientContext * context,
       const ::google::test::admin::database::v1::GetDatabaseDdlRequest&
           request,
       ::google::test::admin::database::v1::GetDatabaseDdlResponse* response),
      (override));
  MOCK_METHOD(::grpc::Status, SetIamPolicy,
              (::grpc::ClientContext * context,
               const ::google::iam::v1::SetIamPolicyRequest& request,
               ::google::iam::v1::Policy* response),
              (override));
  MOCK_METHOD(::grpc::Status, GetIamPolicy,
              (::grpc::ClientContext * context,
               const ::google::iam::v1::GetIamPolicyRequest& request,
               ::google::iam::v1::Policy* response),
              (override));
  MOCK_METHOD(::grpc::Status, TestIamPermissions,
              (::grpc::ClientContext * context,
               const ::google::iam::v1::TestIamPermissionsRequest& request,
               ::google::iam::v1::TestIamPermissionsResponse* response),
              (override));
  MOCK_METHOD(
      ::grpc::Status, CreateBackup,
      (::grpc::ClientContext * context,
       const ::google::test::admin::database::v1::CreateBackupRequest& request,
       ::google::longrunning::Operation* response),
      (override));
  MOCK_METHOD(
      ::grpc::Status, GetBackup,
      (::grpc::ClientContext * context,
       const ::google::test::admin::database::v1::GetBackupRequest& request,
       ::google::test::admin::database::v1::Backup* response),
      (override));
  MOCK_METHOD(
      ::grpc::Status, UpdateBackup,
      (::grpc::ClientContext * context,
       const ::google::test::admin::database::v1::UpdateBackupRequest& request,
       ::google::test::admin::database::v1::Backup* response),
      (override));
  MOCK_METHOD(
      ::grpc::Status, DeleteBackup,
      (::grpc::ClientContext * context,
       const ::google::test::admin::database::v1::DeleteBackupRequest& request,
       ::google::protobuf::Empty* response),
      (override));
  MOCK_METHOD(
      ::grpc::Status, ListBackups,
      (::grpc::ClientContext * context,
       const ::google::test::admin::database::v1::ListBackupsRequest& request,
       ::google::test::admin::database::v1::ListBackupsResponse* response),
      (override));
  MOCK_METHOD(
      ::grpc::Status, RestoreDatabase,
      (::grpc::ClientContext * context,
       const ::google::test::admin::database::v1::RestoreDatabaseRequest&
           request,
       ::google::longrunning::Operation* response),
      (override));
  MOCK_METHOD(
      ::grpc::Status, ListDatabaseOperations,
      (::grpc::ClientContext * context,
       const ::google::test::admin::database::v1::ListDatabaseOperationsRequest&
           request,
       ::google::test::admin::database::v1::ListDatabaseOperationsResponse*
           response),
      (override));
  MOCK_METHOD(
      ::grpc::Status, ListBackupOperations,
      (::grpc::ClientContext * context,
       const ::google::test::admin::database::v1::ListBackupOperationsRequest&
           request,
       ::google::test::admin::database::v1::ListBackupOperationsResponse*
           response),
      (override));

  MOCK_METHOD(
      ::grpc::ClientAsyncResponseReaderInterface<
          ::google::test::admin::database::v1::ListDatabasesResponse>*,
      AsyncListDatabasesRaw,
      (::grpc::ClientContext * context,
       const ::google::test::admin::database::v1::ListDatabasesRequest& request,
       ::grpc::CompletionQueue* cq),
      (override));
  MOCK_METHOD(
      ::grpc::ClientAsyncResponseReaderInterface<
          ::google::test::admin::database::v1::ListDatabasesResponse>*,
      PrepareAsyncListDatabasesRaw,
      (::grpc::ClientContext * context,
       const ::google::test::admin::database::v1::ListDatabasesRequest& request,
       ::grpc::CompletionQueue* cq),
      (override));
  MOCK_METHOD(::grpc::ClientAsyncResponseReaderInterface<
                  ::google::longrunning::Operation>*,
              AsyncCreateDatabaseRaw,
              (::grpc::ClientContext * context,
               const ::google::test::admin::database::v1::CreateDatabaseRequest&
                   request,
               ::grpc::CompletionQueue* cq),
              (override));
  MOCK_METHOD(::grpc::ClientAsyncResponseReaderInterface<
                  ::google::longrunning::Operation>*,
              PrepareAsyncCreateDatabaseRaw,
              (::grpc::ClientContext * context,
               const ::google::test::admin::database::v1::CreateDatabaseRequest&
                   request,
               ::grpc::CompletionQueue* cq),
              (override));
  MOCK_METHOD(
      ::grpc::ClientAsyncResponseReaderInterface<
          ::google::test::admin::database::v1::Database>*,
      AsyncGetDatabaseRaw,
      (::grpc::ClientContext * context,
       const ::google::test::admin::database::v1::GetDatabaseRequest& request,
       ::grpc::CompletionQueue* cq),
      (override));
  MOCK_METHOD(
      ::grpc::ClientAsyncResponseReaderInterface<
          ::google::test::admin::database::v1::Database>*,
      PrepareAsyncGetDatabaseRaw,
      (::grpc::ClientContext * context,
       const ::google::test::admin::database::v1::GetDatabaseRequest& request,
       ::grpc::CompletionQueue* cq),
      (override));
  MOCK_METHOD(
      ::grpc::ClientAsyncResponseReaderInterface<
          ::google::longrunning::Operation>*,
      AsyncUpdateDatabaseDdlRaw,
      (::grpc::ClientContext * context,
       const ::google::test::admin::database::v1::UpdateDatabaseDdlRequest&
           request,
       ::grpc::CompletionQueue* cq),
      (override));
  MOCK_METHOD(
      ::grpc::ClientAsyncResponseReaderInterface<
          ::google::longrunning::Operation>*,
      PrepareAsyncUpdateDatabaseDdlRaw,
      (::grpc::ClientContext * context,
       const ::google::test::admin::database::v1::UpdateDatabaseDdlRequest&
           request,
       ::grpc::CompletionQueue* cq),
      (override));
  MOCK_METHOD(
      ::grpc::ClientAsyncResponseReaderInterface< ::google::protobuf::Empty>*,
      AsyncDropDatabaseRaw,
      (::grpc::ClientContext * context,
       const ::google::test::admin::database::v1::DropDatabaseRequest& request,
       ::grpc::CompletionQueue* cq),
      (override));
  MOCK_METHOD(
      ::grpc::ClientAsyncResponseReaderInterface< ::google::protobuf::Empty>*,
      PrepareAsyncDropDatabaseRaw,
      (::grpc::ClientContext * context,
       const ::google::test::admin::database::v1::DropDatabaseRequest& request,
       ::grpc::CompletionQueue* cq),
      (override));
  MOCK_METHOD(::grpc::ClientAsyncResponseReaderInterface<
                  ::google::test::admin::database::v1::GetDatabaseDdlResponse>*,
              AsyncGetDatabaseDdlRaw,
              (::grpc::ClientContext * context,
               const ::google::test::admin::database::v1::GetDatabaseDdlRequest&
                   request,
               ::grpc::CompletionQueue* cq),
              (override));
  MOCK_METHOD(::grpc::ClientAsyncResponseReaderInterface<
                  ::google::test::admin::database::v1::GetDatabaseDdlResponse>*,
              PrepareAsyncGetDatabaseDdlRaw,
              (::grpc::ClientContext * context,
               const ::google::test::admin::database::v1::GetDatabaseDdlRequest&
                   request,
               ::grpc::CompletionQueue* cq),
              (override));
  MOCK_METHOD(
      ::grpc::ClientAsyncResponseReaderInterface< ::google::iam::v1::Policy>*,
      AsyncSetIamPolicyRaw,
      (::grpc::ClientContext * context,
       const ::google::iam::v1::SetIamPolicyRequest& request,
       ::grpc::CompletionQueue* cq),
      (override));
  MOCK_METHOD(
      ::grpc::ClientAsyncResponseReaderInterface< ::google::iam::v1::Policy>*,
      PrepareAsyncSetIamPolicyRaw,
      (::grpc::ClientContext * context,
       const ::google::iam::v1::SetIamPolicyRequest& request,
       ::grpc::CompletionQueue* cq),
      (override));
  MOCK_METHOD(
      ::grpc::ClientAsyncResponseReaderInterface< ::google::iam::v1::Policy>*,
      AsyncGetIamPolicyRaw,
      (::grpc::ClientContext * context,
       const ::google::iam::v1::GetIamPolicyRequest& request,
       ::grpc::CompletionQueue* cq),
      (override));
  MOCK_METHOD(
      ::grpc::ClientAsyncResponseReaderInterface< ::google::iam::v1::Policy>*,
      PrepareAsyncGetIamPolicyRaw,
      (::grpc::ClientContext * context,
       const ::google::iam::v1::GetIamPolicyRequest& request,
       ::grpc::CompletionQueue* cq),
      (override));
  MOCK_METHOD(::grpc::ClientAsyncResponseReaderInterface<
                  ::google::iam::v1::TestIamPermissionsResponse>*,
              AsyncTestIamPermissionsRaw,
              (::grpc::ClientContext * context,
               const ::google::iam::v1::TestIamPermissionsRequest& request,
               ::grpc::CompletionQueue* cq),
              (override));
  MOCK_METHOD(::grpc::ClientAsyncResponseReaderInterface<
                  ::google::iam::v1::TestIamPermissionsResponse>*,
              PrepareAsyncTestIamPermissionsRaw,
              (::grpc::ClientContext * context,
               const ::google::iam::v1::TestIamPermissionsRequest& request,
               ::grpc::CompletionQueue* cq),
              (override));
  MOCK_METHOD(
      ::grpc::ClientAsyncResponseReaderInterface<
          ::google::longrunning::Operation>*,
      AsyncCreateBackupRaw,
      (::grpc::ClientContext * context,
       const ::google::test::admin::database::v1::CreateBackupRequest& request,
       ::grpc::CompletionQueue* cq),
      (override));
  MOCK_METHOD(
      ::grpc::ClientAsyncResponseReaderInterface<
          ::google::longrunning::Operation>*,
      PrepareAsyncCreateBackupRaw,
      (::grpc::ClientContext * context,
       const ::google::test::admin::database::v1::CreateBackupRequest& request,
       ::grpc::CompletionQueue* cq),
      (override));
  MOCK_METHOD(
      ::grpc::ClientAsyncResponseReaderInterface<
          ::google::test::admin::database::v1::Backup>*,
      AsyncGetBackupRaw,
      (::grpc::ClientContext * context,
       const ::google::test::admin::database::v1::GetBackupRequest& request,
       ::grpc::CompletionQueue* cq),
      (override));
  MOCK_METHOD(
      ::grpc::ClientAsyncResponseReaderInterface<
          ::google::test::admin::database::v1::Backup>*,
      PrepareAsyncGetBackupRaw,
      (::grpc::ClientContext * context,
       const ::google::test::admin::database::v1::GetBackupRequest& request,
       ::grpc::CompletionQueue* cq),
      (override));
  MOCK_METHOD(
      ::grpc::ClientAsyncResponseReaderInterface<
          ::google::test::admin::database::v1::Backup>*,
      AsyncUpdateBackupRaw,
      (::grpc::ClientContext * context,
       const ::google::test::admin::database::v1::UpdateBackupRequest& request,
       ::grpc::CompletionQueue* cq),
      (override));
  MOCK_METHOD(
      ::grpc::ClientAsyncResponseReaderInterface<
          ::google::test::admin::database::v1::Backup>*,
      PrepareAsyncUpdateBackupRaw,
      (::grpc::ClientContext * context,
       const ::google::test::admin::database::v1::UpdateBackupRequest& request,
       ::grpc::CompletionQueue* cq),
      (override));
  MOCK_METHOD(
      ::grpc::ClientAsyncResponseReaderInterface< ::google::protobuf::Empty>*,
      AsyncDeleteBackupRaw,
      (::grpc::ClientContext * context,
       const ::google::test::admin::database::v1::DeleteBackupRequest& request,
       ::grpc::CompletionQueue* cq),
      (override));
  MOCK_METHOD(
      ::grpc::ClientAsyncResponseReaderInterface< ::google::protobuf::Empty>*,
      PrepareAsyncDeleteBackupRaw,
      (::grpc::ClientContext * context,
       const ::google::test::admin::database::v1::DeleteBackupRequest& request,
       ::grpc::CompletionQueue* cq),
      (override));
  MOCK_METHOD(
      ::grpc::ClientAsyncResponseReaderInterface<
          ::google::test::admin::database::v1::ListBackupsResponse>*,
      AsyncListBackupsRaw,
      (::grpc::ClientContext * context,
       const ::google::test::admin::database::v1::ListBackupsRequest& request,
       ::grpc::CompletionQueue* cq),
      (override));
  MOCK_METHOD(
      ::grpc::ClientAsyncResponseReaderInterface<
          ::google::test::admin::database::v1::ListBackupsResponse>*,
      PrepareAsyncListBackupsRaw,
      (::grpc::ClientContext * context,
       const ::google::test::admin::database::v1::ListBackupsRequest& request,
       ::grpc::CompletionQueue* cq),
      (override));
  MOCK_METHOD(
      ::grpc::ClientAsyncResponseReaderInterface<
          ::google::longrunning::Operation>*,
      AsyncRestoreDatabaseRaw,
      (::grpc::ClientContext * context,
       const ::google::test::admin::database::v1::RestoreDatabaseRequest&
           request,
       ::grpc::CompletionQueue* cq),
      (override));
  MOCK_METHOD(
      ::grpc::ClientAsyncResponseReaderInterface<
          ::google::longrunning::Operation>*,
      PrepareAsyncRestoreDatabaseRaw,
      (::grpc::ClientContext * context,
       const ::google::test::admin::database::v1::RestoreDatabaseRequest&
           request,
       ::grpc::CompletionQueue* cq),
      (override));
  MOCK_METHOD(
      ::grpc::ClientAsyncResponseReaderInterface<
          ::google::test::admin::database::v1::ListDatabaseOperationsResponse>*,
      AsyncListDatabaseOperationsRaw,
      (::grpc::ClientContext * context,
       const ::google::test::admin::database::v1::ListDatabaseOperationsRequest&
           request,
       ::grpc::CompletionQueue* cq),
      (override));
  MOCK_METHOD(
      ::grpc::ClientAsyncResponseReaderInterface<
          ::google::test::admin::database::v1::ListDatabaseOperationsResponse>*,
      PrepareAsyncListDatabaseOperationsRaw,
      (::grpc::ClientContext * context,
       const ::google::test::admin::database::v1::ListDatabaseOperationsRequest&
           request,
       ::grpc::CompletionQueue* cq),
      (override));
  MOCK_METHOD(
      ::grpc::ClientAsyncResponseReaderInterface<
          ::google::test::admin::database::v1::ListBackupOperationsResponse>*,
      AsyncListBackupOperationsRaw,
      (::grpc::ClientContext * context,
       const ::google::test::admin::database::v1::ListBackupOperationsRequest&
           request,
       ::grpc::CompletionQueue* cq),
      (override));
  MOCK_METHOD(
      ::grpc::ClientAsyncResponseReaderInterface<
          ::google::test::admin::database::v1::ListBackupOperationsResponse>*,
      PrepareAsyncListBackupOperationsRaw,
      (::grpc::ClientContext * context,
       const ::google::test::admin::database::v1::ListBackupOperationsRequest&
           request,
       ::grpc::CompletionQueue* cq),
      (override));
};

class MockLongrunningOperationsStub
    : public google::longrunning::Operations::StubInterface {
 public:
  ~MockLongrunningOperationsStub() override = default;
  MOCK_METHOD(::grpc::Status, ListOperations,
              (::grpc::ClientContext * context,
               const ::google::longrunning::ListOperationsRequest& request,
               ::google::longrunning::ListOperationsResponse* response),
              (override));
  MOCK_METHOD(::grpc::Status, GetOperation,
              (::grpc::ClientContext * context,
               const ::google::longrunning::GetOperationRequest& request,
               ::google::longrunning::Operation* response),
              (override));
  MOCK_METHOD(::grpc::Status, DeleteOperation,
              (::grpc::ClientContext * context,
               const ::google::longrunning::DeleteOperationRequest& request,
               ::google::protobuf::Empty* response),
              (override));
  MOCK_METHOD(::grpc::Status, CancelOperation,
              (::grpc::ClientContext * context,
               const ::google::longrunning::CancelOperationRequest& request,
               ::google::protobuf::Empty* response),
              (override));
  MOCK_METHOD(::grpc::Status, WaitOperation,
              (::grpc::ClientContext * context,
               const ::google::longrunning::WaitOperationRequest& request,
               ::google::longrunning::Operation* response),
              (override));
  MOCK_METHOD(::grpc::ClientAsyncResponseReaderInterface<
                  ::google::longrunning::ListOperationsResponse>*,
              AsyncListOperationsRaw,
              (::grpc::ClientContext * context,
               const ::google::longrunning::ListOperationsRequest& request,
               ::grpc::CompletionQueue* cq),
              (override));
  MOCK_METHOD(::grpc::ClientAsyncResponseReaderInterface<
                  ::google::longrunning::ListOperationsResponse>*,
              PrepareAsyncListOperationsRaw,
              (::grpc::ClientContext * context,
               const ::google::longrunning::ListOperationsRequest& request,
               ::grpc::CompletionQueue* cq),
              (override));
  MOCK_METHOD(::grpc::ClientAsyncResponseReaderInterface<
                  ::google::longrunning::Operation>*,
              AsyncGetOperationRaw,
              (::grpc::ClientContext * context,
               const ::google::longrunning::GetOperationRequest& request,
               ::grpc::CompletionQueue* cq),
              (override));
  MOCK_METHOD(::grpc::ClientAsyncResponseReaderInterface<
                  ::google::longrunning::Operation>*,
              PrepareAsyncGetOperationRaw,
              (::grpc::ClientContext * context,
               const ::google::longrunning::GetOperationRequest& request,
               ::grpc::CompletionQueue* cq),
              (override));
  MOCK_METHOD(
      ::grpc::ClientAsyncResponseReaderInterface< ::google::protobuf::Empty>*,
      AsyncDeleteOperationRaw,
      (::grpc::ClientContext * context,
       const ::google::longrunning::DeleteOperationRequest& request,
       ::grpc::CompletionQueue* cq),
      (override));
  MOCK_METHOD(
      ::grpc::ClientAsyncResponseReaderInterface< ::google::protobuf::Empty>*,
      PrepareAsyncDeleteOperationRaw,
      (::grpc::ClientContext * context,
       const ::google::longrunning::DeleteOperationRequest& request,
       ::grpc::CompletionQueue* cq),
      (override));
  MOCK_METHOD(
      ::grpc::ClientAsyncResponseReaderInterface< ::google::protobuf::Empty>*,
      AsyncCancelOperationRaw,
      (::grpc::ClientContext * context,
       const ::google::longrunning::CancelOperationRequest& request,
       ::grpc::CompletionQueue* cq),
      (override));
  MOCK_METHOD(
      ::grpc::ClientAsyncResponseReaderInterface< ::google::protobuf::Empty>*,
      PrepareAsyncCancelOperationRaw,
      (::grpc::ClientContext * context,
       const ::google::longrunning::CancelOperationRequest& request,
       ::grpc::CompletionQueue* cq),
      (override));
  MOCK_METHOD(::grpc::ClientAsyncResponseReaderInterface<
                  ::google::longrunning::Operation>*,
              AsyncWaitOperationRaw,
              (::grpc::ClientContext * context,
               const ::google::longrunning::WaitOperationRequest& request,
               ::grpc::CompletionQueue* cq),
              (override));
  MOCK_METHOD(::grpc::ClientAsyncResponseReaderInterface<
                  ::google::longrunning::Operation>*,
              PrepareAsyncWaitOperationRaw,
              (::grpc::ClientContext * context,
               const ::google::longrunning::WaitOperationRequest& request,
               ::grpc::CompletionQueue* cq),
              (override));
};

class GoldenStubTest : public ::testing::Test {
 protected:
  void SetUp() override {
    grpc_stub_ = std::unique_ptr<MockGrpcDatabaseAdminStub>(
        new MockGrpcDatabaseAdminStub());
    longrunning_stub_ = std::unique_ptr<MockLongrunningOperationsStub>(
        new MockLongrunningOperationsStub());
  }

  static grpc::Status GrpcTransientError() {
    return grpc::Status(grpc::StatusCode::UNAVAILABLE, "try-again");
  }

  static Status TransientError() {
    return Status(StatusCode::kUnavailable, "try-again");
  }

  std::unique_ptr<MockGrpcDatabaseAdminStub> grpc_stub_;
  std::unique_ptr<MockLongrunningOperationsStub> longrunning_stub_;
};

TEST_F(GoldenStubTest, ListDatabases) {
  grpc::Status status;
  grpc::ClientContext context;
  google::test::admin::database::v1::ListDatabasesRequest request;
  EXPECT_CALL(*grpc_stub_, ListDatabases(&context, _, _))
      .WillOnce(Return(status))
      .WillOnce(Return(GrpcTransientError()));

  DefaultDatabaseAdminStub stub(std::move(grpc_stub_),
                                std::move(longrunning_stub_));
  auto success = stub.ListDatabases(context, request);
  EXPECT_TRUE(success.ok());
  auto failure = stub.ListDatabases(context, request);
  EXPECT_EQ(failure.status(), TransientError());
}

TEST_F(GoldenStubTest, CreateDatabase) {
  grpc::Status status;
  grpc::ClientContext context;
  google::test::admin::database::v1::CreateDatabaseRequest request;
  EXPECT_CALL(*grpc_stub_, CreateDatabase(&context, _, _))
      .WillOnce(Return(status))
      .WillOnce(Return(GrpcTransientError()));
  DefaultDatabaseAdminStub stub(std::move(grpc_stub_),
                                std::move(longrunning_stub_));
  auto success = stub.CreateDatabase(context, request);
  EXPECT_TRUE(success.ok());
  auto failure = stub.CreateDatabase(context, request);
  EXPECT_EQ(failure.status(), TransientError());
}

TEST_F(GoldenStubTest, GetDatabase) {
  grpc::Status status;
  grpc::ClientContext context;
  google::test::admin::database::v1::GetDatabaseRequest request;
  EXPECT_CALL(*grpc_stub_, GetDatabase(&context, _, _))
      .WillOnce(Return(status))
      .WillOnce(Return(GrpcTransientError()));
  DefaultDatabaseAdminStub stub(std::move(grpc_stub_),
                                std::move(longrunning_stub_));
  auto success = stub.GetDatabase(context, request);
  EXPECT_TRUE(success.ok());
  auto failure = stub.GetDatabase(context, request);
  EXPECT_EQ(failure.status(), TransientError());
}

TEST_F(GoldenStubTest, UpdateDatabaseDdl) {
  grpc::Status status;
  grpc::ClientContext context;
  google::test::admin::database::v1::UpdateDatabaseDdlRequest request;
  EXPECT_CALL(*grpc_stub_, UpdateDatabaseDdl(&context, _, _))
      .WillOnce(Return(status))
      .WillOnce(Return(GrpcTransientError()));
  DefaultDatabaseAdminStub stub(std::move(grpc_stub_),
                                std::move(longrunning_stub_));
  auto success = stub.UpdateDatabaseDdl(context, request);
  EXPECT_TRUE(success.ok());
  auto failure = stub.UpdateDatabaseDdl(context, request);
  EXPECT_EQ(failure.status(), TransientError());
}

TEST_F(GoldenStubTest, DropDatabase) {
  grpc::Status status;
  grpc::ClientContext context;
  google::test::admin::database::v1::DropDatabaseRequest request;
  EXPECT_CALL(*grpc_stub_, DropDatabase(&context, _, _))
      .WillOnce(Return(status))
      .WillOnce(Return(GrpcTransientError()));
  DefaultDatabaseAdminStub stub(std::move(grpc_stub_),
                                std::move(longrunning_stub_));
  auto success = stub.DropDatabase(context, request);
  EXPECT_TRUE(success.ok());
  auto failure = stub.DropDatabase(context, request);
  EXPECT_EQ(failure, TransientError());
}

TEST_F(GoldenStubTest, GetDatabaseDdl) {
  grpc::Status status;
  grpc::ClientContext context;
  google::test::admin::database::v1::GetDatabaseDdlRequest request;
  EXPECT_CALL(*grpc_stub_, GetDatabaseDdl(&context, _, _))
      .WillOnce(Return(status))
      .WillOnce(Return(GrpcTransientError()));
  DefaultDatabaseAdminStub stub(std::move(grpc_stub_),
                                std::move(longrunning_stub_));
  auto success = stub.GetDatabaseDdl(context, request);
  EXPECT_TRUE(success.ok());
  auto failure = stub.GetDatabaseDdl(context, request);
  EXPECT_EQ(failure.status(), TransientError());
}

TEST_F(GoldenStubTest, SetIamPolicy) {
  grpc::Status status;
  grpc::ClientContext context;
  google::iam::v1::SetIamPolicyRequest request;
  EXPECT_CALL(*grpc_stub_, SetIamPolicy(&context, _, _))
      .WillOnce(Return(status))
      .WillOnce(Return(GrpcTransientError()));
  DefaultDatabaseAdminStub stub(std::move(grpc_stub_),
                                std::move(longrunning_stub_));
  auto success = stub.SetIamPolicy(context, request);
  EXPECT_TRUE(success.ok());
  auto failure = stub.SetIamPolicy(context, request);
  EXPECT_EQ(failure.status(), TransientError());
}

TEST_F(GoldenStubTest, GetIamPolicy) {
  grpc::Status status;
  grpc::ClientContext context;
  google::iam::v1::GetIamPolicyRequest request;
  EXPECT_CALL(*grpc_stub_, GetIamPolicy(&context, _, _))
      .WillOnce(Return(status))
      .WillOnce(Return(GrpcTransientError()));
  DefaultDatabaseAdminStub stub(std::move(grpc_stub_),
                                std::move(longrunning_stub_));
  auto success = stub.GetIamPolicy(context, request);
  EXPECT_TRUE(success.ok());
  auto failure = stub.GetIamPolicy(context, request);
  EXPECT_EQ(failure.status(), TransientError());
}

TEST_F(GoldenStubTest, TestIamPermissions) {
  grpc::Status status;
  grpc::ClientContext context;
  google::iam::v1::TestIamPermissionsRequest request;
  EXPECT_CALL(*grpc_stub_, TestIamPermissions(&context, _, _))
      .WillOnce(Return(status))
      .WillOnce(Return(GrpcTransientError()));
  DefaultDatabaseAdminStub stub(std::move(grpc_stub_),
                                std::move(longrunning_stub_));
  auto success = stub.TestIamPermissions(context, request);
  EXPECT_TRUE(success.ok());
  auto failure = stub.TestIamPermissions(context, request);
  EXPECT_EQ(failure.status(), TransientError());
}

TEST_F(GoldenStubTest, CreateBackup) {
  grpc::Status status;
  grpc::ClientContext context;
  google::test::admin::database::v1::CreateBackupRequest request;
  EXPECT_CALL(*grpc_stub_, CreateBackup(&context, _, _))
      .WillOnce(Return(status))
      .WillOnce(Return(GrpcTransientError()));
  DefaultDatabaseAdminStub stub(std::move(grpc_stub_),
                                std::move(longrunning_stub_));
  auto success = stub.CreateBackup(context, request);
  EXPECT_TRUE(success.ok());
  auto failure = stub.CreateBackup(context, request);
  EXPECT_EQ(failure.status(), TransientError());
}

TEST_F(GoldenStubTest, GetBackup) {
  grpc::Status status;
  grpc::ClientContext context;
  google::test::admin::database::v1::GetBackupRequest request;
  EXPECT_CALL(*grpc_stub_, GetBackup(&context, _, _))
      .WillOnce(Return(status))
      .WillOnce(Return(GrpcTransientError()));
  DefaultDatabaseAdminStub stub(std::move(grpc_stub_),
                                std::move(longrunning_stub_));
  auto success = stub.GetBackup(context, request);
  EXPECT_TRUE(success.ok());
  auto failure = stub.GetBackup(context, request);
  EXPECT_EQ(failure.status(), TransientError());
}

TEST_F(GoldenStubTest, UpdateBackup) {
  grpc::Status status;
  grpc::ClientContext context;
  google::test::admin::database::v1::UpdateBackupRequest request;
  EXPECT_CALL(*grpc_stub_, UpdateBackup(&context, _, _))
      .WillOnce(Return(status))
      .WillOnce(Return(GrpcTransientError()));
  DefaultDatabaseAdminStub stub(std::move(grpc_stub_),
                                std::move(longrunning_stub_));
  auto success = stub.UpdateBackup(context, request);
  EXPECT_TRUE(success.ok());
  auto failure = stub.UpdateBackup(context, request);
  EXPECT_EQ(failure.status(), TransientError());
}

TEST_F(GoldenStubTest, DeleteBackup) {
  grpc::Status status;
  grpc::ClientContext context;
  google::test::admin::database::v1::DeleteBackupRequest request;
  EXPECT_CALL(*grpc_stub_, DeleteBackup(&context, _, _))
      .WillOnce(Return(status))
      .WillOnce(Return(GrpcTransientError()));
  DefaultDatabaseAdminStub stub(std::move(grpc_stub_),
                                std::move(longrunning_stub_));
  auto success = stub.DeleteBackup(context, request);
  EXPECT_TRUE(success.ok());
  auto failure = stub.DeleteBackup(context, request);
  EXPECT_EQ(failure, TransientError());
}

TEST_F(GoldenStubTest, ListBackups) {
  grpc::Status status;
  grpc::ClientContext context;
  google::test::admin::database::v1::ListBackupsRequest request;
  EXPECT_CALL(*grpc_stub_, ListBackups(&context, _, _))
      .WillOnce(Return(status))
      .WillOnce(Return(GrpcTransientError()));
  DefaultDatabaseAdminStub stub(std::move(grpc_stub_),
                                std::move(longrunning_stub_));
  auto success = stub.ListBackups(context, request);
  EXPECT_TRUE(success.ok());
  auto failure = stub.ListBackups(context, request);
  EXPECT_EQ(failure.status(), TransientError());
}

TEST_F(GoldenStubTest, RestoreDatabase) {
  grpc::Status status;
  grpc::ClientContext context;
  google::test::admin::database::v1::RestoreDatabaseRequest request;
  EXPECT_CALL(*grpc_stub_, RestoreDatabase(&context, _, _))
      .WillOnce(Return(status))
      .WillOnce(Return(GrpcTransientError()));
  DefaultDatabaseAdminStub stub(std::move(grpc_stub_),
                                std::move(longrunning_stub_));
  auto success = stub.RestoreDatabase(context, request);
  EXPECT_TRUE(success.ok());
  auto failure = stub.RestoreDatabase(context, request);
  EXPECT_EQ(failure.status(), TransientError());
}

TEST_F(GoldenStubTest, ListDatabaseOperations) {
  grpc::Status status;
  grpc::ClientContext context;
  google::test::admin::database::v1::ListDatabaseOperationsRequest request;
  EXPECT_CALL(*grpc_stub_, ListDatabaseOperations(&context, _, _))
      .WillOnce(Return(status))
      .WillOnce(Return(GrpcTransientError()));
  DefaultDatabaseAdminStub stub(std::move(grpc_stub_),
                                std::move(longrunning_stub_));
  auto success = stub.ListDatabaseOperations(context, request);
  EXPECT_TRUE(success.ok());
  auto failure = stub.ListDatabaseOperations(context, request);
  EXPECT_EQ(failure.status(), TransientError());
}

TEST_F(GoldenStubTest, ListBackupOperations) {
  grpc::Status status;
  grpc::ClientContext context;
  google::test::admin::database::v1::ListBackupOperationsRequest request;
  EXPECT_CALL(*grpc_stub_, ListBackupOperations(&context, _, _))
      .WillOnce(Return(status))
      .WillOnce(Return(GrpcTransientError()));
  DefaultDatabaseAdminStub stub(std::move(grpc_stub_),
                                std::move(longrunning_stub_));
  auto success = stub.ListBackupOperations(context, request);
  EXPECT_TRUE(success.ok());
  auto failure = stub.ListBackupOperations(context, request);
  EXPECT_EQ(failure.status(), TransientError());
}

TEST_F(GoldenStubTest, GetOperation) {
  grpc::Status status;
  grpc::ClientContext context;
  google::longrunning::GetOperationRequest request;
  EXPECT_CALL(*longrunning_stub_, GetOperation(&context, _, _))
      .WillOnce(Return(status))
      .WillOnce(Return(GrpcTransientError()));
  DefaultDatabaseAdminStub stub(std::move(grpc_stub_),
                                std::move(longrunning_stub_));
  auto success = stub.GetOperation(context, request);
  EXPECT_TRUE(success.ok());
  auto failure = stub.GetOperation(context, request);
  EXPECT_EQ(failure.status(), TransientError());
}

TEST_F(GoldenStubTest, CancelOperation) {
  grpc::Status status;
  grpc::ClientContext context;
  google::longrunning::CancelOperationRequest request;
  EXPECT_CALL(*longrunning_stub_, CancelOperation(&context, _, _))
      .WillOnce(Return(status))
      .WillOnce(Return(GrpcTransientError()));
  DefaultDatabaseAdminStub stub(std::move(grpc_stub_),
                                std::move(longrunning_stub_));
  auto success = stub.CancelOperation(context, request);
  EXPECT_TRUE(success.ok());
  auto failure = stub.CancelOperation(context, request);
  EXPECT_EQ(failure, TransientError());
}

}  // namespace
}  // namespace golden_internal
}  // namespace GOOGLE_CLOUD_CPP_NS
}  // namespace cloud
}  // namespace google
