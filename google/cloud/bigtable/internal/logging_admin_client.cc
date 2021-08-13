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

#include "google/cloud/bigtable/internal/logging_admin_client.h"
#include "google/cloud/bigtable/internal/common_client.h"
#include "google/cloud/internal/log_wrapper.h"
#include "google/cloud/log.h"
#include "google/cloud/tracing_options.h"
#include <google/longrunning/operations.grpc.pb.h>

namespace google {
namespace cloud {
namespace bigtable {
inline namespace BIGTABLE_CLIENT_NS {
namespace internal {

namespace btadmin = ::google::bigtable::admin::v2;
using ::google::cloud::internal::LogWrapper;

grpc::Status LoggingAdminClient::CreateTable(
    grpc::ClientContext* context, btadmin::CreateTableRequest const& request,
    btadmin::Table* response) {
  return LogWrapper(
      [this](grpc::ClientContext* context,
             btadmin::CreateTableRequest const& request,
             btadmin::Table* response) {
        return child_->CreateTable(context, request, response);
      },
      context, request, response, __func__, tracing_options_);
}

grpc::Status LoggingAdminClient::ListTables(
    grpc::ClientContext* context, btadmin::ListTablesRequest const& request,
    btadmin::ListTablesResponse* response) {
  return LogWrapper(
      [this](grpc::ClientContext* context,
             btadmin::ListTablesRequest const& request,
             btadmin::ListTablesResponse* response) {
        return child_->ListTables(context, request, response);
      },
      context, request, response, __func__, tracing_options_);
}

std::unique_ptr<grpc::ClientAsyncResponseReaderInterface<
    google::bigtable::admin::v2::ListTablesResponse>>
LoggingAdminClient::AsyncListTables(
    grpc::ClientContext* context,
    google::bigtable::admin::v2::ListTablesRequest const& request,
    grpc::CompletionQueue* cq) {
  return child_->AsyncListTables(context, request, cq);
}

grpc::Status LoggingAdminClient::GetTable(
    grpc::ClientContext* context, btadmin::GetTableRequest const& request,
    btadmin::Table* response) {
  return LogWrapper(
      [this](grpc::ClientContext* context,
             btadmin::GetTableRequest const& request,
             btadmin::Table* response) {
        return child_->GetTable(context, request, response);
      },
      context, request, response, __func__, tracing_options_);
}

std::unique_ptr<grpc::ClientAsyncResponseReaderInterface<
    google::bigtable::admin::v2::Table>>
LoggingAdminClient::AsyncGetTable(
    grpc::ClientContext* context,
    google::bigtable::admin::v2::GetTableRequest const& request,
    grpc::CompletionQueue* cq) {
  return child_->AsyncGetTable(context, request, cq);
}

grpc::Status LoggingAdminClient::DeleteTable(
    grpc::ClientContext* context, btadmin::DeleteTableRequest const& request,
    google::protobuf::Empty* response) {
  return LogWrapper(
      [this](grpc::ClientContext* context,
             btadmin::DeleteTableRequest const& request,
             google::protobuf::Empty* response) {
        return child_->DeleteTable(context, request, response);
      },
      context, request, response, __func__, tracing_options_);
}

grpc::Status LoggingAdminClient::CreateBackup(
    grpc::ClientContext* context, btadmin::CreateBackupRequest const& request,
    google::longrunning::Operation* response) {
  return LogWrapper(
      [this](grpc::ClientContext* context,
             btadmin::CreateBackupRequest const& request,
             google::longrunning::Operation* response) {
        return child_->CreateBackup(context, request, response);
      },
      context, request, response, __func__, tracing_options_);
}

grpc::Status LoggingAdminClient::GetBackup(
    grpc::ClientContext* context, btadmin::GetBackupRequest const& request,
    btadmin::Backup* response) {
  return LogWrapper(
      [this](grpc::ClientContext* context,
             btadmin::GetBackupRequest const& request,
             btadmin::Backup* response) {
        return child_->GetBackup(context, request, response);
      },
      context, request, response, __func__, tracing_options_);
}

grpc::Status LoggingAdminClient::UpdateBackup(
    grpc::ClientContext* context, btadmin::UpdateBackupRequest const& request,
    btadmin::Backup* response) {
  return LogWrapper(
      [this](grpc::ClientContext* context,
             btadmin::UpdateBackupRequest const& request,
             btadmin::Backup* response) {
        return child_->UpdateBackup(context, request, response);
      },
      context, request, response, __func__, tracing_options_);
}

grpc::Status LoggingAdminClient::DeleteBackup(
    grpc::ClientContext* context, btadmin::DeleteBackupRequest const& request,
    google::protobuf::Empty* response) {
  return LogWrapper(
      [this](grpc::ClientContext* context,
             btadmin::DeleteBackupRequest const& request,
             google::protobuf::Empty* response) {
        return child_->DeleteBackup(context, request, response);
      },
      context, request, response, __func__, tracing_options_);
}

grpc::Status LoggingAdminClient::ListBackups(
    grpc::ClientContext* context, btadmin::ListBackupsRequest const& request,
    btadmin::ListBackupsResponse* response) {
  return LogWrapper(
      [this](grpc::ClientContext* context,
             btadmin::ListBackupsRequest const& request,
             btadmin::ListBackupsResponse* response) {
        return child_->ListBackups(context, request, response);
      },
      context, request, response, __func__, tracing_options_);
}

grpc::Status LoggingAdminClient::RestoreTable(
    grpc::ClientContext* context, btadmin::RestoreTableRequest const& request,
    google::longrunning::Operation* response) {
  return LogWrapper(
      [this](grpc::ClientContext* context,
             btadmin::RestoreTableRequest const& request,
             google::longrunning::Operation* response) {
        return child_->RestoreTable(context, request, response);
      },
      context, request, response, __func__, tracing_options_);
}

grpc::Status LoggingAdminClient::ModifyColumnFamilies(
    grpc::ClientContext* context,
    btadmin::ModifyColumnFamiliesRequest const& request,
    btadmin::Table* response) {
  return LogWrapper(
      [this](grpc::ClientContext* context,
             btadmin::ModifyColumnFamiliesRequest const& request,
             btadmin::Table* response) {
        return child_->ModifyColumnFamilies(context, request, response);
      },
      context, request, response, __func__, tracing_options_);
}

grpc::Status LoggingAdminClient::DropRowRange(
    grpc::ClientContext* context, btadmin::DropRowRangeRequest const& request,
    google::protobuf::Empty* response) {
  return LogWrapper(
      [this](grpc::ClientContext* context,
             btadmin::DropRowRangeRequest const& request,
             google::protobuf::Empty* response) {
        return child_->DropRowRange(context, request, response);
      },
      context, request, response, __func__, tracing_options_);
}

grpc::Status LoggingAdminClient::GenerateConsistencyToken(
    grpc::ClientContext* context,
    btadmin::GenerateConsistencyTokenRequest const& request,
    btadmin::GenerateConsistencyTokenResponse* response) {
  return LogWrapper(
      [this](grpc::ClientContext* context,
             btadmin::GenerateConsistencyTokenRequest const& request,
             btadmin::GenerateConsistencyTokenResponse* response) {
        return child_->GenerateConsistencyToken(context, request, response);
      },
      context, request, response, __func__, tracing_options_);
}

grpc::Status LoggingAdminClient::CheckConsistency(
    grpc::ClientContext* context,
    btadmin::CheckConsistencyRequest const& request,
    btadmin::CheckConsistencyResponse* response) {
  return LogWrapper(
      [this](grpc::ClientContext* context,
             btadmin::CheckConsistencyRequest const& request,
             btadmin::CheckConsistencyResponse* response) {
        return child_->CheckConsistency(context, request, response);
      },
      context, request, response, __func__, tracing_options_);
}

grpc::Status LoggingAdminClient::GetOperation(
    grpc::ClientContext* context,
    google::longrunning::GetOperationRequest const& request,
    google::longrunning::Operation* response) {
  return LogWrapper(
      [this](grpc::ClientContext* context,
             google::longrunning::GetOperationRequest const& request,
             google::longrunning::Operation* response) {
        return child_->GetOperation(context, request, response);
      },
      context, request, response, __func__, tracing_options_);
}

grpc::Status LoggingAdminClient::GetIamPolicy(
    grpc::ClientContext* context,
    google::iam::v1::GetIamPolicyRequest const& request,
    google::iam::v1::Policy* response) {
  return LogWrapper(
      [this](grpc::ClientContext* context,
             google::iam::v1::GetIamPolicyRequest const& request,
             google::iam::v1::Policy* response) {
        return child_->GetIamPolicy(context, request, response);
      },
      context, request, response, __func__, tracing_options_);
}

grpc::Status LoggingAdminClient::SetIamPolicy(
    grpc::ClientContext* context,
    google::iam::v1::SetIamPolicyRequest const& request,
    google::iam::v1::Policy* response) {
  return LogWrapper(
      [this](grpc::ClientContext* context,
             google::iam::v1::SetIamPolicyRequest const& request,
             google::iam::v1::Policy* response) {
        return child_->SetIamPolicy(context, request, response);
      },
      context, request, response, __func__, tracing_options_);
}

grpc::Status LoggingAdminClient::TestIamPermissions(
    grpc::ClientContext* context,
    google::iam::v1::TestIamPermissionsRequest const& request,
    google::iam::v1::TestIamPermissionsResponse* response) {
  return LogWrapper(
      [this](grpc::ClientContext* context,
             google::iam::v1::TestIamPermissionsRequest const& request,
             google::iam::v1::TestIamPermissionsResponse* response) {
        return child_->TestIamPermissions(context, request, response);
      },
      context, request, response, __func__, tracing_options_);
}

std::unique_ptr<grpc::ClientAsyncResponseReaderInterface<
    google::bigtable::admin::v2::Table>>
LoggingAdminClient::AsyncCreateTable(
    grpc::ClientContext* context,
    google::bigtable::admin::v2::CreateTableRequest const& request,
    grpc::CompletionQueue* cq) {
  return child_->AsyncCreateTable(context, request, cq);
}

std::unique_ptr<
    ::grpc::ClientAsyncResponseReaderInterface<::google::protobuf::Empty>>
LoggingAdminClient::AsyncDeleteTable(
    grpc::ClientContext* context,
    google::bigtable::admin::v2::DeleteTableRequest const& request,
    grpc::CompletionQueue* cq) {
  return child_->AsyncDeleteTable(context, request, cq);
}

std::unique_ptr<
    grpc::ClientAsyncResponseReaderInterface<google::longrunning::Operation>>
LoggingAdminClient::AsyncCreateBackup(
    grpc::ClientContext* context,
    google::bigtable::admin::v2::CreateBackupRequest const& request,
    grpc::CompletionQueue* cq) {
  return child_->AsyncCreateBackup(context, request, cq);
}

std::unique_ptr<grpc::ClientAsyncResponseReaderInterface<
    google::bigtable::admin::v2::Backup>>
LoggingAdminClient::AsyncGetBackup(
    grpc::ClientContext* context,
    google::bigtable::admin::v2::GetBackupRequest const& request,
    grpc::CompletionQueue* cq) {
  return child_->AsyncGetBackup(context, request, cq);
}

std::unique_ptr<grpc::ClientAsyncResponseReaderInterface<
    google::bigtable::admin::v2::Backup>>
LoggingAdminClient::AsyncUpdateBackup(
    grpc::ClientContext* context,
    google::bigtable::admin::v2::UpdateBackupRequest const& request,
    grpc::CompletionQueue* cq) {
  return child_->AsyncUpdateBackup(context, request, cq);
}

std::unique_ptr<
    grpc::ClientAsyncResponseReaderInterface<google::protobuf::Empty>>
LoggingAdminClient::AsyncDeleteBackup(
    grpc::ClientContext* context,
    google::bigtable::admin::v2::DeleteBackupRequest const& request,
    grpc::CompletionQueue* cq) {
  return child_->AsyncDeleteBackup(context, request, cq);
}

std::unique_ptr<grpc::ClientAsyncResponseReaderInterface<
    google::bigtable::admin::v2::ListBackupsResponse>>
LoggingAdminClient::AsyncListBackups(
    grpc::ClientContext* context,
    google::bigtable::admin::v2::ListBackupsRequest const& request,
    grpc::CompletionQueue* cq) {
  return child_->AsyncListBackups(context, request, cq);
}

std::unique_ptr<
    grpc::ClientAsyncResponseReaderInterface<google::longrunning::Operation>>
LoggingAdminClient::AsyncRestoreTable(
    grpc::ClientContext* context,
    google::bigtable::admin::v2::RestoreTableRequest const& request,
    grpc::CompletionQueue* cq) {
  return child_->AsyncRestoreTable(context, request, cq);
}

std::unique_ptr<grpc::ClientAsyncResponseReaderInterface<
    google::bigtable::admin::v2::Table>>
LoggingAdminClient::AsyncModifyColumnFamilies(
    grpc::ClientContext* context,
    google::bigtable::admin::v2::ModifyColumnFamiliesRequest const& request,
    grpc::CompletionQueue* cq) {
  return child_->AsyncModifyColumnFamilies(context, request, cq);
}

std::unique_ptr<
    grpc::ClientAsyncResponseReaderInterface<google::protobuf::Empty>>
LoggingAdminClient::AsyncDropRowRange(
    grpc::ClientContext* context,
    google::bigtable::admin::v2::DropRowRangeRequest const& request,
    grpc::CompletionQueue* cq) {
  return child_->AsyncDropRowRange(context, request, cq);
};

std::unique_ptr<grpc::ClientAsyncResponseReaderInterface<
    google::bigtable::admin::v2::GenerateConsistencyTokenResponse>>
LoggingAdminClient::AsyncGenerateConsistencyToken(
    grpc::ClientContext* context,
    google::bigtable::admin::v2::GenerateConsistencyTokenRequest const& request,
    grpc::CompletionQueue* cq) {
  return child_->AsyncGenerateConsistencyToken(context, request, cq);
}

std::unique_ptr<grpc::ClientAsyncResponseReaderInterface<
    google::bigtable::admin::v2::CheckConsistencyResponse>>
LoggingAdminClient::AsyncCheckConsistency(
    grpc::ClientContext* context,
    google::bigtable::admin::v2::CheckConsistencyRequest const& request,
    grpc::CompletionQueue* cq) {
  return child_->AsyncCheckConsistency(context, request, cq);
}

std::unique_ptr<
    grpc::ClientAsyncResponseReaderInterface<google::iam::v1::Policy>>
LoggingAdminClient::AsyncGetIamPolicy(
    grpc::ClientContext* context,
    google::iam::v1::GetIamPolicyRequest const& request,
    grpc::CompletionQueue* cq) {
  return child_->AsyncGetIamPolicy(context, request, cq);
}

std::unique_ptr<
    grpc::ClientAsyncResponseReaderInterface<google::iam::v1::Policy>>
LoggingAdminClient::AsyncSetIamPolicy(
    grpc::ClientContext* context,
    google::iam::v1::SetIamPolicyRequest const& request,
    grpc::CompletionQueue* cq) {
  return child_->AsyncSetIamPolicy(context, request, cq);
}

std::unique_ptr<grpc::ClientAsyncResponseReaderInterface<
    google::iam::v1::TestIamPermissionsResponse>>
LoggingAdminClient::AsyncTestIamPermissions(
    grpc::ClientContext* context,
    google::iam::v1::TestIamPermissionsRequest const& request,
    grpc::CompletionQueue* cq) {
  return child_->AsyncTestIamPermissions(context, request, cq);
}

std::unique_ptr<
    grpc::ClientAsyncResponseReaderInterface<google::longrunning::Operation>>
LoggingAdminClient::AsyncGetOperation(
    grpc::ClientContext* context,
    google::longrunning::GetOperationRequest const& request,
    grpc::CompletionQueue* cq) {
  auto stub = google::longrunning::Operations::NewStub(Channel());
  return std::unique_ptr<
      grpc::ClientAsyncResponseReaderInterface<google::longrunning::Operation>>(
      stub->AsyncGetOperation(context, request, cq).release());
}

}  // namespace internal
}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable
}  // namespace cloud
}  // namespace google
