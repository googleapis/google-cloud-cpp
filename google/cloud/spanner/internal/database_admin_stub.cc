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
#include "google/cloud/spanner/internal/database_admin_retry.h"
#include "google/cloud/grpc_utils/grpc_error_delegate.h"
#include "google/cloud/log.h"
#include <google/longrunning/operations.grpc.pb.h>

namespace google {
namespace cloud {
namespace spanner {
inline namespace SPANNER_CLIENT_NS {
namespace internal {

namespace gcsa = google::spanner::admin::database::v1;

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
      return google::cloud::grpc_utils::MakeStatusFromRpcError(status);
    }
    return response;
  }

  future<StatusOr<gcsa::Database>> AwaitCreateDatabase(
      google::longrunning::Operation) override {
    return make_ready_future(
        StatusOr<gcsa::Database>(Status(StatusCode::kUnimplemented, __func__)));
  }

  StatusOr<google::longrunning::Operation> UpdateDatabase(
      grpc::ClientContext& context,
      google::spanner::admin::database::v1::UpdateDatabaseDdlRequest const&
          request) override {
    google::longrunning::Operation response;
    grpc::Status status =
        database_admin_->UpdateDatabaseDdl(&context, request, &response);
    if (!status.ok()) {
      return google::cloud::grpc_utils::MakeStatusFromRpcError(status);
    }
    return response;
  }

  future<StatusOr<gcsa::UpdateDatabaseDdlMetadata>> AwaitUpdateDatabase(
      google::longrunning::Operation) override {
    return make_ready_future(StatusOr<gcsa::UpdateDatabaseDdlMetadata>(
        Status(StatusCode::kUnimplemented, __func__)));
  }

  Status DropDatabase(grpc::ClientContext& client_context,
                      gcsa::DropDatabaseRequest const& request) override {
    google::protobuf::Empty response;
    grpc::Status status =
        database_admin_->DropDatabase(&client_context, request, &response);
    if (!status.ok()) {
      return google::cloud::grpc_utils::MakeStatusFromRpcError(status);
    }
    return google::cloud::Status();
  }

  StatusOr<google::longrunning::Operation> GetOperation(
      grpc::ClientContext& client_context,
      google::longrunning::GetOperationRequest const& request) override {
    google::longrunning::Operation response;
    grpc::Status status =
        operations_->GetOperation(&client_context, request, &response);
    if (!status.ok()) {
      return google::cloud::grpc_utils::MakeStatusFromRpcError(status);
    }
    return response;
  }

 private:
  std::unique_ptr<gcsa::DatabaseAdmin::Stub> database_admin_;
  std::unique_ptr<google::longrunning::Operations::Stub> operations_;
};

std::shared_ptr<DatabaseAdminStub> CreateDefaultDatabaseAdminStub(
    ConnectionOptions const& options) {
  auto channel = grpc::CreateChannel(options.endpoint(), options.credentials());
  auto spanner_grpc_stub = gcsa::DatabaseAdmin::NewStub(channel);
  auto longrunning_grpc_stub =
      google::longrunning::Operations::NewStub(channel);

  std::shared_ptr<DatabaseAdminStub> stub =
      std::make_shared<DefaultDatabaseAdminStub>(
          std::move(spanner_grpc_stub), std::move(longrunning_grpc_stub));

  if (options.tracing_enabled("rpc")) {
    GCP_LOG(INFO) << "Enabled logging for gRPC calls";
    stub = std::make_shared<DatabaseAdminLogging>(std::move(stub));
  }
  return std::make_shared<internal::DatabaseAdminRetry>(std::move(stub));
}

}  // namespace internal
}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner
}  // namespace cloud
}  // namespace google
