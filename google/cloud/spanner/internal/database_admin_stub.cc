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
#include "google/cloud/bigtable/internal/grpc_error_delegate.h"
#include <google/longrunning/operations.grpc.pb.h>

namespace google {
namespace cloud {
namespace spanner {
inline namespace SPANNER_CLIENT_NS {
namespace internal {

namespace gcsa = google::spanner::admin::database;

DatabaseAdminStub::~DatabaseAdminStub() = default;

StatusOr<google::longrunning::Operation> DatabaseAdminStub::CreateDatabase(
    grpc::ClientContext&, gcsa::v1::CreateDatabaseRequest const&) {
  return Status(StatusCode::kUnimplemented,
                std::string("Unimplemented stub for ") + __func__);
}

Status DatabaseAdminStub::DropDatabase(grpc::ClientContext&,
                                       gcsa::v1::DropDatabaseRequest const&) {
  return Status(StatusCode::kUnimplemented,
                std::string("Unimplemented stub for ") + __func__);
}

StatusOr<google::longrunning::Operation> DatabaseAdminStub::GetOperation(
    grpc::ClientContext&, google::longrunning::GetOperationRequest const&) {
  return Status(StatusCode::kUnimplemented,
                std::string("Unimplemented stub for ") + __func__);
}

class DefaultDatabaseAdminStub : public DatabaseAdminStub {
 public:
  DefaultDatabaseAdminStub(
      std::unique_ptr<gcsa::v1::DatabaseAdmin::Stub> database_admin,
      std::unique_ptr<google::longrunning::Operations::Stub> operations)
      : database_admin_(std::move(database_admin)),
        operations_(std::move(operations)) {}

  ~DefaultDatabaseAdminStub() override = default;

  /// Start the long-running operation to create a new Cloud Spanner
  /// database.
  StatusOr<google::longrunning::Operation> CreateDatabase(
      grpc::ClientContext& client_context,
      gcsa::v1::CreateDatabaseRequest const& request) override {
    google::longrunning::Operation response;
    grpc::Status status =
        database_admin_->CreateDatabase(&client_context, request, &response);
    if (!status.ok()) {
      return google::cloud::bigtable::internal::MakeStatusFromRpcError(status);
    }
    return response;
  }

  /// Drop an existing Cloud Spanner database.
  Status DropDatabase(grpc::ClientContext& client_context,
                      gcsa::v1::DropDatabaseRequest const& request) override {
    google::protobuf::Empty response;
    grpc::Status status =
        database_admin_->DropDatabase(&client_context, request, &response);
    if (!status.ok()) {
      return google::cloud::bigtable::internal::MakeStatusFromRpcError(status);
    }
    return google::cloud::Status();
  }

  /// Poll a long-running operation.
  StatusOr<google::longrunning::Operation> GetOperation(
      grpc::ClientContext& client_context,
      google::longrunning::GetOperationRequest const& request) override {
    google::longrunning::Operation response;
    grpc::Status status =
        operations_->GetOperation(&client_context, request, &response);
    if (!status.ok()) {
      return google::cloud::bigtable::internal::MakeStatusFromRpcError(status);
    }
    return response;
  }

 private:
  std::unique_ptr<gcsa::v1::DatabaseAdmin::Stub> database_admin_;
  std::unique_ptr<google::longrunning::Operations::Stub> operations_;
};

std::shared_ptr<DatabaseAdminStub> CreateDefaultDatabaseAdminStub(
    ClientOptions const& options) {
  auto channel =
      grpc::CreateChannel(options.admin_endpoint(), options.credentials());
  auto spanner_grpc_stub = gcsa::v1::DatabaseAdmin::NewStub(channel);
  auto longrunning_grpc_stub =
      google::longrunning::Operations::NewStub(channel);

  return std::make_shared<DefaultDatabaseAdminStub>(
      std::move(spanner_grpc_stub), std::move(longrunning_grpc_stub));
}

}  // namespace internal
}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner
}  // namespace cloud
}  // namespace google
