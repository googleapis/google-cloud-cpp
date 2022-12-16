// Copyright 2022 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_GENERATOR_INTEGRATION_TESTS_TESTS_MOCK_GOLDEN_THING_ADMIN_REST_STUB_H
#define GOOGLE_CLOUD_CPP_GENERATOR_INTEGRATION_TESTS_TESTS_MOCK_GOLDEN_THING_ADMIN_REST_STUB_H

#include "generator/integration_tests/golden/v1/internal/golden_thing_admin_rest_stub.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace golden_v1_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

class MockGoldenThingAdminRestStub
    : public google::cloud::golden_v1_internal::GoldenThingAdminRestStub {
 public:
  ~MockGoldenThingAdminRestStub() override = default;

  MOCK_METHOD(
      StatusOr<::google::test::admin::database::v1::ListDatabasesResponse>,
      ListDatabases,
      (google::cloud::rest_internal::RestContext&,
       ::google::test::admin::database::v1::ListDatabasesRequest const&),
      (override));
  MOCK_METHOD(
      StatusOr<::google::longrunning::Operation>, CreateDatabase,
      (google::cloud::rest_internal::RestContext&,
       ::google::test::admin::database::v1::CreateDatabaseRequest const&),
      (override));
  MOCK_METHOD(StatusOr<::google::test::admin::database::v1::Database>,
              GetDatabase,
              (google::cloud::rest_internal::RestContext&,
               ::google::test::admin::database::v1::GetDatabaseRequest const&),
              (override));
  MOCK_METHOD(
      StatusOr<::google::longrunning::Operation>, UpdateDatabaseDdl,
      (google::cloud::rest_internal::RestContext&,
       ::google::test::admin::database::v1::UpdateDatabaseDdlRequest const&),
      (override));
  MOCK_METHOD(Status, DropDatabase,
              (google::cloud::rest_internal::RestContext&,
               ::google::test::admin::database::v1::DropDatabaseRequest const&),
              (override));
  MOCK_METHOD(
      StatusOr<::google::test::admin::database::v1::GetDatabaseDdlResponse>,
      GetDatabaseDdl,
      (google::cloud::rest_internal::RestContext&,
       ::google::test::admin::database::v1::GetDatabaseDdlRequest const&),
      (override));
  MOCK_METHOD(StatusOr<::google::iam::v1::Policy>, SetIamPolicy,
              (google::cloud::rest_internal::RestContext&,
               ::google::iam::v1::SetIamPolicyRequest const&),
              (override));
  MOCK_METHOD(StatusOr<::google::iam::v1::Policy>, GetIamPolicy,
              (google::cloud::rest_internal::RestContext&,
               ::google::iam::v1::GetIamPolicyRequest const&),
              (override));
  MOCK_METHOD(StatusOr<::google::iam::v1::TestIamPermissionsResponse>,
              TestIamPermissions,
              (google::cloud::rest_internal::RestContext&,
               ::google::iam::v1::TestIamPermissionsRequest const&),
              (override));
  MOCK_METHOD(StatusOr<::google::longrunning::Operation>, CreateBackup,
              (google::cloud::rest_internal::RestContext&,
               ::google::test::admin::database::v1::CreateBackupRequest const&),
              (override));
  MOCK_METHOD(StatusOr<::google::test::admin::database::v1::Backup>, GetBackup,
              (google::cloud::rest_internal::RestContext&,
               ::google::test::admin::database::v1::GetBackupRequest const&),
              (override));
  MOCK_METHOD(StatusOr<::google::test::admin::database::v1::Backup>,
              UpdateBackup,
              (google::cloud::rest_internal::RestContext&,
               ::google::test::admin::database::v1::UpdateBackupRequest const&),
              (override));
  MOCK_METHOD(Status, DeleteBackup,
              (google::cloud::rest_internal::RestContext&,
               ::google::test::admin::database::v1::DeleteBackupRequest const&),
              (override));
  MOCK_METHOD(
      StatusOr<::google::test::admin::database::v1::ListBackupsResponse>,
      ListBackups,
      (google::cloud::rest_internal::RestContext&,
       ::google::test::admin::database::v1::ListBackupsRequest const&),
      (override));
  MOCK_METHOD(
      StatusOr<::google::longrunning::Operation>, RestoreDatabase,
      (google::cloud::rest_internal::RestContext&,
       ::google::test::admin::database::v1::RestoreDatabaseRequest const&),
      (override));
  MOCK_METHOD(
      StatusOr<
          ::google::test::admin::database::v1::ListDatabaseOperationsResponse>,
      ListDatabaseOperations,
      (google::cloud::rest_internal::RestContext&,
       ::google::test::admin::database::v1::ListDatabaseOperationsRequest const&
           request),
      (override));
  MOCK_METHOD(
      StatusOr<
          ::google::test::admin::database::v1::ListBackupOperationsResponse>,
      ListBackupOperations,
      (google::cloud::rest_internal::RestContext&,
       ::google::test::admin::database::v1::ListBackupOperationsRequest const&),
      (override));
};

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace golden_v1_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GENERATOR_INTEGRATION_TESTS_TESTS_MOCK_GOLDEN_THING_ADMIN_REST_STUB_H
