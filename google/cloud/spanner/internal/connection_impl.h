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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_INTERNAL_CONNECTION_IMPL_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_INTERNAL_CONNECTION_IMPL_H

#include "google/cloud/spanner/backoff_policy.h"
#include "google/cloud/spanner/connection.h"
#include "google/cloud/spanner/database.h"
#include "google/cloud/spanner/internal/session.h"
#include "google/cloud/spanner/internal/session_pool.h"
#include "google/cloud/spanner/internal/spanner_stub.h"
#include "google/cloud/spanner/retry_policy.h"
#include "google/cloud/spanner/tracing_options.h"
#include "google/cloud/spanner/version.h"
#include "google/cloud/background_threads.h"
#include "google/cloud/status.h"
#include "google/cloud/status_or.h"
#include <google/spanner/v1/spanner.pb.h>
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
 * @note In tests we can use mock stubs and custom (or mock) policies.
 */
class ConnectionImpl;
std::shared_ptr<ConnectionImpl> MakeConnection(
    Database db, std::vector<std::shared_ptr<SpannerStub>> stubs,
    ConnectionOptions const& options = ConnectionOptions{},
    SessionPoolOptions session_pool_options = SessionPoolOptions{},
    std::unique_ptr<RetryPolicy> retry_policy = DefaultConnectionRetryPolicy(),
    std::unique_ptr<BackoffPolicy> backoff_policy =
        DefaultConnectionBackoffPolicy());

/**
 * A concrete `Connection` subclass that uses gRPC to actually talk to a real
 * Spanner instance. See `MakeConnection()` for a factory function that creates
 * and returns instances of this class.
 */
class ConnectionImpl : public Connection {
 public:
  RowStream Read(ReadParams) override;
  StatusOr<std::vector<ReadPartition>> PartitionRead(
      PartitionReadParams) override;
  RowStream ExecuteQuery(SqlParams) override;
  StatusOr<DmlResult> ExecuteDml(SqlParams) override;
  ProfileQueryResult ProfileQuery(SqlParams) override;
  StatusOr<ProfileDmlResult> ProfileDml(SqlParams) override;
  StatusOr<ExecutionPlan> AnalyzeSql(SqlParams) override;
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
      Database, std::vector<std::shared_ptr<SpannerStub>>,
      ConnectionOptions const&, SessionPoolOptions,
      std::unique_ptr<RetryPolicy>, std::unique_ptr<BackoffPolicy>);
  ConnectionImpl(Database db, std::vector<std::shared_ptr<SpannerStub>> stubs,
                 ConnectionOptions const& options,
                 SessionPoolOptions session_pool_options,
                 std::unique_ptr<RetryPolicy> retry_policy,
                 std::unique_ptr<BackoffPolicy> backoff_policy);

  Status PrepareSession(SessionHolder& session,
                        bool dissociate_from_pool = false);

  RowStream ReadImpl(SessionHolder& session,
                     google::spanner::v1::TransactionSelector& s,
                     ReadParams params);

  StatusOr<std::vector<ReadPartition>> PartitionReadImpl(
      SessionHolder& session, google::spanner::v1::TransactionSelector& s,
      ReadParams const& params, PartitionOptions const& partition_options);

  RowStream ExecuteQueryImpl(SessionHolder& session,
                             google::spanner::v1::TransactionSelector& s,
                             std::int64_t seqno, SqlParams params);

  StatusOr<DmlResult> ExecuteDmlImpl(
      SessionHolder& session, google::spanner::v1::TransactionSelector& s,
      std::int64_t seqno, SqlParams params);

  ProfileQueryResult ProfileQueryImpl(
      SessionHolder& session, google::spanner::v1::TransactionSelector& s,
      std::int64_t seqno, SqlParams params);

  StatusOr<ProfileDmlResult> ProfileDmlImpl(
      SessionHolder& session, google::spanner::v1::TransactionSelector& s,
      std::int64_t seqno, SqlParams params);

  StatusOr<ExecutionPlan> AnalyzeSqlImpl(
      SessionHolder& session, google::spanner::v1::TransactionSelector& s,
      std::int64_t seqno, SqlParams params);

  StatusOr<PartitionedDmlResult> ExecutePartitionedDmlImpl(
      SessionHolder& session, google::spanner::v1::TransactionSelector& s,
      std::int64_t seqno, ExecutePartitionedDmlParams params);

  StatusOr<std::vector<QueryPartition>> PartitionQueryImpl(
      SessionHolder& session, google::spanner::v1::TransactionSelector& s,
      PartitionQueryParams const& params);

  StatusOr<BatchDmlResult> ExecuteBatchDmlImpl(
      SessionHolder& session, google::spanner::v1::TransactionSelector& s,
      std::int64_t seqno, ExecuteBatchDmlParams params);

  StatusOr<CommitResult> CommitImpl(SessionHolder& session,
                                    google::spanner::v1::TransactionSelector& s,
                                    CommitParams params);

  Status RollbackImpl(SessionHolder& session,
                      google::spanner::v1::TransactionSelector& s);

  template <typename ResultType>
  StatusOr<ResultType> ExecuteSqlImpl(
      SessionHolder& session, google::spanner::v1::TransactionSelector& s,
      std::int64_t seqno, SqlParams params,
      google::spanner::v1::ExecuteSqlRequest::QueryMode query_mode,
      std::function<StatusOr<std::unique_ptr<ResultSourceInterface>>(
          google::spanner::v1 ::ExecuteSqlRequest& request)> const&
          retry_resume_fn);

  template <typename ResultType>
  ResultType CommonQueryImpl(
      SessionHolder& session, google::spanner::v1::TransactionSelector& s,
      std::int64_t seqno, SqlParams params,
      google::spanner::v1::ExecuteSqlRequest::QueryMode query_mode);
  template <typename ResultType>
  StatusOr<ResultType> CommonDmlImpl(
      SessionHolder& session, google::spanner::v1::TransactionSelector& s,
      std::int64_t seqno, SqlParams params,
      google::spanner::v1::ExecuteSqlRequest::QueryMode query_mode);

  Database db_;
  std::shared_ptr<RetryPolicy const> retry_policy_prototype_;
  std::shared_ptr<BackoffPolicy const> backoff_policy_prototype_;
  std::unique_ptr<BackgroundThreads> background_threads_;
  std::shared_ptr<SessionPool> session_pool_;
  bool rpc_stream_tracing_enabled_ = false;
  TracingOptions tracing_options_;
};

}  // namespace internal
}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_INTERNAL_CONNECTION_IMPL_H
