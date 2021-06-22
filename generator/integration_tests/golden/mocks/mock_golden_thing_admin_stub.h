// Copyright 2021 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef GOOGLE_CLOUD_CPP_GENERATOR_INTEGRATION_TESTS_GOLDEN_MOCKS_MOCK_GOLDEN_THING_ADMIN_STUB_H
#define GOOGLE_CLOUD_CPP_GENERATOR_INTEGRATION_TESTS_GOLDEN_MOCKS_MOCK_GOLDEN_THING_ADMIN_STUB_H

#include "generator/integration_tests/golden/internal/golden_thing_admin_stub.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace golden_internal {
inline namespace GOOGLE_CLOUD_CPP_GENERATED_NS {

class MockGoldenThingAdminStub
    : public google::cloud::golden_internal::GoldenThingAdminStub {
 public:
  ~MockGoldenThingAdminStub() override = default;

  MOCK_METHOD(
      StatusOr<::google::test::admin::database::v1::ListDatabasesResponse>,
      ListDatabases,
      (grpc::ClientContext&,
       ::google::test::admin::database::v1::ListDatabasesRequest const&),
      (override));
  MOCK_METHOD(
      future<StatusOr<::google::longrunning::Operation>>, AsyncCreateDatabase,
      (google::cloud::CompletionQueue&,
       std::unique_ptr<grpc::ClientContext>,
       ::google::test::admin::database::v1::CreateDatabaseRequest const&),
      (override));
  MOCK_METHOD(StatusOr<::google::test::admin::database::v1::Database>,
              GetDatabase,
              (grpc::ClientContext&,
               ::google::test::admin::database::v1::GetDatabaseRequest const&),
              (override));
  MOCK_METHOD(
      future<StatusOr<::google::longrunning::Operation>>, AsyncUpdateDatabaseDdl,
      (google::cloud::CompletionQueue&,
          std::unique_ptr<grpc::ClientContext>,
       ::google::test::admin::database::v1::UpdateDatabaseDdlRequest const&),
      (override));
  MOCK_METHOD(Status, DropDatabase,
              (grpc::ClientContext&,
               ::google::test::admin::database::v1::DropDatabaseRequest const&),
              (override));
  MOCK_METHOD(
      StatusOr<::google::test::admin::database::v1::GetDatabaseDdlResponse>,
      GetDatabaseDdl,
      (grpc::ClientContext&,
       ::google::test::admin::database::v1::GetDatabaseDdlRequest const&),
      (override));
  MOCK_METHOD(StatusOr<::google::iam::v1::Policy>, SetIamPolicy,
              (grpc::ClientContext&,
               ::google::iam::v1::SetIamPolicyRequest const&),
              (override));
  MOCK_METHOD(StatusOr<::google::iam::v1::Policy>, GetIamPolicy,
              (grpc::ClientContext&,
               ::google::iam::v1::GetIamPolicyRequest const&),
              (override));
  MOCK_METHOD(StatusOr<::google::iam::v1::TestIamPermissionsResponse>,
              TestIamPermissions,
              (grpc::ClientContext&,
               ::google::iam::v1::TestIamPermissionsRequest const&),
              (override));
  MOCK_METHOD(future<StatusOr<::google::longrunning::Operation>>, AsyncCreateBackup,
              (google::cloud::CompletionQueue&,
                  std::unique_ptr<grpc::ClientContext>,
               ::google::test::admin::database::v1::CreateBackupRequest const&),
              (override));
  MOCK_METHOD(StatusOr<::google::test::admin::database::v1::Backup>, GetBackup,
              (grpc::ClientContext&,
               ::google::test::admin::database::v1::GetBackupRequest const&),
              (override));
  MOCK_METHOD(StatusOr<::google::test::admin::database::v1::Backup>,
              UpdateBackup,
              (grpc::ClientContext&,
               ::google::test::admin::database::v1::UpdateBackupRequest const&),
              (override));
  MOCK_METHOD(Status, DeleteBackup,
              (grpc::ClientContext&,
               ::google::test::admin::database::v1::DeleteBackupRequest const&),
              (override));
  MOCK_METHOD(
      StatusOr<::google::test::admin::database::v1::ListBackupsResponse>,
      ListBackups,
      (grpc::ClientContext&,
       ::google::test::admin::database::v1::ListBackupsRequest const&),
      (override));
  MOCK_METHOD(
      future<StatusOr<::google::longrunning::Operation>>, AsyncRestoreDatabase,
      (google::cloud::CompletionQueue&,
          std::unique_ptr<grpc::ClientContext>,
       ::google::test::admin::database::v1::RestoreDatabaseRequest const&),
      (override));
  MOCK_METHOD(
      StatusOr<
          ::google::test::admin::database::v1::ListDatabaseOperationsResponse>,
      ListDatabaseOperations,
      (grpc::ClientContext&,
       ::google::test::admin::database::v1::ListDatabaseOperationsRequest const&
           request),
      (override));
  MOCK_METHOD(
      StatusOr<
          ::google::test::admin::database::v1::ListBackupOperationsResponse>,
      ListBackupOperations,
      (grpc::ClientContext&,
       ::google::test::admin::database::v1::ListBackupOperationsRequest const&),
      (override));

  MOCK_METHOD(future<StatusOr<google::longrunning::Operation>>, AsyncGetOperation,
              (google::cloud::CompletionQueue&,
                  std::unique_ptr<grpc::ClientContext>,
               google::longrunning::GetOperationRequest const&),
              (override));

  MOCK_METHOD(future<Status>, AsyncCancelOperation,
              (google::cloud::CompletionQueue&,
                  std::unique_ptr<grpc::ClientContext>,
                  google::longrunning::CancelOperationRequest const&),
              (override));
};

}  // namespace GOOGLE_CLOUD_CPP_GENERATED_NS
}  // namespace golden_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GENERATOR_INTEGRATION_TESTS_GOLDEN_MOCKS_MOCK_GOLDEN_THING_ADMIN_STUB_H
