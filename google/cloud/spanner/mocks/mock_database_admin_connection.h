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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_MOCKS_MOCK_DATABASE_ADMIN_CONNECTION_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_MOCKS_MOCK_DATABASE_ADMIN_CONNECTION_H

#include "google/cloud/spanner/database_admin_connection.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace spanner_mocks {
inline namespace SPANNER_CLIENT_NS {

/**
 * A class to mock `google::cloud::spanner::DatabaseAdminConnection`.
 *
 * Application developers may want to test their code with simulated responses,
 * including errors from a `spanner::DatabaseAdminClient`. To do so, construct a
 * `spanner::DatabaseAdminClient` with an instance of this class. Then use the
 * Google Test framework functions to program the behavior of this mock.
 */
class MockDatabaseAdminConnection
    : public google::cloud::spanner::DatabaseAdminConnection {
 public:
  MOCK_METHOD1(CreateDatabase,
               future<StatusOr<google::spanner::admin::database::v1::Database>>(
                   CreateDatabaseParams));
  MOCK_METHOD1(GetDatabase,
               StatusOr<google::spanner::admin::database::v1::Database>(
                   GetDatabaseParams));
  MOCK_METHOD1(
      GetDatabaseDdl,
      StatusOr<google::spanner::admin::database::v1::GetDatabaseDdlResponse>(
          GetDatabaseDdlParams));
  MOCK_METHOD1(
      UpdateDatabase,
      future<StatusOr<
          google::spanner::admin::database::v1::UpdateDatabaseDdlMetadata>>(
          UpdateDatabaseParams));
  MOCK_METHOD1(DropDatabase, Status(DropDatabaseParams));
  MOCK_METHOD1(ListDatabases, spanner::ListDatabaseRange(ListDatabasesParams));
  MOCK_METHOD1(RestoreDatabase,
               future<StatusOr<google::spanner::admin::database::v1::Database>>(
                   RestoreDatabaseParams));
  MOCK_METHOD1(GetIamPolicy,
               StatusOr<google::iam::v1::Policy>(GetIamPolicyParams));
  MOCK_METHOD1(SetIamPolicy,
               StatusOr<google::iam::v1::Policy>(SetIamPolicyParams));
  MOCK_METHOD1(TestIamPermissions,
               StatusOr<google::iam::v1::TestIamPermissionsResponse>(
                   TestIamPermissionsParams));
  MOCK_METHOD1(CreateBackup,
               future<StatusOr<google::spanner::admin::database::v1::Backup>>(
                   CreateBackupParams));
  MOCK_METHOD1(
      GetBackup,
      StatusOr<google::spanner::admin::database::v1::Backup>(GetBackupParams));
  MOCK_METHOD1(DeleteBackup, Status(DeleteBackupParams));
  MOCK_METHOD1(ListBackups, spanner::ListBackupsRange(ListBackupsParams));
  MOCK_METHOD1(UpdateBackup,
               StatusOr<google::spanner::admin::database::v1::Backup>(
                   UpdateBackupParams));
  MOCK_METHOD1(ListBackupOperations,
               spanner::ListBackupOperationsRange(ListBackupOperationsParams));
  MOCK_METHOD1(ListDatabaseOperations, spanner::ListDatabaseOperationsRange(
                                           ListDatabaseOperationsParams));
};

}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner_mocks
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_MOCKS_MOCK_DATABASE_ADMIN_CONNECTION_H
