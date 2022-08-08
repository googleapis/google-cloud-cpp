// Copyright 2019 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_INTERNAL_CONNECTION_IMPL_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_INTERNAL_CONNECTION_IMPL_H

#include "google/cloud/spanner/connection.h"
#include "google/cloud/spanner/database.h"
#include "google/cloud/spanner/internal/session.h"
#include "google/cloud/spanner/internal/session_pool.h"
#include "google/cloud/spanner/internal/spanner_stub.h"
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
namespace spanner_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

/**
 * A concrete `Connection` subclass that uses gRPC to actually talk to a real
 * Spanner instance.
 */
class ConnectionImpl : public spanner::Connection {
 public:
  ConnectionImpl(spanner::Database db,
                 std::unique_ptr<BackgroundThreads> background_threads,
                 std::vector<std::shared_ptr<SpannerStub>> stubs, Options opts);

  Options options() override { return opts_; }

  spanner::RowStream Read(ReadParams) override;
  StatusOr<std::vector<spanner::ReadPartition>> PartitionRead(
      PartitionReadParams) override;
  spanner::RowStream ExecuteQuery(SqlParams) override;
  StatusOr<spanner::DmlResult> ExecuteDml(SqlParams) override;
  spanner::ProfileQueryResult ProfileQuery(SqlParams) override;
  StatusOr<spanner::ProfileDmlResult> ProfileDml(SqlParams) override;
  StatusOr<spanner::ExecutionPlan> AnalyzeSql(SqlParams) override;
  StatusOr<spanner::PartitionedDmlResult> ExecutePartitionedDml(
      ExecutePartitionedDmlParams) override;
  StatusOr<std::vector<spanner::QueryPartition>> PartitionQuery(
      PartitionQueryParams) override;
  StatusOr<spanner::BatchDmlResult> ExecuteBatchDml(
      ExecuteBatchDmlParams) override;
  StatusOr<spanner::CommitResult> Commit(CommitParams) override;
  Status Rollback(RollbackParams) override;

 private:
  Status PrepareSession(SessionHolder& session,
                        bool dissociate_from_pool = false);

  StatusOr<google::spanner::v1::Transaction> BeginTransaction(
      SessionHolder& session, google::spanner::v1::TransactionOptions options,
      std::string request_tag, std::string const& transaction_tag,
      char const* func);

  spanner::RowStream ReadImpl(
      SessionHolder& session,
      StatusOr<google::spanner::v1::TransactionSelector>& s,
      std::string const& transaction_tag, ReadParams params);

  StatusOr<std::vector<spanner::ReadPartition>> PartitionReadImpl(
      SessionHolder& session,
      StatusOr<google::spanner::v1::TransactionSelector>& s,
      std::string const& transaction_tag, ReadParams const& params,
      spanner::PartitionOptions const& partition_options);

  spanner::RowStream ExecuteQueryImpl(
      SessionHolder& session,
      StatusOr<google::spanner::v1::TransactionSelector>& s,
      std::string const& transaction_tag, std::int64_t seqno, SqlParams params);

  StatusOr<spanner::DmlResult> ExecuteDmlImpl(
      SessionHolder& session,
      StatusOr<google::spanner::v1::TransactionSelector>& s,
      std::string const& transaction_tag, std::int64_t seqno, SqlParams params);

  spanner::ProfileQueryResult ProfileQueryImpl(
      SessionHolder& session,
      StatusOr<google::spanner::v1::TransactionSelector>& s,
      std::string const& transaction_tag, std::int64_t seqno, SqlParams params);

  StatusOr<spanner::ProfileDmlResult> ProfileDmlImpl(
      SessionHolder& session,
      StatusOr<google::spanner::v1::TransactionSelector>& s,
      std::string const& transaction_tag, std::int64_t seqno, SqlParams params);

  StatusOr<spanner::ExecutionPlan> AnalyzeSqlImpl(
      SessionHolder& session,
      StatusOr<google::spanner::v1::TransactionSelector>& s,
      std::string const& transaction_tag, std::int64_t seqno, SqlParams params);

  StatusOr<spanner::PartitionedDmlResult> ExecutePartitionedDmlImpl(
      SessionHolder& session,
      StatusOr<google::spanner::v1::TransactionSelector>& s,
      std::string const& transaction_tag, std::int64_t seqno,
      ExecutePartitionedDmlParams params);

  StatusOr<std::vector<spanner::QueryPartition>> PartitionQueryImpl(
      SessionHolder& session,
      StatusOr<google::spanner::v1::TransactionSelector>& s,
      std::string const& transaction_tag, PartitionQueryParams const& params);

  StatusOr<spanner::BatchDmlResult> ExecuteBatchDmlImpl(
      SessionHolder& session,
      StatusOr<google::spanner::v1::TransactionSelector>& s,
      std::string const& transaction_tag, std::int64_t seqno,
      ExecuteBatchDmlParams params);

  StatusOr<spanner::CommitResult> CommitImpl(
      SessionHolder& session,
      StatusOr<google::spanner::v1::TransactionSelector>& s,
      std::string const& transaction_tag, CommitParams params);

  Status RollbackImpl(SessionHolder& session,
                      StatusOr<google::spanner::v1::TransactionSelector>& s,
                      std::string const& transaction_tag);

  template <typename ResultType>
  StatusOr<ResultType> ExecuteSqlImpl(
      SessionHolder& session,
      StatusOr<google::spanner::v1::TransactionSelector>& s,
      std::string const& transaction_tag, std::int64_t seqno, SqlParams params,
      google::spanner::v1::ExecuteSqlRequest::QueryMode query_mode,
      std::function<StatusOr<std::unique_ptr<ResultSourceInterface>>(
          google::spanner::v1::ExecuteSqlRequest& request)> const&
          retry_resume_fn);

  template <typename ResultType>
  ResultType CommonQueryImpl(
      SessionHolder& session,
      StatusOr<google::spanner::v1::TransactionSelector>& s,
      std::string const& transaction_tag, std::int64_t seqno, SqlParams params,
      google::spanner::v1::ExecuteSqlRequest::QueryMode query_mode);
  template <typename ResultType>
  StatusOr<ResultType> CommonDmlImpl(
      SessionHolder& session,
      StatusOr<google::spanner::v1::TransactionSelector>& s,
      std::string const& transaction_tag, std::int64_t seqno, SqlParams params,
      google::spanner::v1::ExecuteSqlRequest::QueryMode query_mode);

  spanner::Database db_;
  std::unique_ptr<BackgroundThreads> background_threads_;
  Options opts_;
  std::shared_ptr<SessionPool> session_pool_;
};

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace spanner_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_INTERNAL_CONNECTION_IMPL_H
