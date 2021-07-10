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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_TESTING_INPROCESS_ADMIN_CLIENT_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_TESTING_INPROCESS_ADMIN_CLIENT_H

#include "google/cloud/bigtable/admin_client.h"
#include "google/cloud/bigtable/client_options.h"
#include <google/bigtable/admin/v2/bigtable_table_admin.grpc.pb.h>
#include <google/longrunning/operations.grpc.pb.h>
#include <string>

namespace google {
namespace cloud {
namespace bigtable {
namespace testing {

/**
 * Connects to Cloud Bigtable's administration APIs.
 *
 * This class is mainly for testing purpose, it enable use of a single embedded
 * server
 * for multiple test cases. This admin client uses a pre-defined channel.
 */
class InProcessAdminClient : public bigtable::AdminClient {
 public:
  InProcessAdminClient(std::string project,
                       std::shared_ptr<grpc::Channel> channel)
      : project_(std::move(project)), channel_(std::move(channel)) {}

  std::unique_ptr<
      google::bigtable::admin::v2::BigtableTableAdmin::StubInterface>
  Stub() {
    return google::bigtable::admin::v2::BigtableTableAdmin::NewStub(channel_);
  }

  std::string const& project() const override { return project_; }
  std::shared_ptr<grpc::Channel> Channel() override { return channel_; }
  void reset() override {}

  //@{
  /// @name the google.bigtable.admin.v2.TableAdmin operations.
  grpc::Status CreateTable(
      grpc::ClientContext* context,
      google::bigtable::admin::v2::CreateTableRequest const& request,
      google::bigtable::admin::v2::Table* response) override;
  std::unique_ptr<grpc::ClientAsyncResponseReaderInterface<
      google::bigtable::admin::v2::Table>>
  AsyncCreateTable(
      grpc::ClientContext* context,
      google::bigtable::admin::v2::CreateTableRequest const& request,
      grpc::CompletionQueue* cq) override;
  grpc::Status ListTables(
      grpc::ClientContext* context,
      google::bigtable::admin::v2::ListTablesRequest const& request,
      google::bigtable::admin::v2::ListTablesResponse* response) override;
  grpc::Status GetTable(
      grpc::ClientContext* context,
      google::bigtable::admin::v2::GetTableRequest const& request,
      google::bigtable::admin::v2::Table* response) override;
  std::unique_ptr<grpc::ClientAsyncResponseReaderInterface<
      google::bigtable::admin::v2::Table>>
  AsyncGetTable(grpc::ClientContext* context,
                google::bigtable::admin::v2::GetTableRequest const& request,
                grpc::CompletionQueue* cq) override;
  grpc::Status DeleteTable(
      grpc::ClientContext* context,
      google::bigtable::admin::v2::DeleteTableRequest const& request,
      google::protobuf::Empty* response) override;
  std::unique_ptr<
      grpc::ClientAsyncResponseReaderInterface<google::protobuf::Empty>>
  AsyncDeleteTable(
      grpc::ClientContext* context,
      google::bigtable::admin::v2::DeleteTableRequest const& request,
      grpc::CompletionQueue* cq) override;
  grpc::Status CreateBackup(
      grpc::ClientContext* context,
      google::bigtable::admin::v2::CreateBackupRequest const& request,
      google::longrunning::Operation* response) override;
  std::unique_ptr<
      grpc::ClientAsyncResponseReaderInterface<google::longrunning::Operation>>
  AsyncCreateBackup(
      grpc::ClientContext* context,
      google::bigtable::admin::v2::CreateBackupRequest const& request,
      grpc::CompletionQueue* cq) override;
  grpc::Status GetBackup(
      grpc::ClientContext* context,
      google::bigtable::admin::v2::GetBackupRequest const& request,
      google::bigtable::admin::v2::Backup* response) override;
  std::unique_ptr<grpc::ClientAsyncResponseReaderInterface<
      google::bigtable::admin::v2::Backup>>
  AsyncGetBackup(grpc::ClientContext* context,
                 google::bigtable::admin::v2::GetBackupRequest const& request,
                 grpc::CompletionQueue* cq) override;
  grpc::Status UpdateBackup(
      grpc::ClientContext* context,
      google::bigtable::admin::v2::UpdateBackupRequest const& request,
      google::bigtable::admin::v2::Backup* response) override;
  std::unique_ptr<grpc::ClientAsyncResponseReaderInterface<
      google::bigtable::admin::v2::Backup>>
  AsyncUpdateBackup(
      grpc::ClientContext* context,
      google::bigtable::admin::v2::UpdateBackupRequest const& request,
      grpc::CompletionQueue* cq) override;
  grpc::Status DeleteBackup(
      grpc::ClientContext* context,
      google::bigtable::admin::v2::DeleteBackupRequest const& request,
      google::protobuf::Empty* response) override;
  std::unique_ptr<
      grpc::ClientAsyncResponseReaderInterface<google::protobuf::Empty>>
  AsyncDeleteBackup(
      grpc::ClientContext* context,
      google::bigtable::admin::v2::DeleteBackupRequest const& request,
      grpc::CompletionQueue* cq) override;
  grpc::Status ListBackups(
      grpc::ClientContext* context,
      google::bigtable::admin::v2::ListBackupsRequest const& request,
      google::bigtable::admin::v2::ListBackupsResponse* response) override;
  std::unique_ptr<grpc::ClientAsyncResponseReaderInterface<
      google::bigtable::admin::v2::ListBackupsResponse>>
  AsyncListBackups(
      grpc::ClientContext* context,
      google::bigtable::admin::v2::ListBackupsRequest const& request,
      grpc::CompletionQueue* cq) override;
  grpc::Status RestoreTable(
      grpc::ClientContext* context,
      google::bigtable::admin::v2::RestoreTableRequest const& request,
      google::longrunning::Operation* response) override;
  std::unique_ptr<
      grpc::ClientAsyncResponseReaderInterface<google::longrunning::Operation>>
  AsyncRestoreTable(
      grpc::ClientContext* context,
      google::bigtable::admin::v2::RestoreTableRequest const& request,
      grpc::CompletionQueue* cq) override;
  grpc::Status ModifyColumnFamilies(
      grpc::ClientContext* context,
      google::bigtable::admin::v2::ModifyColumnFamiliesRequest const& request,
      google::bigtable::admin::v2::Table* response) override;
  grpc::Status DropRowRange(
      grpc::ClientContext* context,
      google::bigtable::admin::v2::DropRowRangeRequest const& request,
      google::protobuf::Empty* response) override;
  grpc::Status GenerateConsistencyToken(
      grpc::ClientContext* context,
      google::bigtable::admin::v2::GenerateConsistencyTokenRequest const&
          request,
      google::bigtable::admin::v2::GenerateConsistencyTokenResponse* response)
      override;
  grpc::Status CheckConsistency(
      grpc::ClientContext* context,
      google::bigtable::admin::v2::CheckConsistencyRequest const& request,
      google::bigtable::admin::v2::CheckConsistencyResponse* response) override;
  grpc::Status GetIamPolicy(grpc::ClientContext* context,
                            google::iam::v1::GetIamPolicyRequest const& request,
                            google::iam::v1::Policy* response) override;
  grpc::Status SetIamPolicy(grpc::ClientContext* context,
                            google::iam::v1::SetIamPolicyRequest const& request,
                            google::iam::v1::Policy* response) override;
  grpc::Status TestIamPermissions(
      grpc::ClientContext* context,
      google::iam::v1::TestIamPermissionsRequest const& request,
      google::iam::v1::TestIamPermissionsResponse* response) override;
  grpc::Status GetOperation(
      grpc::ClientContext* context,
      google::longrunning::GetOperationRequest const& request,
      google::longrunning::Operation* response) override;

