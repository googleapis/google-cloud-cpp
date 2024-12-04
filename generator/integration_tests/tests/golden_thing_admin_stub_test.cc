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

#include "generator/integration_tests/golden/v1/internal/golden_thing_admin_stub.h"
#include "google/cloud/testing_util/mock_async_response_reader.h"
#include "google/cloud/testing_util/mock_completion_queue_impl.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>
#include <memory>

namespace google {
namespace cloud {
namespace golden_v1_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::google::cloud::internal::AsyncGrpcOperation;
using ::google::cloud::testing_util::IsOk;
using ::google::cloud::testing_util::MockAsyncResponseReader;
using ::google::cloud::testing_util::MockCompletionQueueImpl;
using ::google::cloud::testing_util::StatusIs;
using ::testing::_;
using ::testing::Return;

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

  MOCK_METHOD(
      ::grpc::Status, LongRunningWithoutRouting,
      (::grpc::ClientContext * context,
       ::google::test::admin::database::v1::RestoreDatabaseRequest const&
           request,
       ::google::longrunning::Operation* response),
      (override));
  MOCK_METHOD(
      ::grpc::ClientAsyncResponseReaderInterface<
          ::google::longrunning::Operation>*,
      AsyncLongRunningWithoutRoutingRaw,
      (::grpc::ClientContext * context,
       ::google::test::admin::database::v1::RestoreDatabaseRequest const&
           request,
       ::grpc::CompletionQueue* cq),
      (override));
  MOCK_METHOD(
      ::grpc::ClientAsyncResponseReaderInterface<
          ::google::longrunning::Operation>*,
      PrepareAsyncLongRunningWithoutRoutingRaw,
      (::grpc::ClientContext * context,
       ::google::test::admin::database::v1::RestoreDatabaseRequest const&
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

class MockLocationsStub
    : public google::cloud::location::Locations::StubInterface {
 public:
  ~MockLocationsStub() override = default;
  MOCK_METHOD(::grpc::Status, ListLocations,
              (::grpc::ClientContext * context,
               ::google::cloud::location::ListLocationsRequest const& request,
               ::google::cloud::location::ListLocationsResponse* response),
              (override));

  MOCK_METHOD(::grpc::Status, GetLocation,
              (::grpc::ClientContext * context,
               ::google::cloud::location::GetLocationRequest const& request,
               ::google::cloud::location::Location* response),
              (override));

  MOCK_METHOD(::grpc::ClientAsyncResponseReaderInterface<
                  ::google::cloud::location::ListLocationsResponse>*,
              AsyncListLocationsRaw,
              (::grpc::ClientContext * context,
               ::google::cloud::location::ListLocationsRequest const& request,
               ::grpc::CompletionQueue* cq),
              (override));

  MOCK_METHOD(::grpc::ClientAsyncResponseReaderInterface<
                  ::google::cloud::location::ListLocationsResponse>*,
              PrepareAsyncListLocationsRaw,
              (::grpc::ClientContext * context,
               ::google::cloud::location::ListLocationsRequest const& request,
               ::grpc::CompletionQueue* cq),
              (override));

  MOCK_METHOD(::grpc::ClientAsyncResponseReaderInterface<
                  ::google::cloud::location::Location>*,
              AsyncGetLocationRaw,
              (::grpc::ClientContext * context,
               ::google::cloud::location::GetLocationRequest const& request,
               ::grpc::CompletionQueue* cq),
              (override));

  MOCK_METHOD(::grpc::ClientAsyncResponseReaderInterface<
                  ::google::cloud::location::Location>*,
              PrepareAsyncGetLocationRaw,
              (::grpc::ClientContext * context,
               ::google::cloud::location::GetLocationRequest const& request,
               ::grpc::CompletionQueue* cq),
              (override));
};

class GoldenStubTest : public ::testing::Test {
 protected:
  void SetUp() override {
    grpc_stub_ = std::make_unique<MockGrpcGoldenThingAdminStub>();
    longrunning_stub_ = std::make_unique<MockLongrunningOperationsStub>();
    location_stub_ = std::make_unique<MockLocationsStub>();
  }

  static grpc::Status GrpcTransientError() {
    return grpc::Status(grpc::StatusCode::UNAVAILABLE, "try-again");
  }

  static Status TransientError() {
    return Status(StatusCode::kUnavailable, "try-again");
  }

  template <typename Response>
  std::unique_ptr<MockAsyncResponseReader<Response>> AsyncTransientError() {
    auto reader = std::make_unique<MockAsyncResponseReader<Response>>();
    EXPECT_CALL(*reader, Finish)
        .WillOnce([](Response*, grpc::Status* status, void*) {
          *status = GrpcTransientError();
        });
    return reader;
  }

  std::unique_ptr<MockGrpcGoldenThingAdminStub> grpc_stub_;
  std::unique_ptr<MockLongrunningOperationsStub> longrunning_stub_;
  std::unique_ptr<MockLocationsStub> location_stub_;
};

TEST_F(GoldenStubTest, GetLocation) {
  grpc::Status status;
  grpc::ClientContext context;
  google::cloud::location::GetLocationRequest request;
  EXPECT_CALL(*location_stub_, GetLocation(&context, _, _))
      .WillOnce(Return(status))
      .WillOnce(Return(GrpcTransientError()));

  DefaultGoldenThingAdminStub stub(std::move(grpc_stub_),
                                   std::move(location_stub_),
                                   std::move(longrunning_stub_));
  auto success = stub.GetLocation(context, Options{}, request);
  EXPECT_THAT(success, IsOk());
  auto failure = stub.GetLocation(context, Options{}, request);
  EXPECT_EQ(failure.status(), TransientError());
}

TEST_F(GoldenStubTest, ListOperations) {
  grpc::Status status;
  grpc::ClientContext context;
  google::longrunning::ListOperationsRequest request;
  EXPECT_CALL(*longrunning_stub_, ListOperations(&context, _, _))
      .WillOnce(Return(status))
      .WillOnce(Return(GrpcTransientError()));

  DefaultGoldenThingAdminStub stub(std::move(grpc_stub_),
                                   std::move(location_stub_),
                                   std::move(longrunning_stub_));
  auto success = stub.ListOperations(context, Options{}, request);
  EXPECT_THAT(success, IsOk());
  auto failure = stub.ListOperations(context, Options{}, request);
  EXPECT_EQ(failure.status(), TransientError());
}

TEST_F(GoldenStubTest, ListDatabases) {
  grpc::Status status;
  grpc::ClientContext context;
  google::test::admin::database::v1::ListDatabasesRequest request;
  EXPECT_CALL(*grpc_stub_, ListDatabases(&context, _, _))
      .WillOnce(Return(status))
      .WillOnce(Return(GrpcTransientError()));

  DefaultGoldenThingAdminStub stub(std::move(grpc_stub_),
                                   std::move(location_stub_),
                                   std::move(longrunning_stub_));
  auto success = stub.ListDatabases(context, Options{}, request);
  EXPECT_THAT(success, IsOk());
  auto failure = stub.ListDatabases(context, Options{}, request);
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
  EXPECT_CALL(*mock, cq).WillOnce(Return(nullptr));
  CompletionQueue cq(mock);

  DefaultGoldenThingAdminStub stub(std::move(grpc_stub_),
                                   std::move(location_stub_),
                                   std::move(longrunning_stub_));
  google::test::admin::database::v1::CreateDatabaseRequest request;
  auto failure =
      stub.AsyncCreateDatabase(cq, std::make_shared<grpc::ClientContext>(),
                               internal::MakeImmutableOptions({}), request);
  EXPECT_THAT(failure.get(), StatusIs(StatusCode::kCancelled));
}

TEST_F(GoldenStubTest, SynchronousCreateDatabase) {
  grpc::Status status;
  grpc::ClientContext context;
  google::test::admin::database::v1::CreateDatabaseRequest request;
  EXPECT_CALL(*grpc_stub_, CreateDatabase(&context, _, _))
      .WillOnce(Return(status))
      .WillOnce(Return(GrpcTransientError()));
  DefaultGoldenThingAdminStub stub(std::move(grpc_stub_),
                                   std::move(location_stub_),
                                   std::move(longrunning_stub_));
  auto success = stub.CreateDatabase(context, Options{}, request);
  EXPECT_THAT(success, IsOk());
  auto failure = stub.CreateDatabase(context, Options{}, request);
  EXPECT_EQ(failure.status(), TransientError());
}

TEST_F(GoldenStubTest, GetDatabase) {
  grpc::Status status;
  grpc::ClientContext context;
  google::test::admin::database::v1::GetDatabaseRequest request;
  EXPECT_CALL(*grpc_stub_, GetDatabase(&context, _, _))
      .WillOnce(Return(status))
      .WillOnce(Return(GrpcTransientError()));
  DefaultGoldenThingAdminStub stub(std::move(grpc_stub_),
                                   std::move(location_stub_),
                                   std::move(longrunning_stub_));
  auto success = stub.GetDatabase(context, Options{}, request);
  EXPECT_THAT(success, IsOk());
  auto failure = stub.GetDatabase(context, Options{}, request);
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
  EXPECT_CALL(*mock, cq).WillOnce(::testing::Return(nullptr));
  CompletionQueue cq(mock);

  DefaultGoldenThingAdminStub stub(std::move(grpc_stub_),
                                   std::move(location_stub_),
                                   std::move(longrunning_stub_));
  google::test::admin::database::v1::UpdateDatabaseDdlRequest request;
  auto failure =
      stub.AsyncUpdateDatabaseDdl(cq, std::make_shared<grpc::ClientContext>(),
                                  internal::MakeImmutableOptions({}), request);
  EXPECT_THAT(failure.get(), StatusIs(StatusCode::kCancelled));
}

TEST_F(GoldenStubTest, SynchronousUpdateDatabaseDdl) {
  grpc::Status status;
  grpc::ClientContext context;
  google::test::admin::database::v1::UpdateDatabaseDdlRequest request;
  EXPECT_CALL(*grpc_stub_, UpdateDatabaseDdl(&context, _, _))
      .WillOnce(Return(status))
      .WillOnce(Return(GrpcTransientError()));
  DefaultGoldenThingAdminStub stub(std::move(grpc_stub_),
                                   std::move(location_stub_),
                                   std::move(longrunning_stub_));
  auto success = stub.UpdateDatabaseDdl(context, Options{}, request);
  EXPECT_THAT(success, IsOk());
  auto failure = stub.UpdateDatabaseDdl(context, Options{}, request);
  EXPECT_EQ(failure.status(), TransientError());
}

TEST_F(GoldenStubTest, DropDatabase) {
  grpc::Status status;
  grpc::ClientContext context;
  google::test::admin::database::v1::DropDatabaseRequest request;
  EXPECT_CALL(*grpc_stub_, DropDatabase(&context, _, _))
      .WillOnce(Return(status))
      .WillOnce(Return(GrpcTransientError()));
  DefaultGoldenThingAdminStub stub(std::move(grpc_stub_),
                                   std::move(location_stub_),
                                   std::move(longrunning_stub_));
  auto success = stub.DropDatabase(context, Options{}, request);
  EXPECT_THAT(success, IsOk());
  auto failure = stub.DropDatabase(context, Options{}, request);
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
                                   std::move(location_stub_),
                                   std::move(longrunning_stub_));
  auto success = stub.GetDatabaseDdl(context, Options{}, request);
  EXPECT_THAT(success, IsOk());
  auto failure = stub.GetDatabaseDdl(context, Options{}, request);
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
                                   std::move(location_stub_),
                                   std::move(longrunning_stub_));
  auto success = stub.SetIamPolicy(context, Options{}, request);
  EXPECT_THAT(success, IsOk());
  auto failure = stub.SetIamPolicy(context, Options{}, request);
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
                                   std::move(location_stub_),
                                   std::move(longrunning_stub_));
  auto success = stub.GetIamPolicy(context, Options{}, request);
  EXPECT_THAT(success, IsOk());
  auto failure = stub.GetIamPolicy(context, Options{}, request);
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
                                   std::move(location_stub_),
                                   std::move(longrunning_stub_));
  auto success = stub.TestIamPermissions(context, Options{}, request);
  EXPECT_THAT(success, IsOk());
  auto failure = stub.TestIamPermissions(context, Options{}, request);
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
  EXPECT_CALL(*mock, cq).WillOnce(::testing::Return(nullptr));
  CompletionQueue cq(mock);

