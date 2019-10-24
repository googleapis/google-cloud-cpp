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
#include "google/cloud/spanner/internal/session_pool.h"
#include "google/cloud/spanner/internal/spanner_stub.h"
#include "google/cloud/spanner/retry_policy.h"
#include "google/cloud/spanner/version.h"
#include "google/cloud/status.h"
#include "google/cloud/status_or.h"
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

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
 * Factory method to construct a `ConnectionImpl`.
 *
 * @note `ConnectionImpl` relies on `std::enable_shared_from_this`; the
 * factory method ensures it can only be constructed using a `std::shared_ptr`
 *
 * @note In tests we can use a mock stub and custom (or mock) policies.
 */
class ConnectionImpl;
std::shared_ptr<ConnectionImpl> MakeConnection(
    Database db, std::shared_ptr<SpannerStub> stub,
    std::unique_ptr<RetryPolicy> retry_policy = DefaultConnectionRetryPolicy(),
    std::unique_ptr<BackoffPolicy> backoff_policy =
        DefaultConnectionBackoffPolicy());

/**
 * A concrete `Connection` subclass that uses gRPC to actually talk to a real
 * Spanner instance. See `MakeConnection()` for a factory function that creates
 * and returns instances of this class.
 */
class ConnectionImpl : public Connection,
                       public SessionManager,
                       public std::enable_shared_from_this<ConnectionImpl> {
 public:
  RowStream Read(ReadParams) override;
  StatusOr<std::vector<ReadPartition>> PartitionRead(
      PartitionReadParams) override;
  RowStream ExecuteQuery(ExecuteSqlParams) override;
  StatusOr<DmlResult> ExecuteDml(ExecuteSqlParams) override;
  ProfileQueryResult ProfileQuery(ExecuteSqlParams) override;
  StatusOr<ProfileDmlResult> ProfileDml(ExecuteSqlParams) override;
  StatusOr<ExecutionPlan> AnalyzeSql(ExecuteSqlParams) override;
  StatusOr<PartitionedDmlResult> ExecutePartitionedDml(
      ExecutePartitionedDmlParams) override;
  StatusOr<std::vector<QueryPartition>> PartitionQuery(
      PartitionQueryParams) override;
  StatusOr<BatchDmlResult> ExecuteBatchDml(ExecuteBatchDmlParams) override;
  StatusOr<CommitResult> Commit(CommitParams) override;
  Status Rollback(RollbackParams) override;

 private:
  // Only the factory method can construct instances of this class.
  friend std::shared_ptr<ConnectionImpl> MakeConnection(
      Database, std::shared_ptr<SpannerStub>, std::unique_ptr<RetryPolicy>,
      std::unique_ptr<BackoffPolicy>);
  ConnectionImpl(Database db, std::shared_ptr<SpannerStub> stub,
                 std::unique_ptr<RetryPolicy> retry_policy,
                 std::unique_ptr<BackoffPolicy> backoff_policy);

  RowStream ReadImpl(SessionHolder& session,
                     google::spanner::v1::TransactionSelector& s,
                     ReadParams params);

  StatusOr<std::vector<ReadPartition>> PartitionReadImpl(
      SessionHolder& session, google::spanner::v1::TransactionSelector& s,
      ReadParams const& params, PartitionOptions partition_options);

  RowStream ExecuteQueryImpl(SessionHolder& session,
                             google::spanner::v1::TransactionSelector& s,
                             std::int64_t seqno, ExecuteSqlParams params);

  StatusOr<DmlResult> ExecuteDmlImpl(
      SessionHolder& session, google::spanner::v1::TransactionSelector& s,
      std::int64_t seqno, ExecuteSqlParams params);

  ProfileQueryResult ProfileQueryImpl(
      SessionHolder& session, google::spanner::v1::TransactionSelector& s,
      std::int64_t seqno, ExecuteSqlParams params);

  StatusOr<ProfileDmlResult> ProfileDmlImpl(
      SessionHolder& session, google::spanner::v1::TransactionSelector& s,
      std::int64_t seqno, ExecuteSqlParams params);

  StatusOr<ExecutionPlan> AnalyzeSqlImpl(
      SessionHolder& session, google::spanner::v1::TransactionSelector& s,
      std::int64_t seqno, ExecuteSqlParams params);

  StatusOr<PartitionedDmlResult> ExecutePartitionedDmlImpl(
      SessionHolder& session, google::spanner::v1::TransactionSelector& s,
      std::int64_t seqno, ExecutePartitionedDmlParams params);

  StatusOr<std::vector<QueryPartition>> PartitionQueryImpl(
      SessionHolder& session, google::spanner::v1::TransactionSelector& s,
      ExecuteSqlParams const& params, PartitionOptions partition_options);

  StatusOr<BatchDmlResult> ExecuteBatchDmlImpl(
      SessionHolder& session, google::spanner::v1::TransactionSelector& s,
      std::int64_t seqno, ExecuteBatchDmlParams params);

  StatusOr<CommitResult> CommitImpl(SessionHolder& session,
                                    google::spanner::v1::TransactionSelector& s,
                                    CommitParams params);

  Status RollbackImpl(SessionHolder& session,
                      google::spanner::v1::TransactionSelector& s);

  /**
   * Get a session from the pool, or create one if the pool is empty.
   * @returns an error if session creation fails; always returns a valid
   * `SessionHolder` (never `nullptr`) on success.
   *
   * The `SessionHolder` usually returns the session to the pool when it is
   * destroyed. However, if `dissociate_from_pool` is true the session will not
   * be returned to the session pool. This is used in partitioned operations,
   * since we don't know when all parties are done using the session.
   */
  StatusOr<SessionHolder> AllocateSession(bool dissociate_from_pool = false);

  // Forwards calls for the `SessionPool`; used in the `SessionHolder` deleter
  // so it can hold a `weak_ptr` to `ConnectionImpl` (it's already reference
  // counted, and manages the lifetime of `SessionPool`).
  void ReleaseSession(Session* session) {
    session_pool_.Release(std::unique_ptr<Session>(session));
  }

  // `SessionManager` methods; used by the `SessionPool`
  StatusOr<std::vector<std::unique_ptr<Session>>> CreateSessions(
      int num_sessions) override;

  template <typename ResultType>
  StatusOr<ResultType> ExecuteSqlImpl(
      SessionHolder& session, google::spanner::v1::TransactionSelector& s,
      std::int64_t seqno, ExecuteSqlParams params,
      google::spanner::v1::ExecuteSqlRequest::QueryMode query_mode,
      std::function<StatusOr<std::unique_ptr<ResultSourceInterface>>(
          google::spanner::v1 ::ExecuteSqlRequest& request)> const&
          retry_resume_fn);

  template <typename ResultType>
  ResultType CommonQueryImpl(
      SessionHolder& session, google::spanner::v1::TransactionSelector& s,
      std::int64_t seqno, ExecuteSqlParams params,
      google::spanner::v1::ExecuteSqlRequest::QueryMode query_mode);

  template <typename ResultType>
  StatusOr<ResultType> CommonDmlImpl(
      SessionHolder& session, google::spanner::v1::TransactionSelector& s,
      std::int64_t seqno, ExecuteSqlParams params,
      google::spanner::v1::ExecuteSqlRequest::QueryMode query_mode);

  Database db_;
  std::shared_ptr<SpannerStub> stub_;
  std::shared_ptr<RetryPolicy> retry_policy_;
  std::shared_ptr<BackoffPolicy> backoff_policy_;
  SessionPool session_pool_;
};

}  // namespace internal
}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_SPANNER_GOOGLE_CLOUD_SPANNER_INTERNAL_CONNECTION_IMPL_H_
