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

StatusOr<ResultSet> ConnectionImpl::ExecuteSql(ExecuteSqlParams esp) {
  return internal::Visit(
      std::move(esp.transaction),
      [this, &esp](spanner_proto::TransactionSelector& s, std::int64_t seqno) {
        return ExecuteSql(s, seqno, std::move(esp));
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
  if (!sessions_.empty()) {
    std::string session = sessions_.back();
    sessions_.pop_back();
    return SessionHolder(std::move(session), this);
  }
  grpc::ClientContext context;
  spanner_proto::CreateSessionRequest request;
  request.set_database(database_);
  auto response = stub_->CreateSession(context, request);
  if (!response) {
    return response.status();
  }
  return SessionHolder(std::move(*response->mutable_name()), this);
}

StatusOr<ResultSet> ConnectionImpl::Read(spanner_proto::TransactionSelector& s,
                                         ReadParams rp) {
  auto session = GetSession();
  if (!session) {
    return std::move(session).status();
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

StatusOr<ResultSet> ConnectionImpl::ExecuteSql(
    spanner_proto::TransactionSelector& s, std::int64_t seqno,
    ExecuteSqlParams esp) {
  auto session = GetSession();
  if (!session) {
    return std::move(session).status();
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
