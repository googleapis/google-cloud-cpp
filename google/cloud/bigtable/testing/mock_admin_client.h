// Copyright 2018 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_TESTING_MOCK_ADMIN_CLIENT_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_TESTING_MOCK_ADMIN_CLIENT_H

#include "google/cloud/bigtable/admin_client.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace bigtable {
namespace testing {

class MockAdminClient : public bigtable::AdminClient {
 public:
  MOCK_CONST_METHOD0(project, std::string const&());
  MOCK_METHOD0(Channel, std::shared_ptr<grpc::Channel>());
  MOCK_METHOD0(reset, void());

  MOCK_METHOD3(
      CreateTable,
      grpc::Status(
          grpc::ClientContext* context,
          google::bigtable::admin::v2::CreateTableRequest const& request,
          google::bigtable::admin::v2::Table* response));
  MOCK_METHOD3(
      AsyncCreateTable,
      std::unique_ptr<grpc::ClientAsyncResponseReaderInterface<
          google::bigtable::admin::v2::Table>>(
          grpc::ClientContext* context,
          google::bigtable::admin::v2::CreateTableRequest const& request,
          grpc::CompletionQueue* cq));
  MOCK_METHOD3(
      ListTables,
      grpc::Status(
          grpc::ClientContext* context,
          google::bigtable::admin::v2::ListTablesRequest const& request,
          google::bigtable::admin::v2::ListTablesResponse* response));
  MOCK_METHOD3(
      GetTable,
      grpc::Status(grpc::ClientContext* context,
                   google::bigtable::admin::v2::GetTableRequest const& request,
                   google::bigtable::admin::v2::Table* response));
  MOCK_METHOD3(AsyncGetTable,
               std::unique_ptr<grpc::ClientAsyncResponseReaderInterface<
                   google::bigtable::admin::v2::Table>>(
                   grpc::ClientContext* context,
                   google::bigtable::admin::v2::GetTableRequest const& request,
                   grpc::CompletionQueue* cq));
  MOCK_METHOD3(
      DeleteTable,
      grpc::Status(
          grpc::ClientContext* context,
          google::bigtable::admin::v2::DeleteTableRequest const& request,
          google::protobuf::Empty* response));
  MOCK_METHOD3(
      AsyncDeleteTable,
      std::unique_ptr<
          grpc::ClientAsyncResponseReaderInterface<google::protobuf::Empty>>(
          grpc::ClientContext* context,
          google::bigtable::admin::v2::DeleteTableRequest const& request,
          grpc::CompletionQueue* cq));
  MOCK_METHOD3(
      CreateBackup,
      grpc::Status(
          grpc::ClientContext* context,
          google::bigtable::admin::v2::CreateBackupRequest const& request,
          google::longrunning::Operation* response));
  MOCK_METHOD3(
      GetBackup,
      grpc::Status(grpc::ClientContext* context,
                   google::bigtable::admin::v2::GetBackupRequest const& request,
                   google::bigtable::admin::v2::Backup* response));
  MOCK_METHOD3(
      UpdateBackup,
      grpc::Status(
          grpc::ClientContext* context,
          google::bigtable::admin::v2::UpdateBackupRequest const& request,
          google::bigtable::admin::v2::Backup* response));
  MOCK_METHOD3(
      DeleteBackup,
      grpc::Status(
          grpc::ClientContext* context,
          google::bigtable::admin::v2::DeleteBackupRequest const& request,
          google::protobuf::Empty* response));
  MOCK_METHOD3(
      ListBackups,
      grpc::Status(
          grpc::ClientContext* context,
          google::bigtable::admin::v2::ListBackupsRequest const& request,
          google::bigtable::admin::v2::ListBackupsResponse* response));
  MOCK_METHOD3(
      RestoreTable,
      grpc::Status(
          grpc::ClientContext* context,
          google::bigtable::admin::v2::RestoreTableRequest const& request,
          google::longrunning::Operation* response));
  MOCK_METHOD3(
      AsyncCreateBackup,
      std::unique_ptr<grpc::ClientAsyncResponseReaderInterface<
          google::longrunning::Operation>>(
          grpc::ClientContext* context,
          google::bigtable::admin::v2::CreateBackupRequest const& request,
          grpc::CompletionQueue* cq));
  MOCK_METHOD3(AsyncGetBackup,
               std::unique_ptr<grpc::ClientAsyncResponseReaderInterface<
                   google::bigtable::admin::v2::Backup>>(
                   grpc::ClientContext* context,
                   google::bigtable::admin::v2::GetBackupRequest const& request,
                   grpc::CompletionQueue* cq));
  MOCK_METHOD3(
      AsyncUpdateBackup,
      std::unique_ptr<grpc::ClientAsyncResponseReaderInterface<
          google::bigtable::admin::v2::Backup>>(
          grpc::ClientContext* context,
          google::bigtable::admin::v2::UpdateBackupRequest const& request,
          grpc::CompletionQueue* cq));
  MOCK_METHOD3(
      AsyncDeleteBackup,
      std::unique_ptr<
          grpc::ClientAsyncResponseReaderInterface<google::protobuf::Empty>>(
          grpc::ClientContext* context,
          google::bigtable::admin::v2::DeleteBackupRequest const& request,
          grpc::CompletionQueue* cq));
  MOCK_METHOD3(
      AsyncListBackups,
      std::unique_ptr<grpc::ClientAsyncResponseReaderInterface<
          google::bigtable::admin::v2::ListBackupsResponse>>(
          grpc::ClientContext* context,
          google::bigtable::admin::v2::ListBackupsRequest const& request,
          grpc::CompletionQueue* cq));
  MOCK_METHOD3(
      AsyncRestoreTable,
      std::unique_ptr<grpc::ClientAsyncResponseReaderInterface<
          google::longrunning::Operation>>(
          grpc::ClientContext* context,
          google::bigtable::admin::v2::RestoreTableRequest const& request,
          grpc::CompletionQueue* cq));
  MOCK_METHOD3(ModifyColumnFamilies,
               grpc::Status(grpc::ClientContext* context,
                            google::bigtable::admin::v2::
                                ModifyColumnFamiliesRequest const& request,
                            google::bigtable::admin::v2::Table* response));
  MOCK_METHOD3(
      DropRowRange,
      grpc::Status(
          grpc::ClientContext* context,
          google::bigtable::admin::v2::DropRowRangeRequest const& request,
          google::protobuf::Empty* response));
  MOCK_METHOD3(
      GenerateConsistencyToken,
      grpc::Status(
          grpc::ClientContext* context,
          google::bigtable::admin::v2::GenerateConsistencyTokenRequest const&
              request,
          google::bigtable::admin::v2::GenerateConsistencyTokenResponse*
              response));
  MOCK_METHOD3(
      CheckConsistency,
      grpc::Status(
          grpc::ClientContext* context,
          google::bigtable::admin::v2::CheckConsistencyRequest const& request,
          google::bigtable::admin::v2::CheckConsistencyResponse* response));
  MOCK_METHOD3(GetIamPolicy,
               grpc::Status(grpc::ClientContext* context,
                            google::iam::v1::GetIamPolicyRequest const& request,
                            google::iam::v1::Policy* response));
  MOCK_METHOD3(
      AsyncGetIamPolicy,
      std::unique_ptr<
          grpc::ClientAsyncResponseReaderInterface<google::iam::v1::Policy>>(
          grpc::ClientContext* context,
          google::iam::v1::GetIamPolicyRequest const& request,
          grpc::CompletionQueue* cq));
  MOCK_METHOD3(SetIamPolicy,
               grpc::Status(grpc::ClientContext* context,
                            google::iam::v1::SetIamPolicyRequest const& request,
                            google::iam::v1::Policy* response));
  MOCK_METHOD3(
      AsyncSetIamPolicy,
      std::unique_ptr<
          grpc::ClientAsyncResponseReaderInterface<google::iam::v1::Policy>>(
          grpc::ClientContext* context,
          google::iam::v1::SetIamPolicyRequest const& request,
          grpc::CompletionQueue* cq));
  MOCK_METHOD3(
      TestIamPermissions,
      grpc::Status(grpc::ClientContext* context,
                   google::iam::v1::TestIamPermissionsRequest const& request,
                   google::iam::v1::TestIamPermissionsResponse* response));
  MOCK_METHOD3(AsyncTestIamPermissions,
               std::unique_ptr<grpc::ClientAsyncResponseReaderInterface<
                   google::iam::v1::TestIamPermissionsResponse>>(
                   grpc::ClientContext*,
                   google::iam::v1::TestIamPermissionsRequest const&,
                   grpc::CompletionQueue*));
  MOCK_METHOD3(
      GetOperation,
      grpc::Status(grpc::ClientContext* context,
                   google::longrunning::GetOperationRequest const& request,
                   google::longrunning::Operation* response));

