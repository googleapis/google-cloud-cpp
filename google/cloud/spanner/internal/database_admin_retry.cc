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

#include "google/cloud/spanner/internal/database_admin_retry.h"
#include "google/cloud/spanner/internal/retry_loop.h"

namespace google {
namespace cloud {
namespace spanner {
inline namespace SPANNER_CLIENT_NS {
namespace internal {

#ifndef GOOGLE_CLOUD_CPP_SPANNER_ADMIN_DEFAULT_RETRY_TIMEOUT
#define GOOGLE_CLOUD_CPP_SPANNER_ADMIN_DEFAULT_RETRY_TIMEOUT \
  std::chrono::minutes(30)
#endif  // GOOGLE_CLOUD_CPP_SPANNER_ADMIN_DEFAULT_RETRY_TIMEOUT

#ifndef GOOGLE_CLOUD_CPP_SPANNER_ADMIN_DEFAULT_INITIAL_BACKOFF
#define GOOGLE_CLOUD_CPP_SPANNER_ADMIN_DEFAULT_INITIAL_BACKOFF \
  std::chrono::seconds(1)
#endif  // GOOGLE_CLOUD_CPP_SPANNER_ADMIN_DEFAULT_INITIAL_BACKOFF

#ifndef GOOGLE_CLOUD_CPP_SPANNER_ADMIN_DEFAULT_MAXIMUM_BACKOFF
#define GOOGLE_CLOUD_CPP_SPANNER_ADMIN_DEFAULT_MAXIMUM_BACKOFF \
  std::chrono::minutes(5)
#endif  // GOOGLE_CLOUD_CPP_SPANNER_ADMIN_DEFAULT_MAXIMUM_BACKOFF

#ifndef GOOGLE_CLOUD_CPP_SPANNER_ADMIN_DEFAULT_BACKOFF_SCALING
#define GOOGLE_CLOUD_CPP_SPANNER_ADMIN_DEFAULT_BACKOFF_SCALING 2.0
#endif  // GOOGLE_CLOUD_CPP_SPANNER_ADMIN_DEFAULT_BACKOFF_SCALING

std::unique_ptr<RetryPolicy> DefaultAdminRetryPolicy() {
  return google::cloud::spanner::LimitedTimeRetryPolicy(
             GOOGLE_CLOUD_CPP_SPANNER_ADMIN_DEFAULT_RETRY_TIMEOUT)
      .clone();
}

std::unique_ptr<BackoffPolicy> DefaultAdminBackoffPolicy() {
  return google::cloud::spanner::ExponentialBackoffPolicy(
             GOOGLE_CLOUD_CPP_SPANNER_ADMIN_DEFAULT_INITIAL_BACKOFF,
             GOOGLE_CLOUD_CPP_SPANNER_ADMIN_DEFAULT_MAXIMUM_BACKOFF,
             GOOGLE_CLOUD_CPP_SPANNER_ADMIN_DEFAULT_BACKOFF_SCALING)
      .clone();
}

DatabaseAdminRetry::DatabaseAdminRetry(PrivateConstructorTag,
                                       std::shared_ptr<DatabaseAdminStub> child)
    : child_(std::move(child)),
      retry_policy_(DefaultAdminRetryPolicy()),
      backoff_policy_(DefaultAdminBackoffPolicy()) {}

namespace gcsa = google::spanner::admin::database::v1;

StatusOr<google::longrunning::Operation> DatabaseAdminRetry::CreateDatabase(
    grpc::ClientContext& context, gcsa::CreateDatabaseRequest const& request) {
  return RetryLoop(
      retry_policy_->clone(), backoff_policy_->clone(), false,
      [this](grpc::ClientContext& context,
             gcsa::CreateDatabaseRequest const& request) {
        return child_->CreateDatabase(context, request);
      },
      context, request, __func__);
}

Status DatabaseAdminRetry::DropDatabase(
    grpc::ClientContext& context,
    google::spanner::admin::database::v1::DropDatabaseRequest const& request) {
  return RetryLoop(
      retry_policy_->clone(), backoff_policy_->clone(), true,
      [this](grpc::ClientContext& context,
             gcsa::DropDatabaseRequest const& request) {
        return child_->DropDatabase(context, request);
      },
      context, request, __func__);
}

StatusOr<google::longrunning::Operation> DatabaseAdminRetry::GetOperation(
    grpc::ClientContext& context,
    google::longrunning::GetOperationRequest const& request) {
  // No retry because this function is typically wrapped by a polling loop.
  return child_->GetOperation(context, request);
}

}  // namespace internal
}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner
}  // namespace cloud
}  // namespace google
