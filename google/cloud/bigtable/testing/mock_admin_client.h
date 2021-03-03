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
#include <string>

namespace google {
namespace cloud {
namespace bigtable {
namespace testing {

class MockAdminClient : public bigtable::AdminClient {
 public:
  MOCK_METHOD(std::string const&, project, (), (const override));
  MOCK_METHOD(std::shared_ptr<grpc::Channel>, Channel, (), (override));
  MOCK_METHOD(void, reset, (), (override));

  MOCK_METHOD(grpc::Status, CreateTable,
              (grpc::ClientContext * context,
               google::bigtable::admin::v2::CreateTableRequest const& request,
               google::bigtable::admin::v2::Table* response),
              (override));
  MOCK_METHOD(std::unique_ptr<grpc::ClientAsyncResponseReaderInterface<
                  google::bigtable::admin::v2::Table>>,
              AsyncCreateTable,
              (grpc::ClientContext * context,
               google::bigtable::admin::v2::CreateTableRequest const& request,
               grpc::CompletionQueue* cq),
              (override));
  MOCK_METHOD(grpc::Status, ListTables,
              (grpc::ClientContext * context,
               google::bigtable::admin::v2::ListTablesRequest const& request,
               google::bigtable::admin::v2::ListTablesResponse* response),
              (override));
  MOCK_METHOD(grpc::Status, GetTable,
              (grpc::ClientContext * context,
               google::bigtable::admin::v2::GetTableRequest const& request,
               google::bigtable::admin::v2::Table* response),
              (override));
  MOCK_METHOD(std::unique_ptr<grpc::ClientAsyncResponseReaderInterface<
                  google::bigtable::admin::v2::Table>>,
              AsyncGetTable,
              (grpc::ClientContext * context,
               google::bigtable::admin::v2::GetTableRequest const& request,
               grpc::CompletionQueue* cq),
              (override));
  MOCK_METHOD(grpc::Status, DeleteTable,
              (grpc::ClientContext * context,
               google::bigtable::admin::v2::DeleteTableRequest const& request,
               google::protobuf::Empty* response),
              (override));
  MOCK_METHOD(
      std::unique_ptr<
          grpc::ClientAsyncResponseReaderInterface<google::protobuf::Empty>>,
      AsyncDeleteTable,
      (grpc::ClientContext * context,
       google::bigtable::admin::v2::DeleteTableRequest const& request,
       grpc::CompletionQueue* cq),
      (override));
  MOCK_METHOD(grpc::Status, CreateBackup,
              (grpc::ClientContext * context,
               google::bigtable::admin::v2::CreateBackupRequest const& request,
               google::longrunning::Operation* response),
              (override));
  MOCK_METHOD(grpc::Status, GetBackup,
              (grpc::ClientContext * context,
               google::bigtable::admin::v2::GetBackupRequest const& request,
               google::bigtable::admin::v2::Backup* response),
              (override));
  MOCK_METHOD(grpc::Status, UpdateBackup,
              (grpc::ClientContext * context,
               google::bigtable::admin::v2::UpdateBackupRequest const& request,
               google::bigtable::admin::v2::Backup* response),
              (override));
  MOCK_METHOD(grpc::Status, DeleteBackup,
              (grpc::ClientContext * context,
               google::bigtable::admin::v2::DeleteBackupRequest const& request,
               google::protobuf::Empty* response),
              (override));
  MOCK_METHOD(grpc::Status, ListBackups,
              (grpc::ClientContext * context,
               google::bigtable::admin::v2::ListBackupsRequest const& request,
               google::bigtable::admin::v2::ListBackupsResponse* response),
              (override));
  MOCK_METHOD(grpc::Status, RestoreTable,
              (grpc::ClientContext * context,
               google::bigtable::admin::v2::RestoreTableRequest const& request,
               google::longrunning::Operation* response),
              (override));
  MOCK_METHOD(std::unique_ptr<grpc::ClientAsyncResponseReaderInterface<
                  google::longrunning::Operation>>,
              AsyncCreateBackup,
              (grpc::ClientContext * context,
               google::bigtable::admin::v2::CreateBackupRequest const& request,
               grpc::CompletionQueue* cq),
              (override));
  MOCK_METHOD(std::unique_ptr<grpc::ClientAsyncResponseReaderInterface<
                  google::bigtable::admin::v2::Backup>>,
              AsyncGetBackup,
              (grpc::ClientContext * context,
               google::bigtable::admin::v2::GetBackupRequest const& request,
               grpc::CompletionQueue* cq),
              (override));
  MOCK_METHOD(std::unique_ptr<grpc::ClientAsyncResponseReaderInterface<
                  google::bigtable::admin::v2::Backup>>,
              AsyncUpdateBackup,
              (grpc::ClientContext * context,
               google::bigtable::admin::v2::UpdateBackupRequest const& request,
               grpc::CompletionQueue* cq),
              (override));
  MOCK_METHOD(
      std::unique_ptr<
          grpc::ClientAsyncResponseReaderInterface<google::protobuf::Empty>>,
      AsyncDeleteBackup,
      (grpc::ClientContext * context,
       google::bigtable::admin::v2::DeleteBackupRequest const& request,
       grpc::CompletionQueue* cq),
      (override));
  MOCK_METHOD(std::unique_ptr<grpc::ClientAsyncResponseReaderInterface<
                  google::bigtable::admin::v2::ListBackupsResponse>>,
              AsyncListBackups,
              (grpc::ClientContext * context,
               google::bigtable::admin::v2::ListBackupsRequest const& request,
               grpc::CompletionQueue* cq),
              (override));
  MOCK_METHOD(std::unique_ptr<grpc::ClientAsyncResponseReaderInterface<
                  google::longrunning::Operation>>,
              AsyncRestoreTable,
              (grpc::ClientContext * context,
               google::bigtable::admin::v2::RestoreTableRequest const& request,
               grpc::CompletionQueue* cq),
              (override));
  MOCK_METHOD(
      grpc::Status, ModifyColumnFamilies,
      (grpc::ClientContext * context,
       google::bigtable::admin::v2::ModifyColumnFamiliesRequest const& request,
       google::bigtable::admin::v2::Table* response),
      (override));
  MOCK_METHOD(grpc::Status, DropRowRange,
              (grpc::ClientContext * context,
               google::bigtable::admin::v2::DropRowRangeRequest const& request,
               google::protobuf::Empty* response),
              (override));
  MOCK_METHOD(
      grpc::Status, GenerateConsistencyToken,
      (grpc::ClientContext * context,
       google::bigtable::admin::v2::GenerateConsistencyTokenRequest const&
           request,
       google::bigtable::admin::v2::GenerateConsistencyTokenResponse* response),
      (override));
  MOCK_METHOD(
      grpc::Status, CheckConsistency,
      (grpc::ClientContext * context,
       google::bigtable::admin::v2::CheckConsistencyRequest const& request,
       google::bigtable::admin::v2::CheckConsistencyResponse* response),
      (override));
  MOCK_METHOD(grpc::Status, GetIamPolicy,
              (grpc::ClientContext * context,
               google::iam::v1::GetIamPolicyRequest const& request,
               google::iam::v1::Policy* response),
              (override));
  MOCK_METHOD(
      std::unique_ptr<
          grpc::ClientAsyncResponseReaderInterface<google::iam::v1::Policy>>,
      AsyncGetIamPolicy,
      (grpc::ClientContext * context,
       google::iam::v1::GetIamPolicyRequest const& request,
       grpc::CompletionQueue* cq),
      (override));
  MOCK_METHOD(grpc::Status, SetIamPolicy,
              (grpc::ClientContext * context,
               google::iam::v1::SetIamPolicyRequest const& request,
               google::iam::v1::Policy* response),
              (override));
  MOCK_METHOD(
      std::unique_ptr<
          grpc::ClientAsyncResponseReaderInterface<google::iam::v1::Policy>>,
      AsyncSetIamPolicy,
      (grpc::ClientContext * context,
       google::iam::v1::SetIamPolicyRequest const& request,
       grpc::CompletionQueue* cq),
      (override));
  MOCK_METHOD(grpc::Status, TestIamPermissions,
              (grpc::ClientContext * context,
               google::iam::v1::TestIamPermissionsRequest const& request,
               google::iam::v1::TestIamPermissionsResponse* response),
              (override));
  MOCK_METHOD(std::unique_ptr<grpc::ClientAsyncResponseReaderInterface<
                  google::iam::v1::TestIamPermissionsResponse>>,
              AsyncTestIamPermissions,
              (grpc::ClientContext*,
               google::iam::v1::TestIamPermissionsRequest const&,
               grpc::CompletionQueue*),
              (override));
  MOCK_METHOD(grpc::Status, GetOperation,
              (grpc::ClientContext * context,
               google::longrunning::GetOperationRequest const& request,
               google::longrunning::Operation* response),
              (override));