  MOCK_METHOD3(
      AsyncModifyColumnFamilies,
      std::unique_ptr<grpc::ClientAsyncResponseReaderInterface<
          google::bigtable::admin::v2::Table>>(
          grpc::ClientContext* context,
          google::bigtable::admin::v2::ModifyColumnFamiliesRequest const&
              request,
          grpc::CompletionQueue* cq));
  MOCK_METHOD3(
      AsyncDropRowRange,
      std::unique_ptr<
          grpc::ClientAsyncResponseReaderInterface<google::protobuf::Empty>>(
          grpc::ClientContext* context,
          google::bigtable::admin::v2::DropRowRangeRequest const& request,
          grpc::CompletionQueue* cq));
  MOCK_METHOD3(
      AsyncGenerateConsistencyToken,
      std::unique_ptr<grpc::ClientAsyncResponseReaderInterface<
          google::bigtable::admin::v2::GenerateConsistencyTokenResponse>>(
          grpc::ClientContext*,
          const google::bigtable::admin::v2::GenerateConsistencyTokenRequest&,
          grpc::CompletionQueue*));
  MOCK_METHOD3(AsyncCheckConsistency,
               std::unique_ptr<grpc::ClientAsyncResponseReaderInterface<
                   google::bigtable::admin::v2::CheckConsistencyResponse>>(
                   grpc::ClientContext*,
                   const google::bigtable::admin::v2::CheckConsistencyRequest&,
                   grpc::CompletionQueue*));
  MOCK_METHOD3(
      AsyncListTables,
      std::unique_ptr<grpc::ClientAsyncResponseReaderInterface<
          google::bigtable::admin::v2::ListTablesResponse>>(
          grpc::ClientContext* context,
          google::bigtable::admin::v2::ListTablesRequest const& request,
          grpc::CompletionQueue* cq));
  MOCK_METHOD3(AsyncGetOperation,
               std::unique_ptr<grpc::ClientAsyncResponseReaderInterface<
                   google::longrunning::Operation>>(
                   grpc::ClientContext* context,
                   const google::longrunning::GetOperationRequest& request,
                   grpc::CompletionQueue* cq));
};

}  // namespace testing
}  // namespace bigtable
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_TESTING_MOCK_ADMIN_CLIENT_H
