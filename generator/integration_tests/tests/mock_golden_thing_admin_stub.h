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

#ifndef GOOGLE_CLOUD_CPP_GENERATOR_INTEGRATION_TESTS_TESTS_MOCK_GOLDEN_THING_ADMIN_STUB_H
#define GOOGLE_CLOUD_CPP_GENERATOR_INTEGRATION_TESTS_TESTS_MOCK_GOLDEN_THING_ADMIN_STUB_H

#include "generator/integration_tests/golden/v1/internal/golden_thing_admin_stub.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace golden_v1_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

class MockGoldenThingAdminStub
    : public google::cloud::golden_v1_internal::GoldenThingAdminStub {
 public:
  ~MockGoldenThingAdminStub() override = default;

  MOCK_METHOD(
      StatusOr<::google::test::admin::database::v1::ListDatabasesResponse>,
      ListDatabases,
      (grpc::ClientContext&, Options const&,
       ::google::test::admin::database::v1::ListDatabasesRequest const&),
      (override));
  MOCK_METHOD(
      future<StatusOr<::google::longrunning::Operation>>, AsyncCreateDatabase,
      (google::cloud::CompletionQueue&, std::shared_ptr<grpc::ClientContext>,
       google::cloud::internal::ImmutableOptions,
       ::google::test::admin::database::v1::CreateDatabaseRequest const&),
      (override));
  MOCK_METHOD(
      StatusOr<google::longrunning::Operation>, CreateDatabase,
      (grpc::ClientContext & context, Options options,
       google::test::admin::database::v1::CreateDatabaseRequest const& request),
      (override));
  MOCK_METHOD(StatusOr<::google::test::admin::database::v1::Database>,
              GetDatabase,
              (grpc::ClientContext&, Options const&,
               ::google::test::admin::database::v1::GetDatabaseRequest const&),
              (override));
  MOCK_METHOD(
      future<StatusOr<::google::longrunning::Operation>>,
      AsyncUpdateDatabaseDdl,
      (google::cloud::CompletionQueue&, std::shared_ptr<grpc::ClientContext>,
       google::cloud::internal::ImmutableOptions,
       ::google::test::admin::database::v1::UpdateDatabaseDdlRequest const&),
      (override));
  MOCK_METHOD(
      StatusOr<google::longrunning::Operation>, UpdateDatabaseDdl,
      (grpc::ClientContext & context, Options options,
       google::test::admin::database::v1::UpdateDatabaseDdlRequest const&
           request),
      (override));
  MOCK_METHOD(Status, DropDatabase,
              (grpc::ClientContext&, Options const&,
               ::google::test::admin::database::v1::DropDatabaseRequest const&),
              (override));
  MOCK_METHOD(
      StatusOr<::google::test::admin::database::v1::GetDatabaseDdlResponse>,
      GetDatabaseDdl,
      (grpc::ClientContext&, Options const&,
       ::google::test::admin::database::v1::GetDatabaseDdlRequest const&),
      (override));
  MOCK_METHOD(StatusOr<::google::iam::v1::Policy>, SetIamPolicy,
              (grpc::ClientContext&, Options const&,
               ::google::iam::v1::SetIamPolicyRequest const&),
              (override));
  MOCK_METHOD(StatusOr<::google::iam::v1::Policy>, GetIamPolicy,
              (grpc::ClientContext&, Options const&,
               ::google::iam::v1::GetIamPolicyRequest const&),
              (override));
  MOCK_METHOD(StatusOr<::google::iam::v1::TestIamPermissionsResponse>,
              TestIamPermissions,
              (grpc::ClientContext&, Options const&,
               ::google::iam::v1::TestIamPermissionsRequest const&),
              (override));
  MOCK_METHOD(future<StatusOr<::google::longrunning::Operation>>,
              AsyncCreateBackup,
              (google::cloud::CompletionQueue&,
               std::shared_ptr<grpc::ClientContext>,
               google::cloud::internal::ImmutableOptions,
               ::google::test::admin::database::v1::CreateBackupRequest const&),
              (override));
  MOCK_METHOD(
      StatusOr<google::longrunning::Operation>, CreateBackup,
      (grpc::ClientContext & context, Options options,
       google::test::admin::database::v1::CreateBackupRequest const& request),
      (override));
  MOCK_METHOD(StatusOr<::google::test::admin::database::v1::Backup>, GetBackup,
              (grpc::ClientContext&, Options const&,
               ::google::test::admin::database::v1::GetBackupRequest const&),
              (override));
  MOCK_METHOD(StatusOr<::google::test::admin::database::v1::Backup>,
              UpdateBackup,
              (grpc::ClientContext&, Options const&,
               ::google::test::admin::database::v1::UpdateBackupRequest const&),
              (override));
  MOCK_METHOD(Status, DeleteBackup,
              (grpc::ClientContext&, Options const&,
               ::google::test::admin::database::v1::DeleteBackupRequest const&),
              (override));
  MOCK_METHOD(
      StatusOr<::google::test::admin::database::v1::ListBackupsResponse>,
      ListBackups,
      (grpc::ClientContext&, Options const&,
       ::google::test::admin::database::v1::ListBackupsRequest const&),
      (override));
  MOCK_METHOD(
      future<StatusOr<::google::longrunning::Operation>>, AsyncRestoreDatabase,
      (google::cloud::CompletionQueue&, std::shared_ptr<grpc::ClientContext>,
       google::cloud::internal::ImmutableOptions,
       ::google::test::admin::database::v1::RestoreDatabaseRequest const&),
      (override));
  MOCK_METHOD(StatusOr<google::longrunning::Operation>, RestoreDatabase,
              (grpc::ClientContext & context, Options options,
               google::test::admin::database::v1::RestoreDatabaseRequest const&
                   request),
              (override));
  MOCK_METHOD(
      StatusOr<
          ::google::test::admin::database::v1::ListDatabaseOperationsResponse>,
      ListDatabaseOperations,
      (grpc::ClientContext&, Options const&,
       ::google::test::admin::database::v1::ListDatabaseOperationsRequest const&
           request),
      (override));
  MOCK_METHOD(
      StatusOr<
          ::google::test::admin::database::v1::ListBackupOperationsResponse>,
      ListBackupOperations,
      (grpc::ClientContext&, Options const&,
       ::google::test::admin::database::v1::ListBackupOperationsRequest const&),
      (override));

  MOCK_METHOD(future<StatusOr<::google::test::admin::database::v1::Database>>,
              AsyncGetDatabase,
              (google::cloud::CompletionQueue&,
               std::shared_ptr<grpc::ClientContext>,
               google::cloud::internal::ImmutableOptions,
               ::google::test::admin::database::v1::GetDatabaseRequest const&),
              (override));

  MOCK_METHOD(future<Status>, AsyncDropDatabase,
              (google::cloud::CompletionQueue&,
               std::shared_ptr<grpc::ClientContext>,
               google::cloud::internal::ImmutableOptions,
               ::google::test::admin::database::v1::DropDatabaseRequest const&),
              (override));

  MOCK_METHOD(
      future<StatusOr<::google::longrunning::Operation>>,
      AsyncLongRunningWithoutRouting,
      (google::cloud::CompletionQueue&, std::shared_ptr<grpc::ClientContext>,
       google::cloud::internal::ImmutableOptions,
       ::google::test::admin::database::v1::RestoreDatabaseRequest const&),
      (override));
  MOCK_METHOD(StatusOr<google::longrunning::Operation>,
              LongRunningWithoutRouting,
              (grpc::ClientContext & context, Options options,
               google::test::admin::database::v1::RestoreDatabaseRequest const&
                   request),
              (override));
  MOCK_METHOD(future<StatusOr<google::longrunning::Operation>>,
              AsyncGetOperation,
              (google::cloud::CompletionQueue&,
               std::shared_ptr<grpc::ClientContext>,
               google::cloud::internal::ImmutableOptions,
               google::longrunning::GetOperationRequest const&),
              (override));

  MOCK_METHOD(future<Status>, AsyncCancelOperation,
              (google::cloud::CompletionQueue&,
               std::shared_ptr<grpc::ClientContext>,
               google::cloud::internal::ImmutableOptions,
               google::longrunning::CancelOperationRequest const&),
              (override));

  MOCK_METHOD(StatusOr<::google::cloud::location::Location>, GetLocation,
              (grpc::ClientContext&, Options const&,
               ::google::cloud::location::GetLocationRequest const&),
              (override));

  MOCK_METHOD(StatusOr<google::longrunning::ListOperationsResponse>,
              ListOperations,
              (grpc::ClientContext&, Options const&,
               ::google::longrunning::ListOperationsRequest const&),
              (override));
};

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace golden_v1_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GENERATOR_INTEGRATION_TESTS_TESTS_MOCK_GOLDEN_THING_ADMIN_STUB_H
