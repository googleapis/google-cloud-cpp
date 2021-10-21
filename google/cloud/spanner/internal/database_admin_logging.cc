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

#include "google/cloud/spanner/internal/database_admin_logging.h"
#include "google/cloud/internal/log_wrapper.h"
#include <grpcpp/grpcpp.h>

namespace google {
namespace cloud {
namespace spanner_internal {
inline namespace SPANNER_CLIENT_NS {

namespace gcsa = ::google::spanner::admin::database::v1;
using ::google::cloud::internal::LogWrapper;

future<StatusOr<google::longrunning::Operation>>
DatabaseAdminLogging::AsyncCreateDatabase(
    CompletionQueue& cq, std::unique_ptr<grpc::ClientContext> context,
    gcsa::CreateDatabaseRequest const& request) {
  return google::cloud::internal::LogWrapper(
      [this](CompletionQueue& cq, std::unique_ptr<grpc::ClientContext> context,
             gcsa::CreateDatabaseRequest const& request) {
        return child_->AsyncCreateDatabase(cq, std::move(context), request);
      },
      cq, std::move(context), request, __func__, tracing_options_);
}

StatusOr<gcsa::Database> DatabaseAdminLogging::GetDatabase(
    grpc::ClientContext& context, gcsa::GetDatabaseRequest const& request) {
  return LogWrapper(
      [this](grpc::ClientContext& context,
             gcsa::GetDatabaseRequest const& request) {
        return child_->GetDatabase(context, request);
      },
      context, request, __func__, tracing_options_);
}

StatusOr<gcsa::GetDatabaseDdlResponse> DatabaseAdminLogging::GetDatabaseDdl(
    grpc::ClientContext& context, gcsa::GetDatabaseDdlRequest const& request) {
  return LogWrapper(
      [this](grpc::ClientContext& context,
             gcsa::GetDatabaseDdlRequest const& request) {
        return child_->GetDatabaseDdl(context, request);
      },
      context, request, __func__, tracing_options_);
}

future<StatusOr<google::longrunning::Operation>>
DatabaseAdminLogging::AsyncUpdateDatabaseDdl(
    CompletionQueue& cq, std::unique_ptr<grpc::ClientContext> context,
    gcsa::UpdateDatabaseDdlRequest const& request) {
  return google::cloud::internal::LogWrapper(
      [this](CompletionQueue& cq, std::unique_ptr<grpc::ClientContext> context,
             gcsa::UpdateDatabaseDdlRequest const& request) {
        return child_->AsyncUpdateDatabaseDdl(cq, std::move(context), request);
      },
      cq, std::move(context), request, __func__, tracing_options_);
}

Status DatabaseAdminLogging::DropDatabase(
    grpc::ClientContext& context,
    google::spanner::admin::database::v1::DropDatabaseRequest const& request) {
  return LogWrapper(
      [this](grpc::ClientContext& context,
             gcsa::DropDatabaseRequest const& request) {
        return child_->DropDatabase(context, request);
      },
      context, request, __func__, tracing_options_);
}

StatusOr<google::spanner::admin::database::v1::ListDatabasesResponse>
DatabaseAdminLogging::ListDatabases(
    grpc::ClientContext& context,
    google::spanner::admin::database::v1::ListDatabasesRequest const& request) {
  return LogWrapper(
      [this](grpc::ClientContext& context,
             google::spanner::admin::database::v1::ListDatabasesRequest const&
                 request) { return child_->ListDatabases(context, request); },
      context, request, __func__, tracing_options_);
}

future<StatusOr<google::longrunning::Operation>>
DatabaseAdminLogging::AsyncRestoreDatabase(
    CompletionQueue& cq, std::unique_ptr<grpc::ClientContext> context,
    gcsa::RestoreDatabaseRequest const& request) {
  return google::cloud::internal::LogWrapper(
      [this](CompletionQueue& cq, std::unique_ptr<grpc::ClientContext> context,
             gcsa::RestoreDatabaseRequest const& request) {
        return child_->AsyncRestoreDatabase(cq, std::move(context), request);
      },
      cq, std::move(context), request, __func__, tracing_options_);
}

StatusOr<google::iam::v1::Policy> DatabaseAdminLogging::GetIamPolicy(
    grpc::ClientContext& context,
    google::iam::v1::GetIamPolicyRequest const& request) {
  return LogWrapper(
      [this](grpc::ClientContext& context,
             google::iam::v1::GetIamPolicyRequest const& request) {
        return child_->GetIamPolicy(context, request);
      },
      context, request, __func__, tracing_options_);
}

StatusOr<google::iam::v1::Policy> DatabaseAdminLogging::SetIamPolicy(
    grpc::ClientContext& context,
    google::iam::v1::SetIamPolicyRequest const& request) {
  return LogWrapper(
      [this](grpc::ClientContext& context,
             google::iam::v1::SetIamPolicyRequest const& request) {
        return child_->SetIamPolicy(context, request);
      },
      context, request, __func__, tracing_options_);
}

StatusOr<google::iam::v1::TestIamPermissionsResponse>
DatabaseAdminLogging::TestIamPermissions(
    grpc::ClientContext& context,
    google::iam::v1::TestIamPermissionsRequest const& request) {
  return LogWrapper(
      [this](grpc::ClientContext& context,
             google::iam::v1::TestIamPermissionsRequest const& request) {
        return child_->TestIamPermissions(context, request);
      },
      context, request, __func__, tracing_options_);
}

future<StatusOr<google::longrunning::Operation>>
DatabaseAdminLogging::AsyncCreateBackup(
    CompletionQueue& cq, std::unique_ptr<grpc::ClientContext> context,
    gcsa::CreateBackupRequest const& request) {
  return google::cloud::internal::LogWrapper(
      [this](CompletionQueue& cq, std::unique_ptr<grpc::ClientContext> context,
             gcsa::CreateBackupRequest const& request) {
        return child_->AsyncCreateBackup(cq, std::move(context), request);
      },
      cq, std::move(context), request, __func__, tracing_options_);
}

StatusOr<gcsa::Backup> DatabaseAdminLogging::GetBackup(
    grpc::ClientContext& context, gcsa::GetBackupRequest const& request) {
  return LogWrapper(
      [this](grpc::ClientContext& context,
             gcsa::GetBackupRequest const& request) {
        return child_->GetBackup(context, request);
      },
      context, request, __func__, tracing_options_);
}

Status DatabaseAdminLogging::DeleteBackup(
    grpc::ClientContext& context,
    google::spanner::admin::database::v1::DeleteBackupRequest const& request) {
  return LogWrapper(
      [this](grpc::ClientContext& context,
             gcsa::DeleteBackupRequest const& request) {
        return child_->DeleteBackup(context, request);
      },
      context, request, __func__, tracing_options_);
}

StatusOr<google::spanner::admin::database::v1::ListBackupsResponse>
DatabaseAdminLogging::ListBackups(
    grpc::ClientContext& context,
    google::spanner::admin::database::v1::ListBackupsRequest const& request) {
  return LogWrapper(
      [this](grpc::ClientContext& context,
             google::spanner::admin::database::v1::ListBackupsRequest const&
                 request) { return child_->ListBackups(context, request); },
      context, request, __func__, tracing_options_);
}

StatusOr<gcsa::Backup> DatabaseAdminLogging::UpdateBackup(
    grpc::ClientContext& context, gcsa::UpdateBackupRequest const& request) {
  return LogWrapper(
      [this](grpc::ClientContext& context,
             gcsa::UpdateBackupRequest const& request) {
        return child_->UpdateBackup(context, request);
      },
      context, request, __func__, tracing_options_);
}

StatusOr<google::spanner::admin::database::v1::ListBackupOperationsResponse>
DatabaseAdminLogging::ListBackupOperations(
    grpc::ClientContext& context,
    google::spanner::admin::database::v1::ListBackupOperationsRequest const&
        request) {
  return LogWrapper(
      [this](grpc::ClientContext& context,
             google::spanner::admin::database::v1::
                 ListBackupOperationsRequest const& request) {
        return child_->ListBackupOperations(context, request);
      },
      context, request, __func__, tracing_options_);
}

StatusOr<google::spanner::admin::database::v1::ListDatabaseOperationsResponse>
DatabaseAdminLogging::ListDatabaseOperations(
    grpc::ClientContext& context,
    google::spanner::admin::database::v1::ListDatabaseOperationsRequest const&
        request) {
  return LogWrapper(
      [this](grpc::ClientContext& context,
             google::spanner::admin::database::v1::
                 ListDatabaseOperationsRequest const& request) {
        return child_->ListDatabaseOperations(context, request);
      },
      context, request, __func__, tracing_options_);
}

future<StatusOr<google::longrunning::Operation>>
DatabaseAdminLogging::AsyncGetOperation(
    CompletionQueue& cq, std::unique_ptr<grpc::ClientContext> context,
    google::longrunning::GetOperationRequest const& request) {
  return google::cloud::internal::LogWrapper(
      [this](CompletionQueue& cq, std::unique_ptr<grpc::ClientContext> context,
             google::longrunning::GetOperationRequest const& request) {
        return child_->AsyncGetOperation(cq, std::move(context), request);
      },
      cq, std::move(context), request, __func__, tracing_options_);
}

future<Status> DatabaseAdminLogging::AsyncCancelOperation(
    CompletionQueue& cq, std::unique_ptr<grpc::ClientContext> context,
    google::longrunning::CancelOperationRequest const& request) {
  return google::cloud::internal::LogWrapper(
      [this](CompletionQueue& cq, std::unique_ptr<grpc::ClientContext> context,
             google::longrunning::CancelOperationRequest const& request) {
        return child_->AsyncCancelOperation(cq, std::move(context), request);
      },
      cq, std::move(context), request, __func__, tracing_options_);
}

}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner_internal
}  // namespace cloud
}  // namespace google
