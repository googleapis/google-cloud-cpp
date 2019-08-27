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
      [this, &rp](spanner_proto::TransactionSelector& s, std::int64_t) {
        return Read(s, std::move(rp));
      });
}

StatusOr<std::vector<ReadPartition>> ConnectionImpl::PartitionRead(
    PartitionReadParams prp) {
  return internal::Visit(
      std::move(prp.read_params.transaction),
      [this, &prp](spanner_proto::TransactionSelector& s, std::int64_t) {
        return PartitionRead(s, prp.read_params,
                             std::move(prp.partition_options));
      });
}

StatusOr<ResultSet> ConnectionImpl::ExecuteSql(ExecuteSqlParams esp) {
  return internal::Visit(
      std::move(esp.transaction),
      [this, &esp](spanner_proto::TransactionSelector& s, std::int64_t seqno) {
        return ExecuteSql(s, seqno, std::move(esp));
      });
}

StatusOr<std::vector<QueryPartition>> ConnectionImpl::PartitionQuery(
    PartitionQueryParams pqp) {
  return internal::Visit(
      std::move(pqp.sql_params.transaction),
      [this, &pqp](spanner_proto::TransactionSelector& s, std::int64_t) {
        return PartitionQuery(s, pqp.sql_params,
                              std::move(pqp.partition_options));
      });
}

StatusOr<CommitResult> ConnectionImpl::Commit(CommitParams cp) {
  return internal::Visit(
      std::move(cp.transaction),
      [this, &cp](spanner_proto::TransactionSelector& s, std::int64_t) {
        return this->Commit(s, std::move(cp));
      });
}

Status ConnectionImpl::Rollback(RollbackParams rp) {
  return internal::Visit(std::move(rp.transaction),
                         [this](spanner_proto::TransactionSelector& s,
                                std::int64_t) { return this->Rollback(s); });
}

StatusOr<ConnectionImpl::SessionHolder> ConnectionImpl::GetSession() {
  std::unique_lock<std::mutex> lk(mu_);
  if (!sessions_.empty()) {
    std::string session = std::move(sessions_.back());
    sessions_.pop_back();
    return SessionHolder(std::move(session), this);
  }
  // Release the mutex because we won't be doing any more changes to `sessions_`
  // in this function and holding mutexes while making RPCs is (generally) a bad
  // practice.
  lk.unlock();
  grpc::ClientContext context;
  spanner_proto::CreateSessionRequest request;
  request.set_database(db_.FullName());
  auto response = stub_->CreateSession(context, request);
  if (!response) {
    return response.status();
  }
  return SessionHolder(std::move(*response->mutable_name()), this);
}

void ConnectionImpl::ReleaseSession(std::string session) {
  std::lock_guard<std::mutex> lk(mu_);
  sessions_.push_back(std::move(session));
}

StatusOr<ResultSet> ConnectionImpl::Read(spanner_proto::TransactionSelector& s,
                                         ReadParams rp) {
  spanner_proto::ReadRequest request;
  // TODO(#445): Refactor once correct location for session implemented.
  if (rp.session_name) {
    request.set_session(*std::move(rp.session_name));
  } else {
    auto session = GetSession();
    if (!session) {
      return std::move(session).status();
    }
    request.set_session(session->session_name());
  }

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

StatusOr<std::vector<ReadPartition>> ConnectionImpl::PartitionRead(
    spanner_proto::TransactionSelector& s, ReadParams const& rp,
    PartitionOptions partition_options) {
  auto session = GetSession();
  if (!session) {
    return std::move(session).status();
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

StatusOr<ResultSet> ConnectionImpl::ExecuteSql(
    spanner_proto::TransactionSelector& s, std::int64_t seqno,
    ExecuteSqlParams esp) {
  spanner_proto::ExecuteSqlRequest request;
  // TODO(#445): Refactor once correct location for session implemented.
  if (esp.session_name) {
    request.set_session(*std::move(esp.session_name));
  } else {
    auto session = GetSession();
    if (!session) {
      return std::move(session).status();
    }
    request.set_session(session->session_name());
  }
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

StatusOr<std::vector<QueryPartition>> ConnectionImpl::PartitionQuery(
    spanner_proto::TransactionSelector& s, ExecuteSqlParams const& esp,
    PartitionOptions partition_options) {
  auto session = GetSession();
  if (!session) {
    return std::move(session).status();
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

StatusOr<CommitResult> ConnectionImpl::Commit(
    spanner_proto::TransactionSelector& s, CommitParams cp) {
  auto session = GetSession();
  if (!session) {
    return std::move(session).status();
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

Status ConnectionImpl::Rollback(spanner_proto::TransactionSelector& s) {
  auto session = GetSession();
  if (!session) {
    return std::move(session).status();
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

}  // namespace internal
}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner
}  // namespace cloud
}  // namespace google
