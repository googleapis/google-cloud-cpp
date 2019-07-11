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
#include "google/cloud/spanner/internal/polling_loop.h"
#include "google/cloud/spanner/internal/retry_loop.h"
#include "google/cloud/grpc_utils/grpc_error_delegate.h"

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

#ifndef GOOGLE_CLOUD_CPP_SPANNER_ADMIN_DEFAULT_POLLING_TIMEOUT
#define GOOGLE_CLOUD_CPP_SPANNER_ADMIN_DEFAULT_POLLING_TIMEOUT \
  std::chrono::minutes(30)
#endif  // GOOGLE_CLOUD_CPP_SPANNER_ADMIN_DEFAULT_POLLING_TIMEOUT

#ifndef GOOGLE_CLOUD_CPP_SPANNER_ADMIN_DEFAULT_POLLING_INITIAL_BACKOFF
#define GOOGLE_CLOUD_CPP_SPANNER_ADMIN_DEFAULT_POLLING_INITIAL_BACKOFF \
  std::chrono::seconds(10)
#endif  // GOOGLE_CLOUD_CPP_SPANNER_ADMIN_DEFAULT_POLLING_INITIAL_BACKOFF

#ifndef GOOGLE_CLOUD_CPP_SPANNER_ADMIN_DEFAULT_POLLING_MAXIMUM_BACKOFF
#define GOOGLE_CLOUD_CPP_SPANNER_ADMIN_DEFAULT_POLLING_MAXIMUM_BACKOFF \
  std::chrono::minutes(5)
#endif  // GOOGLE_CLOUD_CPP_SPANNER_ADMIN_DEFAULT_POLLING_MAXIMUM_BACKOFF

#ifndef GOOGLE_CLOUD_CPP_SPANNER_ADMIN_DEFAULT_POLLING_BACKOFF_SCALING
#define GOOGLE_CLOUD_CPP_SPANNER_ADMIN_DEFAULT_POLLING_BACKOFF_SCALING 2.0
#endif  // GOOGLE_CLOUD_CPP_SPANNER_ADMIN_DEFAULT_POLLING_BACKOFF_SCALING

std::unique_ptr<PollingPolicy> DefaultAdminPollingPolicy() {
  return GenericPollingPolicy<>(
             LimitedTimeRetryPolicy(
                 GOOGLE_CLOUD_CPP_SPANNER_ADMIN_DEFAULT_POLLING_TIMEOUT),
             ExponentialBackoffPolicy(
                 GOOGLE_CLOUD_CPP_SPANNER_ADMIN_DEFAULT_POLLING_INITIAL_BACKOFF,
                 GOOGLE_CLOUD_CPP_SPANNER_ADMIN_DEFAULT_POLLING_MAXIMUM_BACKOFF,
                 GOOGLE_CLOUD_CPP_SPANNER_ADMIN_DEFAULT_POLLING_BACKOFF_SCALING))
      .clone();
}

DatabaseAdminRetry::DatabaseAdminRetry(PrivateConstructorTag,
                                       std::shared_ptr<DatabaseAdminStub> child)
    : child_(std::move(child)),
      retry_policy_(DefaultAdminRetryPolicy()),
      backoff_policy_(DefaultAdminBackoffPolicy()),
      polling_policy_(DefaultAdminPollingPolicy()) {}

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

future<StatusOr<gcsa::Database>> DatabaseAdminRetry::AwaitCreateDatabase(
    google::longrunning::Operation operation) {
  promise<StatusOr<gcsa::Database>> promise;
  auto f = promise.get_future();

  // TODO(#127) - use the (implicit) completion queue to run this loop.
  std::thread t(
      [](std::shared_ptr<internal::DatabaseAdminStub> stub,
         google::longrunning::Operation operation,
         std::unique_ptr<PollingPolicy> polling_policy,
         google::cloud::promise<StatusOr<gcsa::Database>> promise,
         char const* location) mutable {
        auto result = internal::PollingLoop<gcsa::Database>(
            std::move(polling_policy),
            [stub](grpc::ClientContext& context,
                   google::longrunning::GetOperationRequest const& request) {
              return stub->GetOperation(context, request);
            },
            std::move(operation), location);

        // Release the stub before signalling the promise, this works around a
        // peculiar behavior of googlemock: if the stub is a mock, it should be
        // deleted before the test function returns. Otherwise the mock is
        // declared "leaked" even if it is released later. Holding on to the
        // stub here could extend its lifetime beyond the test function, as the
        // detached thread may take a bit to terminate.
        stub.reset();
        promise.set_value(std::move(result));
      },
      child_, std::move(operation), polling_policy_->clone(),
      std::move(promise), __func__);
  t.detach();

  return f;
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