  std::unique_ptr<grpc::ClientAsyncResponseReaderInterface<
      google::bigtable::admin::v2::Table>>
  AsyncModifyColumnFamilies(
      grpc::ClientContext* context,
      google::bigtable::admin::v2::ModifyColumnFamiliesRequest const& request,
      grpc::CompletionQueue* cq) override;
  std::unique_ptr<
      grpc::ClientAsyncResponseReaderInterface<google::protobuf::Empty>>
  AsyncDropRowRange(
      grpc::ClientContext* context,
      google::bigtable::admin::v2::DropRowRangeRequest const& request,
      grpc::CompletionQueue* cq) override;
  std::unique_ptr<grpc::ClientAsyncResponseReaderInterface<
      google::bigtable::admin::v2::GenerateConsistencyTokenResponse>>
  AsyncGenerateConsistencyToken(
      grpc::ClientContext* context,
      google::bigtable::admin::v2::GenerateConsistencyTokenRequest const&
          request,
      grpc::CompletionQueue* cq) override;
  std::unique_ptr<grpc::ClientAsyncResponseReaderInterface<
      google::bigtable::admin::v2::CheckConsistencyResponse>>
  AsyncCheckConsistency(
      grpc::ClientContext* context,
      google::bigtable::admin::v2::CheckConsistencyRequest const& request,
      grpc::CompletionQueue* cq) override;
  std::unique_ptr<grpc::ClientAsyncResponseReaderInterface<
      google::bigtable::admin::v2::ListTablesResponse>>
  AsyncListTables(grpc::ClientContext* context,
                  google::bigtable::admin::v2::ListTablesRequest const& request,
                  grpc::CompletionQueue* cq) override;
  std::unique_ptr<
      grpc::ClientAsyncResponseReaderInterface<google::iam::v1::Policy>>
  AsyncGetIamPolicy(grpc::ClientContext* context,
                    google::iam::v1::GetIamPolicyRequest const& request,
                    grpc::CompletionQueue* cq) override;
  std::unique_ptr<
      grpc::ClientAsyncResponseReaderInterface<google::iam::v1::Policy>>
  AsyncSetIamPolicy(grpc::ClientContext* context,
                    google::iam::v1::SetIamPolicyRequest const& request,
                    grpc::CompletionQueue* cq) override;
  std::unique_ptr<grpc::ClientAsyncResponseReaderInterface<
      google::iam::v1::TestIamPermissionsResponse>>
  AsyncTestIamPermissions(
      grpc::ClientContext* context,
      google::iam::v1::TestIamPermissionsRequest const& request,
      grpc::CompletionQueue* cq) override;
  std::unique_ptr<
      grpc::ClientAsyncResponseReaderInterface<google::longrunning::Operation>>
  AsyncGetOperation(grpc::ClientContext* context,
                    google::longrunning::GetOperationRequest const& request,
                    grpc::CompletionQueue* cq) override;
  //@}

 private:
  google::cloud::BackgroundThreadsFactory BackgroundThreadsFactory() override {
    return options_.background_threads_factory();
  }

  std::string project_;
  std::shared_ptr<grpc::Channel> channel_;
  ClientOptions options_;
};

}  // namespace testing
}  // namespace bigtable
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_TESTING_INPROCESS_ADMIN_CLIENT_H
