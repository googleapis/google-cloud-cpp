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

#ifndef GOOGLE_CLOUD_CPP_SPANNER_GOOGLE_CLOUD_SPANNER_TESTING_MOCK_DATABASE_ADMIN_STUB_H_
#define GOOGLE_CLOUD_CPP_SPANNER_GOOGLE_CLOUD_SPANNER_TESTING_MOCK_DATABASE_ADMIN_STUB_H_

#include "google/cloud/spanner/internal/database_admin_stub.h"
#include "google/cloud/spanner/version.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace spanner_testing {
inline namespace SPANNER_CLIENT_NS {

class MockDatabaseAdminStub
    : public google::cloud::spanner::internal::DatabaseAdminStub {
 public:
  MOCK_METHOD2(
      CreateDatabase,
      StatusOr<google::longrunning::Operation>(
          grpc::ClientContext&,
          google::spanner::admin::database::v1::CreateDatabaseRequest const&));

  MOCK_METHOD2(
      GetDatabase,
      StatusOr<google::spanner::admin::database::v1::Database>(
          grpc::ClientContext&,
          google::spanner::admin::database::v1::GetDatabaseRequest const&));

  MOCK_METHOD2(
      GetDatabaseDdl,
      StatusOr<google::spanner::admin::database::v1::GetDatabaseDdlResponse>(
          grpc::ClientContext&,
          google::spanner::admin::database::v1::GetDatabaseDdlRequest const&));

  MOCK_METHOD2(UpdateDatabase,
               StatusOr<google::longrunning::Operation>(
                   grpc::ClientContext&, google::spanner::admin::database::v1::
                                             UpdateDatabaseDdlRequest const&));

  MOCK_METHOD2(
      DropDatabase,
      Status(grpc::ClientContext&,
             google::spanner::admin::database::v1::DropDatabaseRequest const&));

  MOCK_METHOD2(
      ListDatabases,
      StatusOr<google::spanner::admin::database::v1::ListDatabasesResponse>(
          grpc::ClientContext&,
          google::spanner::admin::database::v1::ListDatabasesRequest const&));

  MOCK_METHOD2(GetIamPolicy, StatusOr<google::iam::v1::Policy>(
                                 grpc::ClientContext&,
                                 google::iam::v1::GetIamPolicyRequest const&));

  MOCK_METHOD2(SetIamPolicy, StatusOr<google::iam::v1::Policy>(
                                 grpc::ClientContext&,
                                 google::iam::v1::SetIamPolicyRequest const&));

  MOCK_METHOD2(TestIamPermissions,
               StatusOr<google::iam::v1::TestIamPermissionsResponse>(
                   grpc::ClientContext&,
                   google::iam::v1::TestIamPermissionsRequest const&));

  MOCK_METHOD2(GetOperation,
               StatusOr<google::longrunning::Operation>(
                   grpc::ClientContext&,
                   google::longrunning::GetOperationRequest const&));
};

}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner_testing
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_SPANNER_GOOGLE_CLOUD_SPANNER_TESTING_MOCK_DATABASE_ADMIN_STUB_H_
