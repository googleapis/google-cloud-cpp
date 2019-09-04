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
#include "google/cloud/spanner/internal/log_wrapper.h"

namespace google {
namespace cloud {
namespace spanner {
inline namespace SPANNER_CLIENT_NS {
namespace internal {

namespace gcsa = google::spanner::admin::database::v1;

StatusOr<google::longrunning::Operation> DatabaseAdminLogging::CreateDatabase(
    grpc::ClientContext& context, gcsa::CreateDatabaseRequest const& request) {
  return LogWrapper(
      [this](grpc::ClientContext& context,
             gcsa::CreateDatabaseRequest const& request) {
        return child_->CreateDatabase(context, request);
      },
      context, request, __func__);
}

future<StatusOr<gcsa::Database>> DatabaseAdminLogging::AwaitCreateDatabase(
    google::longrunning::Operation operation) {
  return LogWrapper(
      [this](google::longrunning::Operation operation) {
        return child_->AwaitCreateDatabase(std::move(operation));
      },
      std::move(operation), __func__);
}

StatusOr<google::longrunning::Operation> DatabaseAdminLogging::UpdateDatabase(
    grpc::ClientContext& context,
    google::spanner::admin::database::v1::UpdateDatabaseDdlRequest const&
        request) {
  return LogWrapper(
      [this](grpc::ClientContext& context,
             gcsa::UpdateDatabaseDdlRequest const& request) {
        return child_->UpdateDatabase(context, request);
      },
      context, request, __func__);
}

future<StatusOr<gcsa::UpdateDatabaseDdlMetadata>>
DatabaseAdminLogging::AwaitUpdateDatabase(
    google::longrunning::Operation operation) {
  return LogWrapper(
      [this](google::longrunning::Operation operation) {
        return child_->AwaitUpdateDatabase(std::move(operation));
      },
      std::move(operation), __func__);
}

Status DatabaseAdminLogging::DropDatabase(
    grpc::ClientContext& context,
    google::spanner::admin::database::v1::DropDatabaseRequest const& request) {
  return LogWrapper(
      [this](grpc::ClientContext& context,
             gcsa::DropDatabaseRequest const& request) {
        return child_->DropDatabase(context, request);
      },
      context, request, __func__);
}

StatusOr<google::longrunning::Operation> DatabaseAdminLogging::GetOperation(
    grpc::ClientContext& context,
    google::longrunning::GetOperationRequest const& request) {
  return LogWrapper(
      [this](grpc::ClientContext& context,
             google::longrunning::GetOperationRequest const& request) {
        return child_->GetOperation(context, request);
      },
      context, request, __func__);
}

}  // namespace internal
}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner
}  // namespace cloud
}  // namespace google
