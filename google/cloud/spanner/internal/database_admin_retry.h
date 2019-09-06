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

#ifndef GOOGLE_CLOUD_CPP_SPANNER_GOOGLE_CLOUD_SPANNER_INTERNAL_DATABASE_ADMIN_RETRY_H_
#define GOOGLE_CLOUD_CPP_SPANNER_GOOGLE_CLOUD_SPANNER_INTERNAL_DATABASE_ADMIN_RETRY_H_

#include "google/cloud/spanner/backoff_policy.h"
#include "google/cloud/spanner/internal/database_admin_stub.h"
#include "google/cloud/spanner/polling_policy.h"
#include "google/cloud/spanner/retry_policy.h"

namespace google {
namespace cloud {
namespace spanner {
inline namespace SPANNER_CLIENT_NS {
namespace internal {
/**
 * Implements the retry Decorator for DatabaseAdminStub.
 */
class DatabaseAdminRetry : public DatabaseAdminStub {
 public:
  template <typename... PolicyOverrides>
  explicit DatabaseAdminRetry(std::shared_ptr<DatabaseAdminStub> child,
                              PolicyOverrides&&... policies)
      : DatabaseAdminRetry(PrivateConstructorTag{}, std::move(child)) {
    OverridePolicies(std::forward<PolicyOverrides>(policies)...);
  }

  ~DatabaseAdminRetry() override = default;

  //@{
  /**
   * @name Override the functions from `DatabaseAdminStub`.
   *
   * Run the retry loop (if appropriate) for the child DatabaseAdminStub.
   */
  ///
  StatusOr<google::longrunning::Operation> CreateDatabase(
      grpc::ClientContext& context,
      google::spanner::admin::database::v1::CreateDatabaseRequest const&
          request) override;

  future<StatusOr<google::spanner::admin::database::v1::Database>>
      AwaitCreateDatabase(google::longrunning::Operation) override;

  StatusOr<google::spanner::admin::database::v1::Database> GetDatabase(
      grpc::ClientContext&,
      google::spanner::admin::database::v1::GetDatabaseRequest const&) override;

  StatusOr<google::longrunning::Operation> UpdateDatabase(
      grpc::ClientContext& context,
      google::spanner::admin::database::v1::UpdateDatabaseDdlRequest const&
          request) override;

  future<
      StatusOr<google::spanner::admin::database::v1::UpdateDatabaseDdlMetadata>>
  AwaitUpdateDatabase(google::longrunning::Operation operation) override;

  Status DropDatabase(
      grpc::ClientContext& context,
      google::spanner::admin::database::v1::DropDatabaseRequest const& request)
      override;

  StatusOr<google::spanner::admin::database::v1::ListDatabasesResponse>
  ListDatabases(
      grpc::ClientContext&,
      google::spanner::admin::database::v1::ListDatabasesRequest const&)
      override;

  StatusOr<google::longrunning::Operation> GetOperation(
      grpc::ClientContext& context,
      google::longrunning::GetOperationRequest const& request) override;
  //@}

 private:
  void OverridePolicy(RetryPolicy const& p) { retry_policy_ = p.clone(); }
  void OverridePolicy(BackoffPolicy const& p) { backoff_policy_ = p.clone(); }
  void OverridePolicy(PollingPolicy const& p) { polling_policy_ = p.clone(); }
  void OverridePolicies() {}
  template <typename Policy, typename... Policies>
  void OverridePolicies(Policy&& p, Policies&&... policies) {
    OverridePolicy(std::forward<Policy>(p));
    OverridePolicies(std::forward<Policies>(policies)...);
  }

  struct PrivateConstructorTag {};

  explicit DatabaseAdminRetry(PrivateConstructorTag,
                              std::shared_ptr<DatabaseAdminStub> child);

  std::shared_ptr<DatabaseAdminStub> child_;
  std::unique_ptr<RetryPolicy> retry_policy_;
  std::unique_ptr<BackoffPolicy> backoff_policy_;
  std::unique_ptr<PollingPolicy> polling_policy_;
};

}  // namespace internal
}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_SPANNER_GOOGLE_CLOUD_SPANNER_INTERNAL_DATABASE_ADMIN_RETRY_H_
