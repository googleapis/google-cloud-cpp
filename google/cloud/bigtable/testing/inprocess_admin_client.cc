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

#include "google/cloud/bigtable/testing/inprocess_admin_client.h"

namespace google {
namespace cloud {
namespace bigtable {
namespace testing {

namespace btadmin = google::bigtable::admin::v2;

grpc::Status InProcessAdminClient::CreateTable(
    grpc::ClientContext* context, btadmin::CreateTableRequest const& request,
    btadmin::Table* response) {
  return Stub()->CreateTable(context, request, response);
}

std::unique_ptr<grpc::ClientAsyncResponseReaderInterface<btadmin::Table>>
InProcessAdminClient::AsyncCreateTable(
    grpc::ClientContext* context, btadmin::CreateTableRequest const& request,
    grpc::CompletionQueue* cq) {
  return Stub()->AsyncCreateTable(context, request, cq);
}

grpc::Status InProcessAdminClient::ListTables(
    grpc::ClientContext* context, btadmin::ListTablesRequest const& request,
    btadmin::ListTablesResponse* response) {
  return Stub()->ListTables(context, request, response);
}

std::unique_ptr<grpc::ClientAsyncResponseReaderInterface<
    google::bigtable::admin::v2::ListTablesResponse>>
InProcessAdminClient::AsyncListTables(
    grpc::ClientContext* context,
    google::bigtable::admin::v2::ListTablesRequest const& request,
    grpc::CompletionQueue* cq) {
  return Stub()->AsyncListTables(context, request, cq);
}

grpc::Status InProcessAdminClient::GetTable(
    grpc::ClientContext* context, btadmin::GetTableRequest const& request,
    btadmin::Table* response) {
  return Stub()->GetTable(context, request, response);
}

std::unique_ptr<grpc::ClientAsyncResponseReaderInterface<btadmin::Table>>
InProcessAdminClient::AsyncGetTable(grpc::ClientContext* context,
                                    btadmin::GetTableRequest const& request,
                                    grpc::CompletionQueue* cq) {
  return Stub()->AsyncGetTable(context, request, cq);
}

grpc::Status InProcessAdminClient::DeleteTable(
    grpc::ClientContext* context, btadmin::DeleteTableRequest const& request,
    google::protobuf::Empty* response) {
  return Stub()->DeleteTable(context, request, response);
}

std::unique_ptr<
    grpc::ClientAsyncResponseReaderInterface<google::protobuf::Empty>>
InProcessAdminClient::AsyncDeleteTable(
    grpc::ClientContext* context,
    google::bigtable::admin::v2::DeleteTableRequest const& request,
    grpc::CompletionQueue* cq) {
  return Stub()->AsyncDeleteTable(context, request, cq);
}

grpc::Status InProcessAdminClient::CreateBackup(
    grpc::ClientContext* context,
    google::bigtable::admin::v2::CreateBackupRequest const& request,
    google::longrunning::Operation* response) {
  return Stub()->CreateBackup(context, request, response);
}

std::unique_ptr<
    grpc::ClientAsyncResponseReaderInterface<google::longrunning::Operation>>
InProcessAdminClient::AsyncCreateBackup(
    grpc::ClientContext* context,
    google::bigtable::admin::v2::CreateBackupRequest const& request,
    grpc::CompletionQueue* cq) {
  return Stub()->AsyncCreateBackup(context, request, cq);
}

grpc::Status InProcessAdminClient::GetBackup(
    grpc::ClientContext* context,
    google::bigtable::admin::v2::GetBackupRequest const& request,
    google::bigtable::admin::v2::Backup* response) {
  return Stub()->GetBackup(context, request, response);
}

std::unique_ptr<grpc::ClientAsyncResponseReaderInterface<
    google::bigtable::admin::v2::Backup>>
InProcessAdminClient::AsyncGetBackup(
    grpc::ClientContext* context,
    google::bigtable::admin::v2::GetBackupRequest const& request,
    grpc::CompletionQueue* cq) {
  return Stub()->AsyncGetBackup(context, request, cq);
}

grpc::Status InProcessAdminClient::UpdateBackup(
    grpc::ClientContext* context,
    google::bigtable::admin::v2::UpdateBackupRequest const& request,
    google::bigtable::admin::v2::Backup* response) {
  return Stub()->UpdateBackup(context, request, response);
}

std::unique_ptr<grpc::ClientAsyncResponseReaderInterface<
    google::bigtable::admin::v2::Backup>>
InProcessAdminClient::AsyncUpdateBackup(
    grpc::ClientContext* context,
    google::bigtable::admin::v2::UpdateBackupRequest const& request,
    grpc::CompletionQueue* cq) {
  return Stub()->AsyncUpdateBackup(context, request, cq);
}

grpc::Status InProcessAdminClient::DeleteBackup(
    grpc::ClientContext* context,
    google::bigtable::admin::v2::DeleteBackupRequest const& request,
    google::protobuf::Empty* response) {
  return Stub()->DeleteBackup(context, request, response);
}

std::unique_ptr<
    grpc::ClientAsyncResponseReaderInterface<google::protobuf::Empty>>
InProcessAdminClient::AsyncDeleteBackup(
    grpc::ClientContext* context,
    google::bigtable::admin::v2::DeleteBackupRequest const& request,
    grpc::CompletionQueue* cq) {
  return Stub()->AsyncDeleteBackup(context, request, cq);
}

grpc::Status InProcessAdminClient::ListBackups(
    grpc::ClientContext* context,
    google::bigtable::admin::v2::ListBackupsRequest const& request,
    google::bigtable::admin::v2::ListBackupsResponse* response) {
  return Stub()->ListBackups(context, request, response);
}

std::unique_ptr<grpc::ClientAsyncResponseReaderInterface<
    google::bigtable::admin::v2::ListBackupsResponse>>
InProcessAdminClient::AsyncListBackups(
    grpc::ClientContext* context,
    google::bigtable::admin::v2::ListBackupsRequest const& request,
    grpc::CompletionQueue* cq) {
  return Stub()->AsyncListBackups(context, request, cq);
}

grpc::Status InProcessAdminClient::RestoreTable(
    grpc::ClientContext* context,
    google::bigtable::admin::v2::RestoreTableRequest const& request,
    google::longrunning::Operation* response) {
  return Stub()->RestoreTable(context, request, response);
}

std::unique_ptr<
    grpc::ClientAsyncResponseReaderInterface<google::longrunning::Operation>>
InProcessAdminClient::AsyncRestoreTable(
    grpc::ClientContext* context,
    google::bigtable::admin::v2::RestoreTableRequest const& request,
    grpc::CompletionQueue* cq) {
  return Stub()->AsyncRestoreTable(context, request, cq);
}

grpc::Status InProcessAdminClient::ModifyColumnFamilies(
    grpc::ClientContext* context,
    btadmin::ModifyColumnFamiliesRequest const& request,
    btadmin::Table* response) {
  return Stub()->ModifyColumnFamilies(context, request, response);
}

grpc::Status InProcessAdminClient::DropRowRange(
    grpc::ClientContext* context, btadmin::DropRowRangeRequest const& request,
    google::protobuf::Empty* response) {
  return Stub()->DropRowRange(context, request, response);
}

grpc::Status InProcessAdminClient::GenerateConsistencyToken(
    grpc::ClientContext* context,
    btadmin::GenerateConsistencyTokenRequest const& request,
    btadmin::GenerateConsistencyTokenResponse* response) {
  return Stub()->GenerateConsistencyToken(context, request, response);
}

grpc::Status InProcessAdminClient::CheckConsistency(
    grpc::ClientContext* context,
    btadmin::CheckConsistencyRequest const& request,
    btadmin::CheckConsistencyResponse* response) {
  return Stub()->CheckConsistency(context, request, response);
}

grpc::Status InProcessAdminClient::GetIamPolicy(
    grpc::ClientContext* context,
    google::iam::v1::GetIamPolicyRequest const& request,
    google::iam::v1::Policy* response) {
  return Stub()->GetIamPolicy(context, request, response);
}

grpc::Status InProcessAdminClient::SetIamPolicy(
    grpc::ClientContext* context,
    google::iam::v1::SetIamPolicyRequest const& request,
    google::iam::v1::Policy* response) {
  return Stub()->SetIamPolicy(context, request, response);
}

grpc::Status InProcessAdminClient::TestIamPermissions(
    grpc::ClientContext* context,
    google::iam::v1::TestIamPermissionsRequest const& request,
    google::iam::v1::TestIamPermissionsResponse* response) {
  return Stub()->TestIamPermissions(context, request, response);
}

grpc::Status InProcessAdminClient::GetOperation(
    grpc::ClientContext* context,
    google::longrunning::GetOperationRequest const& request,
    google::longrunning::Operation* response) {
  auto stub = google::longrunning::Operations::NewStub(Channel());
  return stub->GetOperation(context, request, response);
}

std::unique_ptr<grpc::ClientAsyncResponseReaderInterface<btadmin::Table>>
InProcessAdminClient::AsyncModifyColumnFamilies(
    grpc::ClientContext* context,
    btadmin::ModifyColumnFamiliesRequest const& request,
    grpc::CompletionQueue* cq) {
  return Stub()->AsyncModifyColumnFamilies(context, request, cq);
}

std::unique_ptr<
    grpc::ClientAsyncResponseReaderInterface<google::protobuf::Empty>>
InProcessAdminClient::AsyncDropRowRange(
    grpc::ClientContext* context, btadmin::DropRowRangeRequest const& request,
    grpc::CompletionQueue* cq) {
  return Stub()->AsyncDropRowRange(context, request, cq);
}

std::unique_ptr<grpc::ClientAsyncResponseReaderInterface<
    google::bigtable::admin::v2::GenerateConsistencyTokenResponse>>
InProcessAdminClient::AsyncGenerateConsistencyToken(
    grpc::ClientContext* context,
    const google::bigtable::admin::v2::GenerateConsistencyTokenRequest& request,
    grpc::CompletionQueue* cq) {
  return Stub()->AsyncGenerateConsistencyToken(context, request, cq);
}

std::unique_ptr<grpc::ClientAsyncResponseReaderInterface<
    google::bigtable::admin::v2::CheckConsistencyResponse>>
InProcessAdminClient::AsyncCheckConsistency(
    grpc::ClientContext* context,
    const google::bigtable::admin::v2::CheckConsistencyRequest& request,
    grpc::CompletionQueue* cq) {
  return Stub()->AsyncCheckConsistency(context, request, cq);
}

std::unique_ptr<
    grpc::ClientAsyncResponseReaderInterface<google::iam::v1::Policy>>
InProcessAdminClient::AsyncGetIamPolicy(
    grpc::ClientContext* context,
    google::iam::v1::GetIamPolicyRequest const& request,
    grpc::CompletionQueue* cq) {
  return Stub()->AsyncGetIamPolicy(context, request, cq);
}

std::unique_ptr<
    grpc::ClientAsyncResponseReaderInterface<google::iam::v1::Policy>>
InProcessAdminClient::AsyncSetIamPolicy(
    grpc::ClientContext* context,
    google::iam::v1::SetIamPolicyRequest const& request,
    grpc::CompletionQueue* cq) {
  return Stub()->AsyncSetIamPolicy(context, request, cq);
}

std::unique_ptr<grpc::ClientAsyncResponseReaderInterface<
    google::iam::v1::TestIamPermissionsResponse>>
InProcessAdminClient::AsyncTestIamPermissions(
    grpc::ClientContext* context,
    google::iam::v1::TestIamPermissionsRequest const& request,
    grpc::CompletionQueue* cq) {
  return Stub()->AsyncTestIamPermissions(context, request, cq);
}

std::unique_ptr<
    grpc::ClientAsyncResponseReaderInterface<google::longrunning::Operation>>
InProcessAdminClient::AsyncGetOperation(
    grpc::ClientContext* context,
    const google::longrunning::GetOperationRequest& request,
    grpc::CompletionQueue* cq) {
  auto stub = google::longrunning::Operations::NewStub(Channel());
  return std::unique_ptr<
      grpc::ClientAsyncResponseReaderInterface<google::longrunning::Operation>>(
      stub->AsyncGetOperation(context, request, cq).release());
}

}  // namespace testing
}  // namespace bigtable
}  // namespace cloud
}  // namespace google