  DefaultGoldenThingAdminStub stub(std::move(grpc_stub_),
                                   std::move(location_stub_),
                                   std::move(longrunning_stub_));
  google::test::admin::database::v1::CreateBackupRequest request;
  auto failure =
      stub.AsyncCreateBackup(cq, std::make_shared<grpc::ClientContext>(),
                             internal::MakeImmutableOptions({}), request);
  EXPECT_THAT(failure.get(), StatusIs(StatusCode::kCancelled));
}

TEST_F(GoldenStubTest, SynchronousCreateBackup) {
  grpc::Status status;
  grpc::ClientContext context;
  google::test::admin::database::v1::CreateBackupRequest request;
  EXPECT_CALL(*grpc_stub_, CreateBackup(&context, _, _))
      .WillOnce(Return(status))
      .WillOnce(Return(GrpcTransientError()));
  DefaultGoldenThingAdminStub stub(std::move(grpc_stub_),
                                   std::move(location_stub_),
                                   std::move(longrunning_stub_));
  auto success = stub.CreateBackup(context, Options{}, request);
  EXPECT_THAT(success, IsOk());
  auto failure = stub.CreateBackup(context, Options{}, request);
  EXPECT_EQ(failure.status(), TransientError());
}

TEST_F(GoldenStubTest, GetBackup) {
  grpc::Status status;
  grpc::ClientContext context;
  google::test::admin::database::v1::GetBackupRequest request;
  EXPECT_CALL(*grpc_stub_, GetBackup(&context, _, _))
      .WillOnce(Return(status))
      .WillOnce(Return(GrpcTransientError()));
  DefaultGoldenThingAdminStub stub(std::move(grpc_stub_),
                                   std::move(location_stub_),
                                   std::move(longrunning_stub_));
  auto success = stub.GetBackup(context, Options{}, request);
  EXPECT_THAT(success, IsOk());
  auto failure = stub.GetBackup(context, Options{}, request);
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
                                   std::move(location_stub_),
                                   std::move(longrunning_stub_));
  auto success = stub.UpdateBackup(context, Options{}, request);
  EXPECT_THAT(success, IsOk());
  auto failure = stub.UpdateBackup(context, Options{}, request);
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
                                   std::move(location_stub_),
                                   std::move(longrunning_stub_));
  auto success = stub.DeleteBackup(context, Options{}, request);
  EXPECT_THAT(success, IsOk());
  auto failure = stub.DeleteBackup(context, Options{}, request);
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
                                   std::move(location_stub_),
                                   std::move(longrunning_stub_));
  auto success = stub.ListBackups(context, Options{}, request);
  EXPECT_THAT(success, IsOk());
  auto failure = stub.ListBackups(context, Options{}, request);
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
  EXPECT_CALL(*mock, cq).WillOnce(::testing::Return(nullptr));
  CompletionQueue cq(mock);

  DefaultGoldenThingAdminStub stub(std::move(grpc_stub_),
                                   std::move(location_stub_),
                                   std::move(longrunning_stub_));
  google::test::admin::database::v1::RestoreDatabaseRequest request;
  auto failure =
      stub.AsyncRestoreDatabase(cq, std::make_shared<grpc::ClientContext>(),
                                internal::MakeImmutableOptions({}), request);
  EXPECT_THAT(failure.get(), StatusIs(StatusCode::kCancelled));
}

