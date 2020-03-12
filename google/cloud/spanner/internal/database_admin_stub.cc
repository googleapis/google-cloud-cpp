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

#include "google/cloud/spanner/internal/database_admin_stub.h"
#include "google/cloud/spanner/internal/database_admin_logging.h"
#include "google/cloud/spanner/internal/database_admin_metadata.h"
#include "google/cloud/grpc_error_delegate.h"
#include "google/cloud/log.h"
#include <google/longrunning/operations.grpc.pb.h>

namespace google {
namespace cloud {
namespace spanner {
inline namespace SPANNER_CLIENT_NS {
namespace internal {

namespace gcsa = ::google::spanner::admin::database::v1;

DatabaseAdminStub::~DatabaseAdminStub() = default;

class DefaultDatabaseAdminStub : public DatabaseAdminStub {
 public:
  DefaultDatabaseAdminStub(
      std::unique_ptr<gcsa::DatabaseAdmin::Stub> database_admin,
      std::unique_ptr<google::longrunning::Operations::Stub> operations)
      : database_admin_(std::move(database_admin)),
        operations_(std::move(operations)) {}

  ~DefaultDatabaseAdminStub() override = default;

  StatusOr<google::longrunning::Operation> CreateDatabase(
      grpc::ClientContext& client_context,
      gcsa::CreateDatabaseRequest const& request) override {
    google::longrunning::Operation response;
    grpc::Status status =
        database_admin_->CreateDatabase(&client_context, request, &response);
    if (!status.ok()) {
      return google::cloud::MakeStatusFromRpcError(status);
    }
    return response;
  }

  StatusOr<gcsa::Database> GetDatabase(
      grpc::ClientContext& client_context,
      gcsa::GetDatabaseRequest const& request) override {
    gcsa::Database response;
    auto status =
        database_admin_->GetDatabase(&client_context, request, &response);
    if (!status.ok()) {
      return google::cloud::MakeStatusFromRpcError(status);
    }
    return response;
  }

  StatusOr<gcsa::GetDatabaseDdlResponse> GetDatabaseDdl(
      grpc::ClientContext& client_context,
      gcsa::GetDatabaseDdlRequest const& request) override {
    gcsa::GetDatabaseDdlResponse response;
    auto status =
        database_admin_->GetDatabaseDdl(&client_context, request, &response);
    if (!status.ok()) {
      return google::cloud::MakeStatusFromRpcError(status);
    }
    return response;
  }

  StatusOr<google::longrunning::Operation> UpdateDatabase(
      grpc::ClientContext& context,
      google::spanner::admin::database::v1::UpdateDatabaseDdlRequest const&
          request) override {
    google::longrunning::Operation response;
    grpc::Status status =
        database_admin_->UpdateDatabaseDdl(&context, request, &response);
    if (!status.ok()) {
      return google::cloud::MakeStatusFromRpcError(status);
    }
    return response;
  }

  Status DropDatabase(grpc::ClientContext& client_context,
                      gcsa::DropDatabaseRequest const& request) override {
    google::protobuf::Empty response;
    grpc::Status status =
        database_admin_->DropDatabase(&client_context, request, &response);
    if (!status.ok()) {
      return google::cloud::MakeStatusFromRpcError(status);
    }
    return google::cloud::Status();
  }

  StatusOr<gcsa::ListDatabasesResponse> ListDatabases(
      grpc::ClientContext& client_context,
      gcsa::ListDatabasesRequest const& request) override {
    gcsa::ListDatabasesResponse response;
    auto status =
        database_admin_->ListDatabases(&client_context, request, &response);
    if (!status.ok()) {
      return google::cloud::MakeStatusFromRpcError(status);
    }
    return response;
  }

  StatusOr<google::longrunning::Operation> RestoreDatabase(
      grpc::ClientContext& client_context,
      gcsa::RestoreDatabaseRequest const& request) override {
    google::longrunning::Operation response;
    grpc::Status status =
        database_admin_->RestoreDatabase(&client_context, request, &response);
    if (!status.ok()) {
      return google::cloud::MakeStatusFromRpcError(status);
    }
    return response;
  }

  StatusOr<google::iam::v1::Policy> GetIamPolicy(
      grpc::ClientContext& client_context,
      google::iam::v1::GetIamPolicyRequest const& request) override {
    google::iam::v1::Policy response;
    auto status =
        database_admin_->GetIamPolicy(&client_context, request, &response);
    if (!status.ok()) {
      return google::cloud::MakeStatusFromRpcError(status);
    }
    return response;
  }

  StatusOr<google::iam::v1::Policy> SetIamPolicy(
      grpc::ClientContext& client_context,
      google::iam::v1::SetIamPolicyRequest const& request) override {
    google::iam::v1::Policy response;
    auto status =
        database_admin_->SetIamPolicy(&client_context, request, &response);
    if (!status.ok()) {
      return google::cloud::MakeStatusFromRpcError(status);
    }
    return response;
  }

  StatusOr<google::iam::v1::TestIamPermissionsResponse> TestIamPermissions(
      grpc::ClientContext& client_context,
      google::iam::v1::TestIamPermissionsRequest const& request) override {
    google::iam::v1::TestIamPermissionsResponse response;
    auto status = database_admin_->TestIamPermissions(&client_context, request,
                                                      &response);
    if (!status.ok()) {
      return google::cloud::MakeStatusFromRpcError(status);
    }
    return response;
  }

  StatusOr<google::longrunning::Operation> CreateBackup(
      grpc::ClientContext& client_context,
      google::spanner::admin::database::v1::CreateBackupRequest const& request)
      override {
    google::longrunning::Operation response;
    auto status =
        database_admin_->CreateBackup(&client_context, request, &response);
    if (!status.ok()) {
      return google::cloud::MakeStatusFromRpcError(status);
    }
    return response;
  }

  StatusOr<google::spanner::admin::database::v1::Backup> GetBackup(
      grpc::ClientContext& client_context,
      google::spanner::admin::database::v1::GetBackupRequest const& request)
      override {
    google::spanner::admin::database::v1::Backup response;
    auto status =
        database_admin_->GetBackup(&client_context, request, &response);
    if (!status.ok()) {
      return google::cloud::MakeStatusFromRpcError(status);
    }
    return response;
  }

  Status DeleteBackup(grpc::ClientContext& client_context,
                      gcsa::DeleteBackupRequest const& request) override {
    google::protobuf::Empty response;
    grpc::Status status =
        database_admin_->DeleteBackup(&client_context, request, &response);
    if (!status.ok()) {
      return google::cloud::MakeStatusFromRpcError(status);
    }
    return google::cloud::Status();
  }

  StatusOr<gcsa::ListBackupsResponse> ListBackups(
      grpc::ClientContext& client_context,
      gcsa::ListBackupsRequest const& request) override {
    gcsa::ListBackupsResponse response;
    auto status =
        database_admin_->ListBackups(&client_context, request, &response);
    if (!status.ok()) {
      return google::cloud::MakeStatusFromRpcError(status);
    }
    return response;
  }

  StatusOr<google::spanner::admin::database::v1::Backup> UpdateBackup(
      grpc::ClientContext& client_context,
      google::spanner::admin::database::v1::UpdateBackupRequest const& request)
      override {
    google::spanner::admin::database::v1::Backup response;
    auto status =
        database_admin_->UpdateBackup(&client_context, request, &response);
    if (!status.ok()) {
      return google::cloud::MakeStatusFromRpcError(status);
    }
    return response;
  }

  StatusOr<gcsa::ListBackupOperationsResponse> ListBackupOperations(
      grpc::ClientContext& client_context,
      gcsa::ListBackupOperationsRequest const& request) override {
    gcsa::ListBackupOperationsResponse response;
    auto status = database_admin_->ListBackupOperations(&client_context,
                                                        request, &response);
    if (!status.ok()) {
      return google::cloud::MakeStatusFromRpcError(status);
    }
    return response;
  }

  StatusOr<gcsa::ListDatabaseOperationsResponse> ListDatabaseOperations(
      grpc::ClientContext& client_context,
      gcsa::ListDatabaseOperationsRequest const& request) override {
    gcsa::ListDatabaseOperationsResponse response;
    auto status = database_admin_->ListDatabaseOperations(&client_context,
                                                          request, &response);
    if (!status.ok()) {
      return google::cloud::MakeStatusFromRpcError(status);
    }
    return response;
  }

  StatusOr<google::longrunning::Operation> GetOperation(
      grpc::ClientContext& client_context,
      google::longrunning::GetOperationRequest const& request) override {
    google::longrunning::Operation response;
    grpc::Status status =
        operations_->GetOperation(&client_context, request, &response);
    if (!status.ok()) {
      return google::cloud::MakeStatusFromRpcError(status);
    }
    return response;
  }

  Status CancelOperation(
      grpc::ClientContext& client_context,
      google::longrunning::CancelOperationRequest const& request) override {
    google::protobuf::Empty response;
    grpc::Status status =
        operations_->CancelOperation(&client_context, request, &response);
    if (!status.ok()) {
      return google::cloud::MakeStatusFromRpcError(status);
    }
    return google::cloud::Status();
  }

 private:
  std::unique_ptr<gcsa::DatabaseAdmin::Stub> database_admin_;
  std::unique_ptr<google::longrunning::Operations::Stub> operations_;
};

std::shared_ptr<DatabaseAdminStub> CreateDefaultDatabaseAdminStub(
    ConnectionOptions options) {
  options = internal::EmulatorOverrides(std::move(options));
  auto channel =
      grpc::CreateCustomChannel(options.endpoint(), options.credentials(),
                                options.CreateChannelArguments());
  auto spanner_grpc_stub = gcsa::DatabaseAdmin::NewStub(channel);
  auto longrunning_grpc_stub =
      google::longrunning::Operations::NewStub(channel);

  std::shared_ptr<DatabaseAdminStub> stub =
      std::make_shared<DefaultDatabaseAdminStub>(
          std::move(spanner_grpc_stub), std::move(longrunning_grpc_stub));

  stub = std::make_shared<DatabaseAdminMetadata>(std::move(stub));

  if (options.tracing_enabled("rpc")) {
    GCP_LOG(INFO) << "Enabled logging for gRPC calls";
    stub = std::make_shared<DatabaseAdminLogging>(std::move(stub),
                                                  options.tracing_options());
  }
  return stub;
}

}  // namespace internal
}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner
}  // namespace cloud
}  // namespace google