  MOCK_METHOD(
      std::unique_ptr<grpc::ClientAsyncResponseReaderInterface<
          google::bigtable::admin::v2::Table>>,
      AsyncModifyColumnFamilies,
      (grpc::ClientContext * context,
       google::bigtable::admin::v2::ModifyColumnFamiliesRequest const& request,
       grpc::CompletionQueue* cq),
      (override));
  MOCK_METHOD(
      std::unique_ptr<
          grpc::ClientAsyncResponseReaderInterface<google::protobuf::Empty>>,
      AsyncDropRowRange,
      (grpc::ClientContext * context,
       google::bigtable::admin::v2::DropRowRangeRequest const& request,
       grpc::CompletionQueue* cq),
      (override));
  MOCK_METHOD(
      std::unique_ptr<grpc::ClientAsyncResponseReaderInterface<
          google::bigtable::admin::v2::GenerateConsistencyTokenResponse>>,
      AsyncGenerateConsistencyToken,
      (grpc::ClientContext*,
       const google::bigtable::admin::v2::GenerateConsistencyTokenRequest&,
       grpc::CompletionQueue*),
      (override));
  MOCK_METHOD(std::unique_ptr<grpc::ClientAsyncResponseReaderInterface<
                  google::bigtable::admin::v2::CheckConsistencyResponse>>,
              AsyncCheckConsistency,
              (grpc::ClientContext*,
               const google::bigtable::admin::v2::CheckConsistencyRequest&,
               grpc::CompletionQueue*),
              (override));
  MOCK_METHOD(std::unique_ptr<grpc::ClientAsyncResponseReaderInterface<
                  google::bigtable::admin::v2::ListTablesResponse>>,
              AsyncListTables,
              (grpc::ClientContext * context,
               google::bigtable::admin::v2::ListTablesRequest const& request,
               grpc::CompletionQueue* cq),
              (override));
  MOCK_METHOD(std::unique_ptr<grpc::ClientAsyncResponseReaderInterface<
                  google::longrunning::Operation>>,
              AsyncGetOperation,
              (grpc::ClientContext * context,
               const google::longrunning::GetOperationRequest& request,
               grpc::CompletionQueue* cq),
              (override));
};

}  // namespace testing
}  // namespace bigtable
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_TESTING_MOCK_ADMIN_CLIENT_H