TEST_F(GoldenStubTest, SynchronousRestoreDatabase) {
  grpc::Status status;
  grpc::ClientContext context;
  google::test::admin::database::v1::RestoreDatabaseRequest request;
  EXPECT_CALL(*grpc_stub_, RestoreDatabase(&context, _, _))
      .WillOnce(Return(status))
      .WillOnce(Return(GrpcTransientError()));
  DefaultGoldenThingAdminStub stub(std::move(grpc_stub_),
                                   std::move(location_stub_),
                                   std::move(longrunning_stub_));
  auto success = stub.RestoreDatabase(context, Options{}, request);
  EXPECT_THAT(success, IsOk());
  auto failure = stub.RestoreDatabase(context, Options{}, request);
  EXPECT_EQ(failure.status(), TransientError());
}

TEST_F(GoldenStubTest, ListDatabaseOperations) {
  grpc::Status status;
  grpc::ClientContext context;
  google::test::admin::database::v1::ListDatabaseOperationsRequest request;
  EXPECT_CALL(*grpc_stub_, ListDatabaseOperations(&context, _, _))
      .WillOnce(Return(status))
      .WillOnce(Return(GrpcTransientError()));
  DefaultGoldenThingAdminStub stub(std::move(grpc_stub_),
                                   std::move(location_stub_),
                                   std::move(longrunning_stub_));
  auto success = stub.ListDatabaseOperations(context, Options{}, request);
  EXPECT_THAT(success, IsOk());
  auto failure = stub.ListDatabaseOperations(context, Options{}, request);
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
                                   std::move(location_stub_),
                                   std::move(longrunning_stub_));
  auto success = stub.ListBackupOperations(context, Options{}, request);
  EXPECT_THAT(success, IsOk());
  auto failure = stub.ListBackupOperations(context, Options{}, request);
  EXPECT_EQ(failure.status(), TransientError());
}

