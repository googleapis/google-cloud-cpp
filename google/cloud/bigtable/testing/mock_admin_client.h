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
  explicit MockAdminClient(ClientOptions options = {})
      : options_(std::move(options)) {}

  MOCK_METHOD(std::string const&, project, (), (const, override));
  MOCK_METHOD(std::shared_ptr<grpc::Channel>, Channel, (), (override));
  MOCK_METHOD(void, reset, (), (override));

  MOCK_METHOD(grpc::Status, CreateTable,
              (grpc::ClientContext*,
               google::bigtable::admin::v2::CreateTableRequest const&,
               google::bigtable::admin::v2::Table*),
              (override));
  MOCK_METHOD(std::unique_ptr<grpc::ClientAsyncResponseReaderInterface<
                  google::bigtable::admin::v2::Table>>,
              AsyncCreateTable,
              (grpc::ClientContext*,
               google::bigtable::admin::v2::CreateTableRequest const&,
               grpc::CompletionQueue*),
              (override));
  MOCK_METHOD(grpc::Status, ListTables,
              (grpc::ClientContext*,
               google::bigtable::admin::v2::ListTablesRequest const&,
               google::bigtable::admin::v2::ListTablesResponse*),
              (override));
  MOCK_METHOD(grpc::Status, GetTable,
              (grpc::ClientContext*,
               google::bigtable::admin::v2::GetTableRequest const&,
               google::bigtable::admin::v2::Table*),
              (override));
  MOCK_METHOD(std::unique_ptr<grpc::ClientAsyncResponseReaderInterface<
                  google::bigtable::admin::v2::Table>>,
              AsyncGetTable,
              (grpc::ClientContext*,
               google::bigtable::admin::v2::GetTableRequest const&,
               grpc::CompletionQueue*),
              (override));
  MOCK_METHOD(grpc::Status, DeleteTable,
              (grpc::ClientContext*,
               google::bigtable::admin::v2::DeleteTableRequest const&,
               google::protobuf::Empty*),
              (override));
  MOCK_METHOD(
      std::unique_ptr<
          grpc::ClientAsyncResponseReaderInterface<google::protobuf::Empty>>,
      AsyncDeleteTable,
      (grpc::ClientContext*,
       google::bigtable::admin::v2::DeleteTableRequest const&,
       grpc::CompletionQueue*),
      (override));
  MOCK_METHOD(grpc::Status, CreateBackup,
              (grpc::ClientContext*,
               google::bigtable::admin::v2::CreateBackupRequest const&,
               google::longrunning::Operation*),
              (override));
  MOCK_METHOD(grpc::Status, GetBackup,
              (grpc::ClientContext*,
               google::bigtable::admin::v2::GetBackupRequest const&,
               google::bigtable::admin::v2::Backup*),
              (override));
  MOCK_METHOD(grpc::Status, UpdateBackup,
              (grpc::ClientContext*,
               google::bigtable::admin::v2::UpdateBackupRequest const&,
               google::bigtable::admin::v2::Backup*),
              (override));
  MOCK_METHOD(grpc::Status, DeleteBackup,
              (grpc::ClientContext*,
               google::bigtable::admin::v2::DeleteBackupRequest const&,
               google::protobuf::Empty*),
              (override));
  MOCK_METHOD(grpc::Status, ListBackups,
              (grpc::ClientContext*,
               google::bigtable::admin::v2::ListBackupsRequest const&,
               google::bigtable::admin::v2::ListBackupsResponse*),
              (override));
  MOCK_METHOD(grpc::Status, RestoreTable,
              (grpc::ClientContext*,
               google::bigtable::admin::v2::RestoreTableRequest const&,
               google::longrunning::Operation*),
              (override));
  MOCK_METHOD(std::unique_ptr<grpc::ClientAsyncResponseReaderInterface<
                  google::longrunning::Operation>>,
              AsyncCreateBackup,
              (grpc::ClientContext*,
               google::bigtable::admin::v2::CreateBackupRequest const&,
               grpc::CompletionQueue*),
              (override));
  MOCK_METHOD(std::unique_ptr<grpc::ClientAsyncResponseReaderInterface<
                  google::bigtable::admin::v2::Backup>>,
              AsyncGetBackup,
              (grpc::ClientContext*,
               google::bigtable::admin::v2::GetBackupRequest const&,
               grpc::CompletionQueue*),
              (override));
  MOCK_METHOD(std::unique_ptr<grpc::ClientAsyncResponseReaderInterface<
                  google::bigtable::admin::v2::Backup>>,
              AsyncUpdateBackup,
              (grpc::ClientContext*,
               google::bigtable::admin::v2::UpdateBackupRequest const&,
               grpc::CompletionQueue*),
              (override));
  MOCK_METHOD(
      std::unique_ptr<
          grpc::ClientAsyncResponseReaderInterface<google::protobuf::Empty>>,
      AsyncDeleteBackup,
      (grpc::ClientContext*,
       google::bigtable::admin::v2::DeleteBackupRequest const&,
       grpc::CompletionQueue*),
      (override));
  MOCK_METHOD(std::unique_ptr<grpc::ClientAsyncResponseReaderInterface<
                  google::bigtable::admin::v2::ListBackupsResponse>>,
              AsyncListBackups,
              (grpc::ClientContext*,
               google::bigtable::admin::v2::ListBackupsRequest const&,
               grpc::CompletionQueue*),
              (override));
  MOCK_METHOD(std::unique_ptr<grpc::ClientAsyncResponseReaderInterface<
                  google::longrunning::Operation>>,
              AsyncRestoreTable,
              (grpc::ClientContext*,
               google::bigtable::admin::v2::RestoreTableRequest const&,
               grpc::CompletionQueue*),
              (override));
  MOCK_METHOD(grpc::Status, ModifyColumnFamilies,
              (grpc::ClientContext*,
               google::bigtable::admin::v2::ModifyColumnFamiliesRequest const&,
               google::bigtable::admin::v2::Table*),
              (override));
  MOCK_METHOD(grpc::Status, DropRowRange,
              (grpc::ClientContext*,
               google::bigtable::admin::v2::DropRowRangeRequest const&,
               google::protobuf::Empty*),
              (override));
  MOCK_METHOD(
      grpc::Status, GenerateConsistencyToken,
      (grpc::ClientContext*,
       google::bigtable::admin::v2::GenerateConsistencyTokenRequest const&,
       google::bigtable::admin::v2::GenerateConsistencyTokenResponse*),
      (override));
  MOCK_METHOD(grpc::Status, CheckConsistency,
              (grpc::ClientContext*,
               google::bigtable::admin::v2::CheckConsistencyRequest const&,
               google::bigtable::admin::v2::CheckConsistencyResponse*),
              (override));
  MOCK_METHOD(grpc::Status, GetIamPolicy,
              (grpc::ClientContext*,
               google::iam::v1::GetIamPolicyRequest const&,
               google::iam::v1::Policy*),
              (override));
  MOCK_METHOD(
      std::unique_ptr<
          grpc::ClientAsyncResponseReaderInterface<google::iam::v1::Policy>>,
      AsyncGetIamPolicy,
      (grpc::ClientContext*, google::iam::v1::GetIamPolicyRequest const&,
       grpc::CompletionQueue*),
      (override));
  MOCK_METHOD(grpc::Status, SetIamPolicy,
              (grpc::ClientContext*,
               google::iam::v1::SetIamPolicyRequest const&,
               google::iam::v1::Policy*),
              (override));
  MOCK_METHOD(
      std::unique_ptr<
          grpc::ClientAsyncResponseReaderInterface<google::iam::v1::Policy>>,
      AsyncSetIamPolicy,
      (grpc::ClientContext*, google::iam::v1::SetIamPolicyRequest const&,
       grpc::CompletionQueue*),
      (override));
  MOCK_METHOD(grpc::Status, TestIamPermissions,
              (grpc::ClientContext*,
               google::iam::v1::TestIamPermissionsRequest const&,
               google::iam::v1::TestIamPermissionsResponse*),
              (override));
  MOCK_METHOD(std::unique_ptr<grpc::ClientAsyncResponseReaderInterface<
                  google::iam::v1::TestIamPermissionsResponse>>,
              AsyncTestIamPermissions,
              (grpc::ClientContext*,
               google::iam::v1::TestIamPermissionsRequest const&,
               grpc::CompletionQueue*),
              (override));
  MOCK_METHOD(grpc::Status, GetOperation,
              (grpc::ClientContext*,
               google::longrunning::GetOperationRequest const&,
               google::longrunning::Operation*),
              (override));

  MOCK_METHOD(std::unique_ptr<grpc::ClientAsyncResponseReaderInterface<
                  google::bigtable::admin::v2::Table>>,
              AsyncModifyColumnFamilies,
              (grpc::ClientContext*,
               google::bigtable::admin::v2::ModifyColumnFamiliesRequest const&,
               grpc::CompletionQueue*),
              (override));
  MOCK_METHOD(
      std::unique_ptr<
          grpc::ClientAsyncResponseReaderInterface<google::protobuf::Empty>>,
      AsyncDropRowRange,
      (grpc::ClientContext*,
       google::bigtable::admin::v2::DropRowRangeRequest const&,
       grpc::CompletionQueue*),
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
              (grpc::ClientContext*,
               google::bigtable::admin::v2::ListTablesRequest const&,
               grpc::CompletionQueue*),
              (override));
  MOCK_METHOD(std::unique_ptr<grpc::ClientAsyncResponseReaderInterface<
                  google::longrunning::Operation>>,
              AsyncGetOperation,
              (grpc::ClientContext*,
               const google::longrunning::GetOperationRequest&,
               grpc::CompletionQueue*),
              (override));

 private:
  ClientOptions::BackgroundThreadsFactory BackgroundThreadsFactory() override {
    return options_.background_threads_factory();
  }

  ClientOptions options_;
};

}  // namespace testing
}  // namespace bigtable
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_TESTING_MOCK_ADMIN_CLIENT_H
