// Copyright 2020 Google Inc.
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INTERNAL_LOGGING_ADMIN_CLIENT_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INTERNAL_LOGGING_ADMIN_CLIENT_H

#include "google/cloud/bigtable/admin_client.h"
#include <memory>
#include <string>

namespace google {
namespace cloud {
namespace bigtable {
inline namespace BIGTABLE_CLIENT_NS {
namespace internal {

namespace btadmin = ::google::bigtable::admin::v2;

/**
 * Implement a logging AdminClient.
 *
 * This class should not be used by multiple threads, it makes no attempt to
 * protect its critical sections.  While it is rare that the admin interface
 * will be used by multiple threads, we should use the same approach here and in
 * the regular client to support multi-threaded programs.
 *
 * The class also aggressively reconnects on any gRPC errors. A future version
 * should only reconnect on those errors that indicate the credentials or
 * connections need refreshing.
 */

class LoggingAdminClient : public google::cloud::bigtable::AdminClient {
 public:
  LoggingAdminClient(
      std::shared_ptr<google::cloud::bigtable::AdminClient> child,
      google::cloud::TracingOptions options)
      : child_(std::move(child)), tracing_options_(std::move(options)) {}

  std::string const& project() const override { return child_->project(); }

  std::shared_ptr<grpc::Channel> Channel() override {
    return child_->Channel();
  }

  void reset() override { child_->reset(); }

  grpc::Status CreateTable(grpc::ClientContext* context,
                           btadmin::CreateTableRequest const& request,
                           btadmin::Table* response) override;

  grpc::Status ListTables(grpc::ClientContext* context,
                          btadmin::ListTablesRequest const& request,
                          btadmin::ListTablesResponse* response) override;

  std::unique_ptr<grpc::ClientAsyncResponseReaderInterface<
      google::bigtable::admin::v2::ListTablesResponse>>
  AsyncListTables(grpc::ClientContext* context,
                  google::bigtable::admin::v2::ListTablesRequest const& request,
                  grpc::CompletionQueue* cq) override;

  grpc::Status GetTable(grpc::ClientContext* context,
                        btadmin::GetTableRequest const& request,
                        btadmin::Table* response) override;

  std::unique_ptr<grpc::ClientAsyncResponseReaderInterface<
      google::bigtable::admin::v2::Table>>
  AsyncGetTable(grpc::ClientContext* context,
                google::bigtable::admin::v2::GetTableRequest const& request,
                grpc::CompletionQueue* cq) override;

  grpc::Status DeleteTable(grpc::ClientContext* context,
                           btadmin::DeleteTableRequest const& request,
                           google::protobuf::Empty* response) override;

  grpc::Status CreateBackup(grpc::ClientContext* context,
                            btadmin::CreateBackupRequest const& request,
                            google::longrunning::Operation* response) override;

  grpc::Status GetBackup(grpc::ClientContext* context,
                         btadmin::GetBackupRequest const& request,
                         btadmin::Backup* response) override;

  grpc::Status UpdateBackup(grpc::ClientContext* context,
                            btadmin::UpdateBackupRequest const& request,
                            btadmin::Backup* response) override;

  grpc::Status DeleteBackup(grpc::ClientContext* context,
                            btadmin::DeleteBackupRequest const& request,
                            google::protobuf::Empty* response) override;

  grpc::Status ListBackups(grpc::ClientContext* context,
                           btadmin::ListBackupsRequest const& request,
                           btadmin::ListBackupsResponse* response) override;

  grpc::Status RestoreTable(grpc::ClientContext* context,
                            btadmin::RestoreTableRequest const& request,
                            google::longrunning::Operation* response) override;

  grpc::Status ModifyColumnFamilies(
      grpc::ClientContext* context,
      btadmin::ModifyColumnFamiliesRequest const& request,
      btadmin::Table* response) override;

  grpc::Status DropRowRange(grpc::ClientContext* context,
                            btadmin::DropRowRangeRequest const& request,
                            google::protobuf::Empty* response) override;

  grpc::Status GenerateConsistencyToken(
      grpc::ClientContext* context,
      btadmin::GenerateConsistencyTokenRequest const& request,
      btadmin::GenerateConsistencyTokenResponse* response) override;

  grpc::Status CheckConsistency(
      grpc::ClientContext* context,
      btadmin::CheckConsistencyRequest const& request,
      btadmin::CheckConsistencyResponse* response) override;

  grpc::Status GetOperation(
      grpc::ClientContext* context,
      google::longrunning::GetOperationRequest const& request,
      google::longrunning::Operation* response) override;

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

  std::unique_ptr<grpc::ClientAsyncResponseReaderInterface<
      google::bigtable::admin::v2::Table>>
  AsyncCreateTable(
      grpc::ClientContext* context,
      google::bigtable::admin::v2::CreateTableRequest const& request,
      grpc::CompletionQueue* cq) override;

  std::unique_ptr<
      ::grpc::ClientAsyncResponseReaderInterface<::google::protobuf::Empty>>
  AsyncDeleteTable(
      grpc::ClientContext* context,
      google::bigtable::admin::v2::DeleteTableRequest const& request,
      grpc::CompletionQueue* cq) override;

  std::unique_ptr<
      grpc::ClientAsyncResponseReaderInterface<google::longrunning::Operation>>
  AsyncCreateBackup(
      grpc::ClientContext* context,
      google::bigtable::admin::v2::CreateBackupRequest const& request,
      grpc::CompletionQueue* cq) override;

  std::unique_ptr<grpc::ClientAsyncResponseReaderInterface<
      google::bigtable::admin::v2::Backup>>
  AsyncGetBackup(grpc::ClientContext* context,
                 google::bigtable::admin::v2::GetBackupRequest const& request,
                 grpc::CompletionQueue* cq) override;

  std::unique_ptr<grpc::ClientAsyncResponseReaderInterface<
      google::bigtable::admin::v2::Backup>>
  AsyncUpdateBackup(
      grpc::ClientContext* context,
      google::bigtable::admin::v2::UpdateBackupRequest const& request,
      grpc::CompletionQueue* cq) override;

  std::unique_ptr<
      grpc::ClientAsyncResponseReaderInterface<google::protobuf::Empty>>
  AsyncDeleteBackup(
      grpc::ClientContext* context,
      google::bigtable::admin::v2::DeleteBackupRequest const& request,
      grpc::CompletionQueue* cq) override;

  std::unique_ptr<grpc::ClientAsyncResponseReaderInterface<
      google::bigtable::admin::v2::ListBackupsResponse>>
  AsyncListBackups(
      grpc::ClientContext* context,
      google::bigtable::admin::v2::ListBackupsRequest const& request,
      grpc::CompletionQueue* cq) override;

  std::unique_ptr<
      grpc::ClientAsyncResponseReaderInterface<google::longrunning::Operation>>
  AsyncRestoreTable(
      grpc::ClientContext* context,
      google::bigtable::admin::v2::RestoreTableRequest const& request,
      grpc::CompletionQueue* cq) override;

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

 private:
  google::cloud::BackgroundThreadsFactory BackgroundThreadsFactory() override {
    return child_->BackgroundThreadsFactory();
  }

  std::shared_ptr<google::cloud::bigtable::AdminClient> child_;
  google::cloud::TracingOptions tracing_options_;
};

}  // namespace internal
}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INTERNAL_LOGGING_ADMIN_CLIENT_H
