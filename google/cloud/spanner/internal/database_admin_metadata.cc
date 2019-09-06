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

#include "google/cloud/spanner/internal/database_admin_metadata.h"
#include "google/cloud/spanner/internal/api_client_header.h"

namespace google {
namespace cloud {
namespace spanner {
inline namespace SPANNER_CLIENT_NS {
namespace internal {

namespace gcsa = google::spanner::admin::database::v1;

DatabaseAdminMetadata::DatabaseAdminMetadata(
    std::shared_ptr<DatabaseAdminStub> child)
    : child_(std::move(child)), api_client_header_(ApiClientHeader()) {}

StatusOr<google::longrunning::Operation> DatabaseAdminMetadata::CreateDatabase(
    grpc::ClientContext& context, gcsa::CreateDatabaseRequest const& request) {
  SetMetadata(context, "parent=" + request.parent());
  return child_->CreateDatabase(context, request);
}

future<StatusOr<gcsa::Database>> DatabaseAdminMetadata::AwaitCreateDatabase(
    google::longrunning::Operation operation) {
  return child_->AwaitCreateDatabase(std::move(operation));
}

StatusOr<google::longrunning::Operation> DatabaseAdminMetadata::UpdateDatabase(
    grpc::ClientContext& context,
    google::spanner::admin::database::v1::UpdateDatabaseDdlRequest const&
        request) {
  SetMetadata(context, "database=" + request.database());
  return child_->UpdateDatabase(context, request);
}

future<StatusOr<gcsa::UpdateDatabaseDdlMetadata>>
DatabaseAdminMetadata::AwaitUpdateDatabase(
    google::longrunning::Operation operation) {
  return child_->AwaitUpdateDatabase(std::move(operation));
}

Status DatabaseAdminMetadata::DropDatabase(
    grpc::ClientContext& context,
    google::spanner::admin::database::v1::DropDatabaseRequest const& request) {
  SetMetadata(context, "database=" + request.database());
  return child_->DropDatabase(context, request);
}

StatusOr<google::spanner::admin::database::v1::ListDatabasesResponse>
DatabaseAdminMetadata::ListDatabases(
    grpc::ClientContext& context,
    google::spanner::admin::database::v1::ListDatabasesRequest const& request) {
  SetMetadata(context, "parent=" + request.parent());
  return child_->ListDatabases(context, request);
}

StatusOr<google::longrunning::Operation> DatabaseAdminMetadata::GetOperation(
    grpc::ClientContext& context,
    google::longrunning::GetOperationRequest const& request) {
  SetMetadata(context, "name=" + request.name());
  return child_->GetOperation(context, request);
}

void DatabaseAdminMetadata::SetMetadata(grpc::ClientContext& context,
                                        std::string const& request_params) {
  context.AddMetadata("x-goog-request-params", request_params);
  context.AddMetadata("x-goog-api-client", api_client_header_);
}

}  // namespace internal
}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner
}  // namespace cloud
}  // namespace google
