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

#include "google/cloud/spanner/internal/connection_impl.h"
#include "google/cloud/spanner/internal/partial_result_set_reader.h"
#include "google/cloud/spanner/internal/time.h"
#include "google/cloud/spanner/query_partition.h"
#include "google/cloud/spanner/read_partition.h"
#include "google/cloud/grpc_utils/grpc_error_delegate.h"
#include "google/cloud/internal/make_unique.h"
#include <google/spanner/v1/spanner.pb.h>

namespace google {
namespace cloud {
namespace spanner {
inline namespace SPANNER_CLIENT_NS {
namespace internal {

namespace spanner_proto = ::google::spanner::v1;

StatusOr<ResultSet> ConnectionImpl::Read(ReadParams rp) {
  return internal::Visit(
      std::move(rp.transaction),
      [this, &rp](SessionHolder& session, spanner_proto::TransactionSelector& s,
                  std::int64_t) {
        return ReadImpl(session, s, std::move(rp));
      });
}

StatusOr<std::vector<ReadPartition>> ConnectionImpl::PartitionRead(
    PartitionReadParams prp) {
  return internal::Visit(
      std::move(prp.read_params.transaction),
      [this, &prp](SessionHolder& session,
                   spanner_proto::TransactionSelector& s, std::int64_t) {
        return PartitionReadImpl(session, s, prp.read_params,
                                 std::move(prp.partition_options));
      });
}

StatusOr<ResultSet> ConnectionImpl::ExecuteSql(ExecuteSqlParams esp) {
  return internal::Visit(
      std::move(esp.transaction),
      [this, &esp](SessionHolder& session,
                   spanner_proto::TransactionSelector& s, std::int64_t seqno) {
        return ExecuteSqlImpl(session, s, seqno, std::move(esp));
      });
}

StatusOr<PartitionedDmlResult> ConnectionImpl::ExecutePartitionedDml(
    ExecutePartitionedDmlParams epdp) {
  auto txn = MakeReadOnlyTransaction();
  return internal::Visit(
      txn,
      [this, &epdp](SessionHolder& session,
                    spanner_proto::TransactionSelector& s, std::int64_t seqno) {
        return ExecutePartitionedDmlImpl(session, s, seqno, std::move(epdp));
      });
}

StatusOr<std::vector<QueryPartition>> ConnectionImpl::PartitionQuery(
    PartitionQueryParams pqp) {
  return internal::Visit(
      std::move(pqp.sql_params.transaction),
      [this, &pqp](SessionHolder& session,
                   spanner_proto::TransactionSelector& s, std::int64_t) {
        return PartitionQueryImpl(session, s, pqp.sql_params,
                                  std::move(pqp.partition_options));
      });
}

StatusOr<BatchDmlResult> ConnectionImpl::ExecuteBatchDml(
    BatchDmlParams params) {
  return internal::Visit(std::move(params.transaction),
                         [this, &params](SessionHolder& session,
                                         spanner_proto::TransactionSelector& s,
                                         std::int64_t seqno) {
                           return ExecuteBatchDmlImpl(session, s, seqno,
                                                      std::move(params));
                         });
}

StatusOr<CommitResult> ConnectionImpl::Commit(CommitParams cp) {
  return internal::Visit(
      std::move(cp.transaction),
      [this, &cp](SessionHolder& session, spanner_proto::TransactionSelector& s,
                  std::int64_t) {
        return this->CommitImpl(session, s, std::move(cp));
      });
}

Status ConnectionImpl::Rollback(RollbackParams rp) {
  return internal::Visit(
      std::move(rp.transaction),
      [this](SessionHolder& session, spanner_proto::TransactionSelector& s,
             std::int64_t) { return this->RollbackImpl(session, s); });
}

StatusOr<ResultSet> ConnectionImpl::ReadImpl(
    SessionHolder& session, spanner_proto::TransactionSelector& s,
    ReadParams rp) {
  if (!session) {
    auto session_or = GetSession();
    if (!session_or) {
      return std::move(session_or).status();
    }
    session = std::move(*session_or);
  }

  spanner_proto::ReadRequest request;
  request.set_session(session->session_name());
  *request.mutable_transaction() = s;
  request.set_table(std::move(rp.table));
  request.set_index(std::move(rp.read_options.index_name));
  for (auto&& column : rp.columns) {
    request.add_columns(std::move(column));
  }
  *request.mutable_key_set() = internal::ToProto(std::move(rp.keys));
  request.set_limit(rp.read_options.limit);
  if (rp.partition_token) {
    request.set_partition_token(*std::move(rp.partition_token));
  }

  auto context = google::cloud::internal::make_unique<grpc::ClientContext>();
  auto rpc = stub_->StreamingRead(*context, request);
  auto reader = internal::PartialResultSetReader::Create(std::move(context),
                                                         std::move(rpc));
  if (!reader.ok()) {
    return std::move(reader).status();
  }
  if (s.has_begin()) {
    auto metadata = (*reader)->Metadata();
    if (!metadata || metadata->transaction().id().empty()) {
      return Status(
          StatusCode::kInternal,
          "Begin transaction requested but no transaction returned (in Read).");
    }
    s.set_id(metadata->transaction().id());
  }
  return ResultSet(std::move(*reader));
}

StatusOr<std::vector<ReadPartition>> ConnectionImpl::PartitionReadImpl(
    SessionHolder& session, spanner_proto::TransactionSelector& s,
    ReadParams const& rp, PartitionOptions partition_options) {
  if (!session) {
    // Since the session may be sent to other machines, it should not be
    // returned to the pool when the Transaction is destroyed.
    auto session_or = GetSession(/*dissociate_from_pool=*/true);
    if (!session_or) {
      return std::move(session_or).status();
    }
    session = std::move(*session_or);
  }

  spanner_proto::PartitionReadRequest request;
  request.set_session(session->session_name());
  *request.mutable_transaction() = s;
  request.set_table(rp.table);
  request.set_index(rp.read_options.index_name);
  for (auto const& column : rp.columns) {
    *request.add_columns() = column;
  }
  *request.mutable_key_set() = internal::ToProto(rp.keys);
  *request.mutable_partition_options() = std::move(partition_options);

  auto context = google::cloud::internal::make_unique<grpc::ClientContext>();
  auto response = stub_->PartitionRead(*context, request);
  if (!response.ok()) {
    return std::move(response).status();
  }

  if (s.has_begin()) {
    s.set_id(response->transaction().id());
  }

  std::vector<ReadPartition> read_partitions;
  for (auto& partition : response->partitions()) {
    read_partitions.push_back(internal::MakeReadPartition(
        response->transaction().id(), session->session_name(),
        partition.partition_token(), rp.table, rp.keys, rp.columns,
        rp.read_options));
  }

  return read_partitions;
}

StatusOr<ResultSet> ConnectionImpl::ExecuteSqlImpl(
    SessionHolder& session, spanner_proto::TransactionSelector& s,
    std::int64_t seqno, ExecuteSqlParams esp) {
  if (!session) {
    auto session_or = GetSession();
    if (!session_or) {
      return std::move(session_or).status();
    }
    session = std::move(*session_or);
  }

  spanner_proto::ExecuteSqlRequest request;
  request.set_session(session->session_name());
  *request.mutable_transaction() = s;
  auto sql_statement = internal::ToProto(std::move(esp.statement));
  request.set_sql(std::move(*sql_statement.mutable_sql()));
  *request.mutable_params() = std::move(*sql_statement.mutable_params());
  *request.mutable_param_types() =
      std::move(*sql_statement.mutable_param_types());
  request.set_seqno(seqno);
  if (esp.partition_token) {
    request.set_partition_token(*std::move(esp.partition_token));
  }

  auto context = google::cloud::internal::make_unique<grpc::ClientContext>();
  auto rpc = stub_->ExecuteStreamingSql(*context, request);
  auto reader = internal::PartialResultSetReader::Create(std::move(context),
                                                         std::move(rpc));
  if (!reader.ok()) {
    return std::move(reader).status();
  }
  if (s.has_begin()) {
    auto metadata = (*reader)->Metadata();
    if (!metadata || metadata->transaction().id().empty()) {
      return Status(StatusCode::kInternal,
                    "Begin transaction requested but no transaction returned "
                    "(in ExecuteSql).");
    }
    s.set_id(metadata->transaction().id());
  }
  return ResultSet(std::move(*reader));
}

StatusOr<PartitionedDmlResult> ConnectionImpl::ExecutePartitionedDmlImpl(
    SessionHolder& session, spanner_proto::TransactionSelector& s,
    std::int64_t seqno, ExecutePartitionedDmlParams epdp) {
  if (!session) {
    auto session_or = GetSession();
    if (!session_or) {
      return std::move(session_or).status();
    }
    session = std::move(*session_or);
  }

  grpc::ClientContext begin_context;
  spanner_proto::BeginTransactionRequest begin_request;
  begin_request.set_session(session->session_name());
  *begin_request.mutable_options()->mutable_partitioned_dml() =
      spanner_proto::TransactionOptions_PartitionedDml();

  auto begin_response = stub_->BeginTransaction(begin_context, begin_request);
  if (!begin_response) return std::move(begin_response).status();

  s.set_id(begin_response->id());

  grpc::ClientContext context;
  spanner_proto::ExecuteSqlRequest request;
  request.set_session(session->session_name());
  *request.mutable_transaction() = s;
  auto sql_statement = internal::ToProto(std::move(epdp.statement));
  request.set_sql(std::move(*sql_statement.mutable_sql()));
  *request.mutable_params() = std::move(*sql_statement.mutable_params());
  *request.mutable_param_types() =
      std::move(*sql_statement.mutable_param_types());
  request.set_seqno(seqno);

  auto response = stub_->ExecuteSql(context, request);
  if (!response) return std::move(response).status();

  PartitionedDmlResult result{0};
  if (response->has_stats()) {
    result.row_count_lower_bound = response->stats().row_count_lower_bound();
  }

  return result;
}

StatusOr<std::vector<QueryPartition>> ConnectionImpl::PartitionQueryImpl(
    SessionHolder& session, spanner_proto::TransactionSelector& s,
    ExecuteSqlParams const& esp, PartitionOptions partition_options) {
  if (!session) {
    // Since the session may be sent to other machines, it should not be
    // returned to the pool when the Transaction is destroyed.
    auto session_or = GetSession(/*dissociate_from_pool=*/true);
    if (!session_or) {
      return std::move(session_or).status();
    }
    session = std::move(*session_or);
  }

  spanner_proto::PartitionQueryRequest request;
  request.set_session(session->session_name());
  *request.mutable_transaction() = s;
  auto sql_statement = internal::ToProto(esp.statement);
  request.set_sql(std::move(*sql_statement.mutable_sql()));
  *request.mutable_params() = std::move(*sql_statement.mutable_params());
  *request.mutable_param_types() =
      std::move(*sql_statement.mutable_param_types());
  *request.mutable_partition_options() = std::move(partition_options);

  auto context = google::cloud::internal::make_unique<grpc::ClientContext>();
  auto response = stub_->PartitionQuery(*context, request);

  if (!response.ok()) {
    return std::move(response).status();
  }

  if (s.has_begin()) {
    s.set_id(response->transaction().id());
  }

  std::vector<QueryPartition> query_partitions;
  for (auto& partition : response->partitions()) {
    query_partitions.push_back(internal::MakeQueryPartition(
        response->transaction().id(), session->session_name(),
        partition.partition_token(), esp.statement));
  }

  return query_partitions;
}

StatusOr<BatchDmlResult> ConnectionImpl::ExecuteBatchDmlImpl(
    SessionHolder& session, spanner_proto::TransactionSelector& s,
    std::int64_t seqno, BatchDmlParams params) {
  if (!session) {
    auto session_or = GetSession();
    if (!session_or) {
      return std::move(session_or).status();
    }
    session = std::move(*session_or);
  }

  spanner_proto::ExecuteBatchDmlRequest request;
  request.set_session(session->session_name());
  request.set_seqno(seqno);
  *request.mutable_transaction() = s;
  for (auto& sql : params.statements) {
    *request.add_statements() = internal::ToProto(std::move(sql));
  }

  grpc::ClientContext context;
  auto response = stub_->ExecuteBatchDml(context, request);
  if (!response) {
    return std::move(response).status();
  }

  if (response->result_sets_size() > 0 && s.has_begin()) {
    s.set_id(response->result_sets(0).metadata().transaction().id());
  }

  BatchDmlResult result;
  result.status = grpc_utils::MakeStatusFromRpcError(response->status());
  for (auto const& result_set : response->result_sets()) {
    result.stats.push_back({result_set.stats().row_count_exact()});
  }

  return result;
}

StatusOr<CommitResult> ConnectionImpl::CommitImpl(
    SessionHolder& session, spanner_proto::TransactionSelector& s,
    CommitParams cp) {
  if (!session) {
    auto session_or = GetSession();
    if (!session_or) {
      return std::move(session_or).status();
    }
    session = std::move(*session_or);
  }

  spanner_proto::CommitRequest request;
  request.set_session(session->session_name());
  for (auto&& m : cp.mutations) {
    *request.add_mutations() = std::move(m).as_proto();
  }
  if (s.has_single_use()) {
    *request.mutable_single_use_transaction() = s.single_use();
  } else if (s.has_begin()) {
    *request.mutable_single_use_transaction() = s.begin();
  } else {
    request.set_transaction_id(s.id());
  }
  grpc::ClientContext context;
  auto response = stub_->Commit(context, request);
  if (!response) {
    return std::move(response).status();
  }
  CommitResult r;
  r.commit_timestamp = internal::FromProto(response->commit_timestamp());
  return r;
}

Status ConnectionImpl::RollbackImpl(SessionHolder& session,
                                    spanner_proto::TransactionSelector& s) {
  if (!session) {
    auto session_or = GetSession();
    if (!session_or) {
      return std::move(session_or).status();
    }
    session = std::move(*session_or);
  }

  if (s.has_single_use()) {
    return Status(StatusCode::kInvalidArgument,
                  "Cannot rollback a single-use transaction");
  }
  if (s.has_begin()) {
    // There is nothing to rollback if a transaction id has not yet been
    // assigned, so we just succeed without making an RPC.
    return Status();
  }
  spanner_proto::RollbackRequest request;
  request.set_session(session->session_name());
  request.set_transaction_id(s.id());
  grpc::ClientContext context;
  return stub_->Rollback(context, request);
}

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
StatusOr<SessionHolder> ConnectionImpl::GetSession(bool dissociate_from_pool) {
  std::unique_ptr<Session> session;
  std::unique_lock<std::mutex> lk(mu_);
  if (!sessions_.empty()) {
    session = std::move(sessions_.back());
    sessions_.pop_back();
  } else {
    // Release the mutex because we won't be doing any more changes to
    // `sessions_` in this function and holding mutexes while making RPCs is
    // (generally) a bad practice.
    lk.unlock();
    grpc::ClientContext context;
    spanner_proto::CreateSessionRequest request;
    request.set_database(db_.FullName());
    auto response = stub_->CreateSession(context, request);
    if (!response) {
      return response.status();
    }
    session = google::cloud::internal::make_unique<Session>(
        std::move(*response->mutable_name()));
  }

  if (dissociate_from_pool) {
    // Uses the default deleter; the Session is not returned to the pool.
    return {std::move(session)};
  }

  std::weak_ptr<ConnectionImpl> connection = shared_from_this();
  return SessionHolder(session.release(), [connection](Session* session) {
    auto shared_connection = connection.lock();
    // If `connection` is still alive, release the `Session` to its pool;
    // otherwise just delete the `Session`.
    if (shared_connection) {
      shared_connection->ReleaseSession(session);
    } else {
      delete session;
    }
  });
}

void ConnectionImpl::ReleaseSession(Session* session) {
  std::lock_guard<std::mutex> lk(mu_);
  sessions_.emplace_back(session);
}

}  // namespace internal
}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner
}  // namespace cloud
}  // namespace google
