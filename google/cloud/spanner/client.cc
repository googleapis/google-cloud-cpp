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

#include "google/cloud/spanner/client.h"
#include "google/cloud/spanner/value.h"
#include "google/cloud/grpc_utils/grpc_error_delegate.h"
#include "google/cloud/log.h"

namespace google {
namespace cloud {
namespace spanner {
inline namespace SPANNER_CLIENT_NS {

namespace spanner_proto = ::google::spanner::v1;

StatusOr<ResultSet> Client::Read(std::string const& /*table*/,
                                 KeySet const& /*keys*/,
                                 std::vector<std::string> const& /*columns*/,
                                 ReadOptions const& /*read_options*/) {
  return Status(StatusCode::kUnimplemented, "not implemented");
}

StatusOr<ResultSet> Client::Read(
    Transaction::SingleUseOptions const& /*transaction_options*/,
    std::string const& /*table*/, KeySet const& /*keys*/,
    std::vector<std::string> const& /*columns*/,
    ReadOptions const& /*read_options*/) {
  return Status(StatusCode::kUnimplemented, "not implemented");
}

StatusOr<ResultSet> Client::Read(Transaction const& /*transaction*/,
                                 std::string const& /*table*/,
                                 KeySet const& /*keys*/,
                                 std::vector<std::string> const& /*columns*/,
                                 ReadOptions const& /*read_options*/) {
  return Status(StatusCode::kUnimplemented, "not implemented");
}

StatusOr<ResultSet> Client::Read(SqlPartition const& /*partition*/) {
  return Status(StatusCode::kUnimplemented, "not implemented");
}

StatusOr<std::vector<SqlPartition>> Client::PartitionRead(
    Transaction const& /*transaction*/, std::string const& /*table*/,
    spanner::KeySet const& /*keys*/,
    std::vector<std::string> const& /*columns*/,
    ReadOptions const& /*read_options*/,
    PartitionOptions const& /*partition_options*/) {
  return Status(StatusCode::kUnimplemented, "not implemented");
}

StatusOr<ResultSet> Client::ExecuteSql(SqlStatement const& /*statement*/) {
  return Status(StatusCode::kUnimplemented, "not implemented");
}

StatusOr<ResultSet> Client::ExecuteSql(
    Transaction::SingleUseOptions const& /*transaction_options*/,
    SqlStatement const& /*statement*/) {
  return Status(StatusCode::kUnimplemented, "not implemented");
}

StatusOr<ResultSet> Client::ExecuteSql(Transaction const& /*transaction*/,
                                       SqlStatement const& /*statement*/) {
  return Status(StatusCode::kUnimplemented, "not implemented");
}

StatusOr<ResultSet> Client::ExecuteSql(SqlPartition const& /* partition */) {
  return Status(StatusCode::kUnimplemented, "not implemented");
}

StatusOr<std::vector<SqlPartition>> Client::PartitionQuery(
    Transaction const& /*transaction*/, SqlStatement const& /*statement*/,
    PartitionOptions const& /*partition_options*/) {
  return Status(StatusCode::kUnimplemented, "not implemented");
}

std::vector<StatusOr<spanner_proto::ResultSetStats>> Client::ExecuteBatchDml(
    Transaction const& /*transaction*/,
    std::vector<SqlStatement> const& /*statements*/) {
  // This method is NOT part of the Alpha release. Please do not work on this
  // until all the higher-priority alpha work is finished.
  return {Status(StatusCode::kUnimplemented, "not implemented")};
}

// returns a lower bound on the number of modified rows
StatusOr<std::int64_t> Client::ExecutePartitionedDml(
    SqlStatement const& /*statement*/) {
  return Status(StatusCode::kUnimplemented, "not implemented");
}

StatusOr<CommitResult> Client::Commit(Transaction transaction,
                                      std::vector<Mutation> const& mutations) {
  return internal::Visit(
      std::move(transaction),
      [this, &mutations](spanner_proto::TransactionSelector& s) {
        return this->Commit(s, mutations);
      });
}

Status Client::Rollback(Transaction const& /*transaction*/) {
  return Status(StatusCode::kUnimplemented, "not implemented");
}

Client::SessionHolder::SessionHolder(std::string session,
                                     Client* client) noexcept
    : session_(std::move(session)), client_(client) {}

Client::SessionHolder::~SessionHolder() {
  client_->sessions_.emplace_back(std::move(session_));
}

StatusOr<Client::SessionHolder> Client::GetSession() {
  if (!sessions_.empty()) {
    std::string session = sessions_.back();
    sessions_.pop_back();
    return SessionHolder(std::move(session), this);
  }
  grpc::ClientContext context;
  google::spanner::v1::CreateSessionRequest request;
  request.set_database(database_name_);
  auto response = stub_->CreateSession(context, request);
  if (!response) {
    return response.status();
  }
  return SessionHolder(std::move(*response->mutable_name()), this);
}

StatusOr<CommitResult> Client::Commit(
    google::spanner::v1::TransactionSelector& s,
    std::vector<Mutation> const& mutations) {
  auto session = GetSession();
  if (!session) {
    return std::move(session).status();
  }
  google::spanner::v1::CommitRequest request;
  request.set_session(session->session_name());
  for (auto const& m : mutations) {
    *request.add_mutations() = m.as_proto();
  }
  if (!s.id().empty()) {
    request.set_transaction_id(s.id());
  } else if (s.has_begin()) {
    *request.mutable_single_use_transaction() = s.begin();
  } else if (s.has_single_use()) {
    *request.mutable_single_use_transaction() = s.single_use();
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

StatusOr<Client> MakeClient(std::string database_name,
                            ClientOptions const& client_options) {
  return Client(std::move(database_name),
                internal::CreateDefaultSpannerStub(client_options));
}

std::string MakeDatabaseName(std::string const& project,
                             std::string const& instance,
                             std::string const& database_id) {
  return std::string("projects/") + project + "/instances/" + instance +
         "/databases/" + database_id;
}

}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner
}  // namespace cloud
}  // namespace google
