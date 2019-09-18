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

#ifndef GOOGLE_CLOUD_CPP_SPANNER_GOOGLE_CLOUD_SPANNER_INTERNAL_CONNECTION_IMPL_H_
#define GOOGLE_CLOUD_CPP_SPANNER_GOOGLE_CLOUD_SPANNER_INTERNAL_CONNECTION_IMPL_H_

#include "google/cloud/spanner/backoff_policy.h"
#include "google/cloud/spanner/connection.h"
#include "google/cloud/spanner/database.h"
#include "google/cloud/spanner/internal/session.h"
#include "google/cloud/spanner/internal/spanner_stub.h"
#include "google/cloud/spanner/retry_policy.h"
#include "google/cloud/spanner/version.h"
#include "google/cloud/status.h"
#include "google/cloud/status_or.h"
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>

namespace google {
namespace cloud {
namespace spanner {
inline namespace SPANNER_CLIENT_NS {
namespace internal {

/// Return the default retry policy for `ConnectionImpl`
std::unique_ptr<RetryPolicy> DefaultConnectionRetryPolicy();

/// Return the default backoff policy for `ConnectionImpl`
std::unique_ptr<BackoffPolicy> DefaultConnectionBackoffPolicy();

/**
 * A concrete `Connection` subclass that uses gRPC to actually talk to a real
 * Spanner instance. See `MakeConnection()` for a factory function that creates
 * and returns instances of this class.
 */
class ConnectionImpl : public Connection,
                       public std::enable_shared_from_this<ConnectionImpl> {
 public:
  /**
   * A constructor that sets the retry abd backoff policies.
   *
   * @note In tests we can use a mock stub and custom (or mock) policies.
   */
  ConnectionImpl(Database db, std::shared_ptr<internal::SpannerStub> stub,
                 std::unique_ptr<RetryPolicy> retry_policy,
                 std::unique_ptr<BackoffPolicy> backoff_policy)
      : db_(std::move(db)),
        stub_(std::move(stub)),
        retry_policy_(std::move(retry_policy)),
        backoff_policy_(std::move(backoff_policy)) {}

  /**
   * A constructor using the default retry and backoff policies.
   *
   * @note We can test this class by injecting in a mock `stub`.
   */
  ConnectionImpl(Database db, std::shared_ptr<internal::SpannerStub> stub)
      : ConnectionImpl(std::move(db), std::move(stub),
                       DefaultConnectionRetryPolicy(),
                       DefaultConnectionBackoffPolicy()) {}

  StatusOr<ResultSet> Read(ReadParams) override;
  StatusOr<std::vector<ReadPartition>> PartitionRead(
      PartitionReadParams) override;
  StatusOr<ResultSet> ExecuteSql(ExecuteSqlParams) override;
  StatusOr<PartitionedDmlResult> ExecutePartitionedDml(
      ExecutePartitionedDmlParams) override;
  StatusOr<std::vector<QueryPartition>> PartitionQuery(
      PartitionQueryParams) override;
  StatusOr<BatchDmlResult> ExecuteBatchDml(ExecuteBatchDmlParams) override;
  StatusOr<CommitResult> Commit(CommitParams) override;
  Status Rollback(RollbackParams) override;

 private:
  StatusOr<ResultSet> ReadImpl(SessionHolder& session,
                               google::spanner::v1::TransactionSelector& s,
                               ReadParams rp);

  StatusOr<std::vector<ReadPartition>> PartitionReadImpl(
      SessionHolder& session, google::spanner::v1::TransactionSelector& s,
      ReadParams const& rp, PartitionOptions partition_options);

  StatusOr<ResultSet> ExecuteSqlImpl(
      SessionHolder& session, google::spanner::v1::TransactionSelector& s,
      std::int64_t seqno, ExecuteSqlParams esp);

  StatusOr<PartitionedDmlResult> ExecutePartitionedDmlImpl(
      SessionHolder& session, google::spanner::v1::TransactionSelector& s,
      std::int64_t seqno, ExecutePartitionedDmlParams epdp);

  StatusOr<std::vector<QueryPartition>> PartitionQueryImpl(
      SessionHolder& session, google::spanner::v1::TransactionSelector& s,
      ExecuteSqlParams const& esp, PartitionOptions partition_options);

  StatusOr<BatchDmlResult> ExecuteBatchDmlImpl(
      SessionHolder& session, google::spanner::v1::TransactionSelector& s,
      std::int64_t seqno, ExecuteBatchDmlParams params);

  StatusOr<CommitResult> CommitImpl(SessionHolder& session,
                                    google::spanner::v1::TransactionSelector& s,
                                    CommitParams cp);

  Status RollbackImpl(SessionHolder& session,
                      google::spanner::v1::TransactionSelector& s);

  StatusOr<SessionHolder> GetSession(bool dissociate_from_pool = false);
  void ReleaseSession(Session* session);

  Database db_;
  std::shared_ptr<internal::SpannerStub> stub_;

  // The current session pool.
  // TODO(#307) - improve session refresh and expiration.
  std::mutex mu_;
  std::vector<std::unique_ptr<Session>> sessions_;  // GUARDED_BY(mu_)

  std::unique_ptr<RetryPolicy> retry_policy_;
  std::unique_ptr<BackoffPolicy> backoff_policy_;
};

}  // namespace internal
}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_SPANNER_GOOGLE_CLOUD_SPANNER_INTERNAL_CONNECTION_IMPL_H_