TEST_F(GoldenStubTest, AsyncGetDatabase) {
  auto reader =
      AsyncTransientError<google::test::admin::database::v1::Database>();
  EXPECT_CALL(*grpc_stub_, AsyncGetDatabaseRaw).WillOnce(Return(reader.get()));
  auto mock = std::make_shared<MockCompletionQueueImpl>();
  EXPECT_CALL(*mock, StartOperation)
      .WillOnce([](std::shared_ptr<AsyncGrpcOperation> const& op,
                   absl::FunctionRef<void(void*)> f) {
        f(op.get());
        op->Notify(false);
      });
  EXPECT_CALL(*mock, cq).WillOnce(::testing::Return(nullptr));
  CompletionQueue cq(mock);

  DefaultGoldenThingAdminStub stub(std::move(grpc_stub_),
                                   std::move(location_stub_),
                                   std::move(longrunning_stub_));
  google::test::admin::database::v1::GetDatabaseRequest request;
  auto failure = stub.AsyncGetDatabase(
      cq, std::make_shared<grpc::ClientContext>(),
      google::cloud::internal::MakeImmutableOptions({}), request);
  EXPECT_THAT(failure.get(), StatusIs(StatusCode::kCancelled));
}

TEST_F(GoldenStubTest, AsyncDropDatabase) {
  auto reader = AsyncTransientError<google::protobuf::Empty>();
  EXPECT_CALL(*grpc_stub_, AsyncDropDatabaseRaw).WillOnce(Return(reader.get()));
  auto mock = std::make_shared<MockCompletionQueueImpl>();
  EXPECT_CALL(*mock, StartOperation)
      .WillOnce([](std::shared_ptr<AsyncGrpcOperation> const& op,
                   absl::FunctionRef<void(void*)> f) {
        f(op.get());
        op->Notify(false);
      });
  EXPECT_CALL(*mock, cq).WillOnce(::testing::Return(nullptr));
  CompletionQueue cq(mock);

  DefaultGoldenThingAdminStub stub(std::move(grpc_stub_),
                                   std::move(location_stub_),
                                   std::move(longrunning_stub_));
  google::test::admin::database::v1::DropDatabaseRequest request;
  auto failure = stub.AsyncDropDatabase(
      cq, std::make_shared<grpc::ClientContext>(),
      google::cloud::internal::MakeImmutableOptions({}), request);
  EXPECT_THAT(failure.get(), StatusIs(StatusCode::kCancelled));
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
  EXPECT_CALL(*mock, cq).WillOnce(::testing::Return(nullptr));
  CompletionQueue cq(mock);

  DefaultGoldenThingAdminStub stub(std::move(grpc_stub_),
                                   std::move(location_stub_),
                                   std::move(longrunning_stub_));
  google::longrunning::GetOperationRequest request;
  auto failure =
      stub.AsyncGetOperation(cq, std::make_shared<grpc::ClientContext>(),
                             internal::MakeImmutableOptions({}), request);
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
  EXPECT_CALL(*mock, cq).WillOnce(::testing::Return(nullptr));
  CompletionQueue cq(mock);

  DefaultGoldenThingAdminStub stub(std::move(grpc_stub_),
                                   std::move(location_stub_),
                                   std::move(longrunning_stub_));
  google::longrunning::CancelOperationRequest request;
  auto failure =
      stub.AsyncCancelOperation(cq, std::make_shared<grpc::ClientContext>(),
                                internal::MakeImmutableOptions({}), request);
  EXPECT_THAT(failure.get(), StatusIs(StatusCode::kCancelled));
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace golden_v1_internal
}  // namespace cloud
}  // namespace google
