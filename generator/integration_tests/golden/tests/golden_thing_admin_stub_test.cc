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

#include "generator/integration_tests/golden/internal/golden_thing_admin_stub.h"
#include "google/cloud/testing_util/mock_async_response_reader.h"
#include "google/cloud/testing_util/mock_completion_queue_impl.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>
#include <memory>

namespace google {
namespace cloud {
namespace golden_internal {
inline namespace GOOGLE_CLOUD_CPP_GENERATED_NS {
namespace {

using ::google::cloud::internal::AsyncGrpcOperation;
using ::google::cloud::testing_util::IsOk;
using ::google::cloud::testing_util::MockAsyncResponseReader;
using ::google::cloud::testing_util::MockCompletionQueueImpl;
using ::google::cloud::testing_util::StatusIs;
using ::testing::_;
using ::testing::Return;
using ::testing::ReturnRef;

class MockGrpcGoldenThingAdminStub : public ::google::test::admin::database::
                                         v1::GoldenThingAdmin::StubInterface {
 public:
  ~MockGrpcGoldenThingAdminStub() override = default;
  MOCK_METHOD(
      ::grpc::Status, ListDatabases,
      (::grpc::ClientContext * context,
       ::google::test::admin::database::v1::ListDatabasesRequest const& request,
       ::google::test::admin::database::v1::ListDatabasesResponse* response),
      (override));
  MOCK_METHOD(::grpc::Status, CreateDatabase,
              (::grpc::ClientContext * context,
               ::google::test::admin::database::v1::CreateDatabaseRequest const&
                   request,
               ::google::longrunning::Operation* response),
              (override));
  MOCK_METHOD(
      ::grpc::Status, GetDatabase,
      (::grpc::ClientContext * context,
       ::google::test::admin::database::v1::GetDatabaseRequest const& request,
       ::google::test::admin::database::v1::Database* response),
      (override));
  MOCK_METHOD(
      ::grpc::Status, UpdateDatabaseDdl,
      (::grpc::ClientContext * context,
       ::google::test::admin::database::v1::UpdateDatabaseDdlRequest const&
           request,
       ::google::longrunning::Operation* response),
      (override));
  MOCK_METHOD(
      ::grpc::Status, DropDatabase,
      (::grpc::ClientContext * context,
       ::google::test::admin::database::v1::DropDatabaseRequest const& request,
       ::google::protobuf::Empty* response),
      (override));
  MOCK_METHOD(
      ::grpc::Status, GetDatabaseDdl,
      (::grpc::ClientContext * context,
       ::google::test::admin::database::v1::GetDatabaseDdlRequest const&
           request,
       ::google::test::admin::database::v1::GetDatabaseDdlResponse* response),
      (override));
  MOCK_METHOD(::grpc::Status, SetIamPolicy,
              (::grpc::ClientContext * context,
               ::google::iam::v1::SetIamPolicyRequest const& request,
               ::google::iam::v1::Policy* response),
              (override));
  MOCK_METHOD(::grpc::Status, GetIamPolicy,
              (::grpc::ClientContext * context,
               ::google::iam::v1::GetIamPolicyRequest const& request,
               ::google::iam::v1::Policy* response),
              (override));
  MOCK_METHOD(::grpc::Status, TestIamPermissions,
              (::grpc::ClientContext * context,
               ::google::iam::v1::TestIamPermissionsRequest const& request,
               ::google::iam::v1::TestIamPermissionsResponse* response),
              (override));
  MOCK_METHOD(
      ::grpc::Status, CreateBackup,
      (::grpc::ClientContext * context,
       ::google::test::admin::database::v1::CreateBackupRequest const& request,
       ::google::longrunning::Operation* response),
      (override));
  MOCK_METHOD(
      ::grpc::Status, GetBackup,
      (::grpc::ClientContext * context,
       ::google::test::admin::database::v1::GetBackupRequest const& request,
       ::google::test::admin::database::v1::Backup* response),
      (override));
  MOCK_METHOD(
      ::grpc::Status, UpdateBackup,
      (::grpc::ClientContext * context,
       ::google::test::admin::database::v1::UpdateBackupRequest const& request,
       ::google::test::admin::database::v1::Backup* response),
      (override));
  MOCK_METHOD(
      ::grpc::Status, DeleteBackup,
      (::grpc::ClientContext * context,
       ::google::test::admin::database::v1::DeleteBackupRequest const& request,
       ::google::protobuf::Empty* response),
      (override));
  MOCK_METHOD(
      ::grpc::Status, ListBackups,
      (::grpc::ClientContext * context,
       ::google::test::admin::database::v1::ListBackupsRequest const& request,
       ::google::test::admin::database::v1::ListBackupsResponse* response),
      (override));
  MOCK_METHOD(
      ::grpc::Status, RestoreDatabase,
      (::grpc::ClientContext * context,
       ::google::test::admin::database::v1::RestoreDatabaseRequest const&
           request,
       ::google::longrunning::Operation* response),
      (override));
  MOCK_METHOD(
      ::grpc::Status, ListDatabaseOperations,
      (::grpc::ClientContext * context,
       ::google::test::admin::database::v1::ListDatabaseOperationsRequest const&
           request,
       ::google::test::admin::database::v1::ListDatabaseOperationsResponse*
           response),
      (override));
  MOCK_METHOD(
      ::grpc::Status, ListBackupOperations,
      (::grpc::ClientContext * context,
       ::google::test::admin::database::v1::ListBackupOperationsRequest const&
           request,
       ::google::test::admin::database::v1::ListBackupOperationsResponse*
           response),
      (override));

  MOCK_METHOD(
      ::grpc::ClientAsyncResponseReaderInterface<
          ::google::test::admin::database::v1::ListDatabasesResponse>*,
      AsyncListDatabasesRaw,
      (::grpc::ClientContext * context,
       ::google::test::admin::database::v1::ListDatabasesRequest const& request,
       ::grpc::CompletionQueue* cq),
      (override));
  MOCK_METHOD(
      ::grpc::ClientAsyncResponseReaderInterface<
          ::google::test::admin::database::v1::ListDatabasesResponse>*,
      PrepareAsyncListDatabasesRaw,
      (::grpc::ClientContext * context,
       ::google::test::admin::database::v1::ListDatabasesRequest const& request,
       ::grpc::CompletionQueue* cq),
      (override));
  MOCK_METHOD(::grpc::ClientAsyncResponseReaderInterface<
                  ::google::longrunning::Operation>*,
              AsyncCreateDatabaseRaw,
              (::grpc::ClientContext * context,
               ::google::test::admin::database::v1::CreateDatabaseRequest const&
                   request,
               ::grpc::CompletionQueue* cq),
              (override));
  MOCK_METHOD(::grpc::ClientAsyncResponseReaderInterface<
                  ::google::longrunning::Operation>*,
              PrepareAsyncCreateDatabaseRaw,
              (::grpc::ClientContext * context,
               ::google::test::admin::database::v1::CreateDatabaseRequest const&
                   request,
               ::grpc::CompletionQueue* cq),
              (override));
  MOCK_METHOD(
      ::grpc::ClientAsyncResponseReaderInterface<
          ::google::test::admin::database::v1::Database>*,
      AsyncGetDatabaseRaw,
      (::grpc::ClientContext * context,
       ::google::test::admin::database::v1::GetDatabaseRequest const& request,
       ::grpc::CompletionQueue* cq),
      (override));
  MOCK_METHOD(
      ::grpc::ClientAsyncResponseReaderInterface<
          ::google::test::admin::database::v1::Database>*,
      PrepareAsyncGetDatabaseRaw,
      (::grpc::ClientContext * context,
       ::google::test::admin::database::v1::GetDatabaseRequest const& request,
       ::grpc::CompletionQueue* cq),
      (override));
  MOCK_METHOD(
      ::grpc::ClientAsyncResponseReaderInterface<
          ::google::longrunning::Operation>*,
      AsyncUpdateDatabaseDdlRaw,
      (::grpc::ClientContext * context,
       ::google::test::admin::database::v1::UpdateDatabaseDdlRequest const&
           request,
       ::grpc::CompletionQueue* cq),
      (override));
  MOCK_METHOD(
      ::grpc::ClientAsyncResponseReaderInterface<
          ::google::longrunning::Operation>*,
      PrepareAsyncUpdateDatabaseDdlRaw,
      (::grpc::ClientContext * context,
       ::google::test::admin::database::v1::UpdateDatabaseDdlRequest const&
           request,
       ::grpc::CompletionQueue* cq),
      (override));
  MOCK_METHOD(
      ::grpc::ClientAsyncResponseReaderInterface<::google::protobuf::Empty>*,
      AsyncDropDatabaseRaw,
      (::grpc::ClientContext * context,
       ::google::test::admin::database::v1::DropDatabaseRequest const& request,
       ::grpc::CompletionQueue* cq),
      (override));
  MOCK_METHOD(
      ::grpc::ClientAsyncResponseReaderInterface<::google::protobuf::Empty>*,
      PrepareAsyncDropDatabaseRaw,
      (::grpc::ClientContext * context,
       ::google::test::admin::database::v1::DropDatabaseRequest const& request,
       ::grpc::CompletionQueue* cq),
      (override));
  MOCK_METHOD(::grpc::ClientAsyncResponseReaderInterface<
                  ::google::test::admin::database::v1::GetDatabaseDdlResponse>*,
              AsyncGetDatabaseDdlRaw,
              (::grpc::ClientContext * context,
               ::google::test::admin::database::v1::GetDatabaseDdlRequest const&
                   request,
               ::grpc::CompletionQueue* cq),
              (override));
  MOCK_METHOD(::grpc::ClientAsyncResponseReaderInterface<
                  ::google::test::admin::database::v1::GetDatabaseDdlResponse>*,
              PrepareAsyncGetDatabaseDdlRaw,
              (::grpc::ClientContext * context,
               ::google::test::admin::database::v1::GetDatabaseDdlRequest const&
                   request,
               ::grpc::CompletionQueue* cq),
              (override));
  MOCK_METHOD(
      ::grpc::ClientAsyncResponseReaderInterface<::google::iam::v1::Policy>*,
      AsyncSetIamPolicyRaw,
      (::grpc::ClientContext * context,
       ::google::iam::v1::SetIamPolicyRequest const& request,
       ::grpc::CompletionQueue* cq),
      (override));
  MOCK_METHOD(
      ::grpc::ClientAsyncResponseReaderInterface<::google::iam::v1::Policy>*,
      PrepareAsyncSetIamPolicyRaw,
      (::grpc::ClientContext * context,
       ::google::iam::v1::SetIamPolicyRequest const& request,
       ::grpc::CompletionQueue* cq),
      (override));
  MOCK_METHOD(
      ::grpc::ClientAsyncResponseReaderInterface<::google::iam::v1::Policy>*,
      AsyncGetIamPolicyRaw,
      (::grpc::ClientContext * context,
       ::google::iam::v1::GetIamPolicyRequest const& request,
       ::grpc::CompletionQueue* cq),
      (override));
  MOCK_METHOD(
      ::grpc::ClientAsyncResponseReaderInterface<::google::iam::v1::Policy>*,
      PrepareAsyncGetIamPolicyRaw,
      (::grpc::ClientContext * context,
       ::google::iam::v1::GetIamPolicyRequest const& request,
       ::grpc::CompletionQueue* cq),
      (override));
  MOCK_METHOD(::grpc::ClientAsyncResponseReaderInterface<
                  ::google::iam::v1::TestIamPermissionsResponse>*,
              AsyncTestIamPermissionsRaw,
              (::grpc::ClientContext * context,
               ::google::iam::v1::TestIamPermissionsRequest const& request,
               ::grpc::CompletionQueue* cq),
              (override));
  MOCK_METHOD(::grpc::ClientAsyncResponseReaderInterface<
                  ::google::iam::v1::TestIamPermissionsResponse>*,
              PrepareAsyncTestIamPermissionsRaw,
              (::grpc::ClientContext * context,
               ::google::iam::v1::TestIamPermissionsRequest const& request,
               ::grpc::CompletionQueue* cq),
              (override));
  MOCK_METHOD(
      ::grpc::ClientAsyncResponseReaderInterface<
          ::google::longrunning::Operation>*,
      AsyncCreateBackupRaw,
      (::grpc::ClientContext * context,
       ::google::test::admin::database::v1::CreateBackupRequest const& request,
       ::grpc::CompletionQueue* cq),
      (override));
  MOCK_METHOD(
      ::grpc::ClientAsyncResponseReaderInterface<
          ::google::longrunning::Operation>*,
      PrepareAsyncCreateBackupRaw,
      (::grpc::ClientContext * context,
       ::google::test::admin::database::v1::CreateBackupRequest const& request,
       ::grpc::CompletionQueue* cq),
      (override));
  MOCK_METHOD(
      ::grpc::ClientAsyncResponseReaderInterface<
          ::google::test::admin::database::v1::Backup>*,
      AsyncGetBackupRaw,
      (::grpc::ClientContext * context,
       ::google::test::admin::database::v1::GetBackupRequest const& request,
       ::grpc::CompletionQueue* cq),
      (override));
  MOCK_METHOD(
      ::grpc::ClientAsyncResponseReaderInterface<
          ::google::test::admin::database::v1::Backup>*,
      PrepareAsyncGetBackupRaw,
      (::grpc::ClientContext * context,
       ::google::test::admin::database::v1::GetBackupRequest const& request,
       ::grpc::CompletionQueue* cq),
      (override));
  MOCK_METHOD(
      ::grpc::ClientAsyncResponseReaderInterface<
          ::google::test::admin::database::v1::Backup>*,
      AsyncUpdateBackupRaw,
      (::grpc::ClientContext * context,
       ::google::test::admin::database::v1::UpdateBackupRequest const& request,
       ::grpc::CompletionQueue* cq),
      (override));
  MOCK_METHOD(
      ::grpc::ClientAsyncResponseReaderInterface<
          ::google::test::admin::database::v1::Backup>*,
      PrepareAsyncUpdateBackupRaw,
      (::grpc::ClientContext * context,
       ::google::test::admin::database::v1::UpdateBackupRequest const& request,
       ::grpc::CompletionQueue* cq),
      (override));
  MOCK_METHOD(
      ::grpc::ClientAsyncResponseReaderInterface<::google::protobuf::Empty>*,
      AsyncDeleteBackupRaw,
      (::grpc::ClientContext * context,
       ::google::test::admin::database::v1::DeleteBackupRequest const& request,
       ::grpc::CompletionQueue* cq),
      (override));
  MOCK_METHOD(
      ::grpc::ClientAsyncResponseReaderInterface<::google::protobuf::Empty>*,
      PrepareAsyncDeleteBackupRaw,
      (::grpc::ClientContext * context,
       ::google::test::admin::database::v1::DeleteBackupRequest const& request,
       ::grpc::CompletionQueue* cq),
      (override));
  MOCK_METHOD(
      ::grpc::ClientAsyncResponseReaderInterface<
          ::google::test::admin::database::v1::ListBackupsResponse>*,
      AsyncListBackupsRaw,
      (::grpc::ClientContext * context,
       ::google::test::admin::database::v1::ListBackupsRequest const& request,
       ::grpc::CompletionQueue* cq),
      (override));
  MOCK_METHOD(
      ::grpc::ClientAsyncResponseReaderInterface<
          ::google::test::admin::database::v1::ListBackupsResponse>*,
      PrepareAsyncListBackupsRaw,
      (::grpc::ClientContext * context,
       ::google::test::admin::database::v1::ListBackupsRequest const& request,
       ::grpc::CompletionQueue* cq),
      (override));
  MOCK_METHOD(
      ::grpc::ClientAsyncResponseReaderInterface<
          ::google::longrunning::Operation>*,
      AsyncRestoreDatabaseRaw,
      (::grpc::ClientContext * context,
       ::google::test::admin::database::v1::RestoreDatabaseRequest const&
           request,
       ::grpc::CompletionQueue* cq),
      (override));
  MOCK_METHOD(
      ::grpc::ClientAsyncResponseReaderInterface<
          ::google::longrunning::Operation>*,
      PrepareAsyncRestoreDatabaseRaw,
      (::grpc::ClientContext * context,
       ::google::test::admin::database::v1::RestoreDatabaseRequest const&
           request,
       ::grpc::CompletionQueue* cq),
      (override));
  MOCK_METHOD(
      ::grpc::ClientAsyncResponseReaderInterface<
          ::google::test::admin::database::v1::ListDatabaseOperationsResponse>*,
      AsyncListDatabaseOperationsRaw,
      (::grpc::ClientContext * context,
       ::google::test::admin::database::v1::ListDatabaseOperationsRequest const&
           request,
       ::grpc::CompletionQueue* cq),
      (override));
  MOCK_METHOD(
      ::grpc::ClientAsyncResponseReaderInterface<
          ::google::test::admin::database::v1::ListDatabaseOperationsResponse>*,
      PrepareAsyncListDatabaseOperationsRaw,
      (::grpc::ClientContext * context,
       ::google::test::admin::database::v1::ListDatabaseOperationsRequest const&
           request,
       ::grpc::CompletionQueue* cq),
      (override));
  MOCK_METHOD(
      ::grpc::ClientAsyncResponseReaderInterface<
          ::google::test::admin::database::v1::ListBackupOperationsResponse>*,
      AsyncListBackupOperationsRaw,
      (::grpc::ClientContext * context,
       ::google::test::admin::database::v1::ListBackupOperationsRequest const&
           request,
       ::grpc::CompletionQueue* cq),
      (override));
  MOCK_METHOD(
      ::grpc::ClientAsyncResponseReaderInterface<
          ::google::test::admin::database::v1::ListBackupOperationsResponse>*,
      PrepareAsyncListBackupOperationsRaw,
      (::grpc::ClientContext * context,
       ::google::test::admin::database::v1::ListBackupOperationsRequest const&
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
               ::google::longrunning::ListOperationsRequest const& request,
               ::google::longrunning::ListOperationsResponse* response),
              (override));
  MOCK_METHOD(::grpc::Status, GetOperation,
              (::grpc::ClientContext * context,
               ::google::longrunning::GetOperationRequest const& request,
               ::google::longrunning::Operation* response),
              (override));
  MOCK_METHOD(::grpc::Status, DeleteOperation,
              (::grpc::ClientContext * context,
               ::google::longrunning::DeleteOperationRequest const& request,
               ::google::protobuf::Empty* response),
              (override));
  MOCK_METHOD(::grpc::Status, CancelOperation,
              (::grpc::ClientContext * context,
               ::google::longrunning::CancelOperationRequest const& request,
               ::google::protobuf::Empty* response),
              (override));
  MOCK_METHOD(::grpc::Status, WaitOperation,
              (::grpc::ClientContext * context,
               ::google::longrunning::WaitOperationRequest const& request,
               ::google::longrunning::Operation* response),
              (override));
  MOCK_METHOD(::grpc::ClientAsyncResponseReaderInterface<
                  ::google::longrunning::ListOperationsResponse>*,
              AsyncListOperationsRaw,
              (::grpc::ClientContext * context,
               ::google::longrunning::ListOperationsRequest const& request,
               ::grpc::CompletionQueue* cq),
              (override));
  MOCK_METHOD(::grpc::ClientAsyncResponseReaderInterface<
                  ::google::longrunning::ListOperationsResponse>*,
              PrepareAsyncListOperationsRaw,
              (::grpc::ClientContext * context,
               ::google::longrunning::ListOperationsRequest const& request,
               ::grpc::CompletionQueue* cq),
              (override));
  MOCK_METHOD(::grpc::ClientAsyncResponseReaderInterface<
                  ::google::longrunning::Operation>*,
              AsyncGetOperationRaw,
              (::grpc::ClientContext * context,
               ::google::longrunning::GetOperationRequest const& request,
               ::grpc::CompletionQueue* cq),
              (override));
  MOCK_METHOD(::grpc::ClientAsyncResponseReaderInterface<
                  ::google::longrunning::Operation>*,
              PrepareAsyncGetOperationRaw,
              (::grpc::ClientContext * context,
               ::google::longrunning::GetOperationRequest const& request,
               ::grpc::CompletionQueue* cq),
              (override));
  MOCK_METHOD(
      ::grpc::ClientAsyncResponseReaderInterface<::google::protobuf::Empty>*,
      AsyncDeleteOperationRaw,
      (::grpc::ClientContext * context,
       ::google::longrunning::DeleteOperationRequest const& request,
       ::grpc::CompletionQueue* cq),
      (override));
  MOCK_METHOD(
      ::grpc::ClientAsyncResponseReaderInterface<::google::protobuf::Empty>*,
      PrepareAsyncDeleteOperationRaw,
      (::grpc::ClientContext * context,
       ::google::longrunning::DeleteOperationRequest const& request,
       ::grpc::CompletionQueue* cq),
      (override));
  MOCK_METHOD(
      ::grpc::ClientAsyncResponseReaderInterface<::google::protobuf::Empty>*,
      AsyncCancelOperationRaw,
      (::grpc::ClientContext * context,
       ::google::longrunning::CancelOperationRequest const& request,
       ::grpc::CompletionQueue* cq),
      (override));
  MOCK_METHOD(
      ::grpc::ClientAsyncResponseReaderInterface<::google::protobuf::Empty>*,
      PrepareAsyncCancelOperationRaw,
      (::grpc::ClientContext * context,
       ::google::longrunning::CancelOperationRequest const& request,
       ::grpc::CompletionQueue* cq),
      (override));
  MOCK_METHOD(::grpc::ClientAsyncResponseReaderInterface<
                  ::google::longrunning::Operation>*,
              AsyncWaitOperationRaw,
              (::grpc::ClientContext * context,
               ::google::longrunning::WaitOperationRequest const& request,
               ::grpc::CompletionQueue* cq),
              (override));
  MOCK_METHOD(::grpc::ClientAsyncResponseReaderInterface<
                  ::google::longrunning::Operation>*,
              PrepareAsyncWaitOperationRaw,
              (::grpc::ClientContext * context,
               ::google::longrunning::WaitOperationRequest const& request,
               ::grpc::CompletionQueue* cq),
              (override));
};

class GoldenStubTest : public ::testing::Test {
 protected:
  void SetUp() override {
    grpc_stub_ = absl::make_unique<MockGrpcGoldenThingAdminStub>();
    longrunning_stub_ = absl::make_unique<MockLongrunningOperationsStub>();
  }

  static grpc::Status GrpcTransientError() {
    return grpc::Status(grpc::StatusCode::UNAVAILABLE, "try-again");
  }

  static Status TransientError() {
    return Status(StatusCode::kUnavailable, "try-again");
  }

  template <typename Response>
  std::unique_ptr<MockAsyncResponseReader<Response>> AsyncTransientError() {
    auto reader = absl::make_unique<MockAsyncResponseReader<Response>>();
    EXPECT_CALL(*reader, Finish)
        .WillOnce([](Response*, grpc::Status* status, void*) {
          *status = GrpcTransientError();
        });
    return reader;
  }

  std::unique_ptr<MockGrpcGoldenThingAdminStub> grpc_stub_;
  std::unique_ptr<MockLongrunningOperationsStub> longrunning_stub_;
  grpc::CompletionQueue grpc_cq_;
};

TEST_F(GoldenStubTest, ListDatabases) {
  grpc::Status status;
  grpc::ClientContext context;
  google::test::admin::database::v1::ListDatabasesRequest request;
  EXPECT_CALL(*grpc_stub_, ListDatabases(&context, _, _))
      .WillOnce(Return(status))
      .WillOnce(Return(GrpcTransientError()));

  DefaultGoldenThingAdminStub stub(std::move(grpc_stub_),
                                   std::move(longrunning_stub_));
  auto success = stub.ListDatabases(context, request);
  EXPECT_THAT(success, IsOk());
  auto failure = stub.ListDatabases(context, request);
  EXPECT_EQ(failure.status(), TransientError());
}

TEST_F(GoldenStubTest, AsyncCreateDatabase) {
  auto reader = AsyncTransientError<google::longrunning::Operation>();
  EXPECT_CALL(*grpc_stub_, AsyncCreateDatabaseRaw)
      .WillOnce(Return(reader.get()));
  auto mock = std::make_shared<MockCompletionQueueImpl>();
  EXPECT_CALL(*mock, StartOperation)
      .WillOnce([](std::shared_ptr<AsyncGrpcOperation> const& op,
                   absl::FunctionRef<void(void*)> f) {
        f(op.get());
        op->Notify(false);
      });
  EXPECT_CALL(*mock, cq).WillOnce(ReturnRef(grpc_cq_));
  CompletionQueue cq(mock);

  DefaultGoldenThingAdminStub stub(std::move(grpc_stub_),
                                   std::move(longrunning_stub_));
  google::test::admin::database::v1::CreateDatabaseRequest request;
  auto failure = stub.AsyncCreateDatabase(
      cq, absl::make_unique<grpc::ClientContext>(), request);
  EXPECT_THAT(failure.get(), StatusIs(StatusCode::kCancelled));
}

TEST_F(GoldenStubTest, GetDatabase) {
  grpc::Status status;
  grpc::ClientContext context;
  google::test::admin::database::v1::GetDatabaseRequest request;
  EXPECT_CALL(*grpc_stub_, GetDatabase(&context, _, _))
      .WillOnce(Return(status))
      .WillOnce(Return(GrpcTransientError()));
  DefaultGoldenThingAdminStub stub(std::move(grpc_stub_),
                                   std::move(longrunning_stub_));
  auto success = stub.GetDatabase(context, request);
  EXPECT_THAT(success, IsOk());
  auto failure = stub.GetDatabase(context, request);
  EXPECT_EQ(failure.status(), TransientError());
}

TEST_F(GoldenStubTest, AsyncUpdateDatabaseDdl) {
  auto reader = AsyncTransientError<google::longrunning::Operation>();
  EXPECT_CALL(*grpc_stub_, AsyncUpdateDatabaseDdlRaw)
      .WillOnce(Return(reader.get()));
  auto mock = std::make_shared<MockCompletionQueueImpl>();
  EXPECT_CALL(*mock, StartOperation)
      .WillOnce([](std::shared_ptr<AsyncGrpcOperation> const& op,
                   absl::FunctionRef<void(void*)> f) {
        f(op.get());
        op->Notify(false);
      });
  EXPECT_CALL(*mock, cq).WillOnce(::testing::ReturnRef(grpc_cq_));
  CompletionQueue cq(mock);

  DefaultGoldenThingAdminStub stub(std::move(grpc_stub_),
                                   std::move(longrunning_stub_));
  google::test::admin::database::v1::UpdateDatabaseDdlRequest request;
  auto failure = stub.AsyncUpdateDatabaseDdl(
      cq, absl::make_unique<grpc::ClientContext>(), request);
  EXPECT_THAT(failure.get(), StatusIs(StatusCode::kCancelled));
}

TEST_F(GoldenStubTest, DropDatabase) {
  grpc::Status status;
  grpc::ClientContext context;
  google::test::admin::database::v1::DropDatabaseRequest request;
  EXPECT_CALL(*grpc_stub_, DropDatabase(&context, _, _))
      .WillOnce(Return(status))
      .WillOnce(Return(GrpcTransientError()));
  DefaultGoldenThingAdminStub stub(std::move(grpc_stub_),
                                   std::move(longrunning_stub_));
  auto success = stub.DropDatabase(context, request);
  EXPECT_THAT(success, IsOk());
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
  DefaultGoldenThingAdminStub stub(std::move(grpc_stub_),
                                   std::move(longrunning_stub_));
  auto success = stub.GetDatabaseDdl(context, request);
  EXPECT_THAT(success, IsOk());
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
  DefaultGoldenThingAdminStub stub(std::move(grpc_stub_),
                                   std::move(longrunning_stub_));
  auto success = stub.SetIamPolicy(context, request);
  EXPECT_THAT(success, IsOk());
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
  DefaultGoldenThingAdminStub stub(std::move(grpc_stub_),
                                   std::move(longrunning_stub_));
  auto success = stub.GetIamPolicy(context, request);
  EXPECT_THAT(success, IsOk());
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
  DefaultGoldenThingAdminStub stub(std::move(grpc_stub_),
                                   std::move(longrunning_stub_));
  auto success = stub.TestIamPermissions(context, request);
  EXPECT_THAT(success, IsOk());
  auto failure = stub.TestIamPermissions(context, request);
  EXPECT_EQ(failure.status(), TransientError());
}

TEST_F(GoldenStubTest, AsyncCreateBackup) {
  auto reader = AsyncTransientError<google::longrunning::Operation>();
  EXPECT_CALL(*grpc_stub_, AsyncCreateBackupRaw).WillOnce(Return(reader.get()));
  auto mock = std::make_shared<MockCompletionQueueImpl>();
  EXPECT_CALL(*mock, StartOperation)
      .WillOnce([](std::shared_ptr<AsyncGrpcOperation> const& op,
                   absl::FunctionRef<void(void*)> f) {
        f(op.get());
        op->Notify(false);
      });
  EXPECT_CALL(*mock, cq).WillOnce(::testing::ReturnRef(grpc_cq_));
  CompletionQueue cq(mock);

  DefaultGoldenThingAdminStub stub(std::move(grpc_stub_),
                                   std::move(longrunning_stub_));
  google::test::admin::database::v1::CreateBackupRequest request;
  auto failure = stub.AsyncCreateBackup(
      cq, absl::make_unique<grpc::ClientContext>(), request);
  EXPECT_THAT(failure.get(), StatusIs(StatusCode::kCancelled));
}

TEST_F(GoldenStubTest, GetBackup) {
  grpc::Status status;
  grpc::ClientContext context;
  google::test::admin::database::v1::GetBackupRequest request;
  EXPECT_CALL(*grpc_stub_, GetBackup(&context, _, _))
      .WillOnce(Return(status))
      .WillOnce(Return(GrpcTransientError()));
  DefaultGoldenThingAdminStub stub(std::move(grpc_stub_),
                                   std::move(longrunning_stub_));
  auto success = stub.GetBackup(context, request);
  EXPECT_THAT(success, IsOk());
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
  DefaultGoldenThingAdminStub stub(std::move(grpc_stub_),
                                   std::move(longrunning_stub_));
  auto success = stub.UpdateBackup(context, request);
  EXPECT_THAT(success, IsOk());
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
  DefaultGoldenThingAdminStub stub(std::move(grpc_stub_),
                                   std::move(longrunning_stub_));
  auto success = stub.DeleteBackup(context, request);
  EXPECT_THAT(success, IsOk());
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
  DefaultGoldenThingAdminStub stub(std::move(grpc_stub_),
                                   std::move(longrunning_stub_));
  auto success = stub.ListBackups(context, request);
  EXPECT_THAT(success, IsOk());
  auto failure = stub.ListBackups(context, request);
  EXPECT_EQ(failure.status(), TransientError());
}

TEST_F(GoldenStubTest, AsyncRestoreDatabase) {
  auto reader = AsyncTransientError<google::longrunning::Operation>();
  EXPECT_CALL(*grpc_stub_, AsyncRestoreDatabaseRaw)
      .WillOnce(Return(reader.get()));
  auto mock = std::make_shared<MockCompletionQueueImpl>();
  EXPECT_CALL(*mock, StartOperation)
      .WillOnce([](std::shared_ptr<AsyncGrpcOperation> const& op,
                   absl::FunctionRef<void(void*)> f) {
        f(op.get());
        op->Notify(false);
      });
  EXPECT_CALL(*mock, cq).WillOnce(::testing::ReturnRef(grpc_cq_));
  CompletionQueue cq(mock);

  DefaultGoldenThingAdminStub stub(std::move(grpc_stub_),
                                   std::move(longrunning_stub_));
  google::test::admin::database::v1::RestoreDatabaseRequest request;
  auto failure = stub.AsyncRestoreDatabase(
      cq, absl::make_unique<grpc::ClientContext>(), request);
  EXPECT_THAT(failure.get(), StatusIs(StatusCode::kCancelled));
}

TEST_F(GoldenStubTest, ListDatabaseOperations) {
  grpc::Status status;
  grpc::ClientContext context;
  google::test::admin::database::v1::ListDatabaseOperationsRequest request;
  EXPECT_CALL(*grpc_stub_, ListDatabaseOperations(&context, _, _))
      .WillOnce(Return(status))
      .WillOnce(Return(GrpcTransientError()));
  DefaultGoldenThingAdminStub stub(std::move(grpc_stub_),
                                   std::move(longrunning_stub_));
  auto success = stub.ListDatabaseOperations(context, request);
  EXPECT_THAT(success, IsOk());
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
  DefaultGoldenThingAdminStub stub(std::move(grpc_stub_),
                                   std::move(longrunning_stub_));
  auto success = stub.ListBackupOperations(context, request);
  EXPECT_THAT(success, IsOk());
  auto failure = stub.ListBackupOperations(context, request);
  EXPECT_EQ(failure.status(), TransientError());
}

TEST_F(GoldenStubTest, AsyncGetOperation) {
  auto reader = AsyncTransientError<google::longrunning::Operation>();
  EXPECT_CALL(*longrunning_stub_, AsyncGetOperationRaw)
      .WillOnce(Return(reader.get()));
  auto mock = std::make_shared<MockCompletionQueueImpl>();
  EXPECT_CALL(*mock, StartOperation)
      .WillOnce([](std::shared_ptr<AsyncGrpcOperation> const& op,
                   absl::FunctionRef<void(void*)> f) {
        f(op.get());
        op->Notify(false);
      });
  EXPECT_CALL(*mock, cq).WillOnce(::testing::ReturnRef(grpc_cq_));
  CompletionQueue cq(mock);

  DefaultGoldenThingAdminStub stub(std::move(grpc_stub_),
                                   std::move(longrunning_stub_));
  google::longrunning::GetOperationRequest request;
  auto failure = stub.AsyncGetOperation(
      cq, absl::make_unique<grpc::ClientContext>(), request);
  EXPECT_THAT(failure.get(), StatusIs(StatusCode::kCancelled));
}

TEST_F(GoldenStubTest, AsyncCancelOperation) {
  auto reader = AsyncTransientError<google::protobuf::Empty>();
  EXPECT_CALL(*longrunning_stub_, AsyncCancelOperationRaw)
      .WillOnce(Return(reader.get()));
  auto mock = std::make_shared<MockCompletionQueueImpl>();
  EXPECT_CALL(*mock, StartOperation)
      .WillOnce([](std::shared_ptr<AsyncGrpcOperation> const& op,
                   absl::FunctionRef<void(void*)> f) {
        f(op.get());
        op->Notify(false);
      });
  EXPECT_CALL(*mock, cq).WillOnce(::testing::ReturnRef(grpc_cq_));
  CompletionQueue cq(mock);

  DefaultGoldenThingAdminStub stub(std::move(grpc_stub_),
                                   std::move(longrunning_stub_));
  google::longrunning::CancelOperationRequest request;
  auto failure = stub.AsyncCancelOperation(
      cq, absl::make_unique<grpc::ClientContext>(), request);
  EXPECT_THAT(failure.get(), StatusIs(StatusCode::kCancelled));
}

}  // namespace
}  // namespace GOOGLE_CLOUD_CPP_GENERATED_NS
}  // namespace golden_internal
}  // namespace cloud
}  // namespace google
