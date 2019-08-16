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
#include "google/cloud/spanner/internal/connection_impl.h"
#include "google/cloud/spanner/internal/spanner_stub.h"
#include <grpcpp/grpcpp.h>

namespace google {
namespace cloud {
namespace spanner {
inline namespace SPANNER_CLIENT_NS {

namespace spanner_proto = ::google::spanner::v1;

StatusOr<ResultSet> Client::Read(std::string table, KeySet keys,
                                 std::vector<std::string> columns,
                                 ReadOptions read_options) {
  return conn_->Read(
      {internal::MakeSingleUseTransaction(Transaction::ReadOnlyOptions()),
       std::move(table), std::move(keys), std::move(columns),
       std::move(read_options)});
}

StatusOr<ResultSet> Client::Read(
    Transaction::SingleUseOptions transaction_options, std::string table,
    KeySet keys, std::vector<std::string> columns, ReadOptions read_options) {
  return conn_->Read(
      {internal::MakeSingleUseTransaction(std::move(transaction_options)),
       std::move(table), std::move(keys), std::move(columns),
       std::move(read_options)});
}

StatusOr<ResultSet> Client::Read(Transaction transaction, std::string table,
                                 KeySet keys, std::vector<std::string> columns,
                                 ReadOptions read_options) {
  return conn_->Read({std::move(transaction), std::move(table), std::move(keys),
                      std::move(columns), std::move(read_options)});
}

StatusOr<ResultSet> Client::Read(SqlPartition const& /*partition*/) {
  return Status(StatusCode::kUnimplemented, "not implemented");
}

StatusOr<std::vector<SqlPartition>> Client::PartitionRead(
    Transaction const& /*transaction*/, std::string const& /*table*/,
    KeySet const& /*keys*/, std::vector<std::string> const& /*columns*/,
    ReadOptions const& /*read_options*/,
    PartitionOptions const& /*partition_options*/) {
  return Status(StatusCode::kUnimplemented, "not implemented");
}

StatusOr<ResultSet> Client::ExecuteSql(SqlStatement statement) {
  return conn_->ExecuteSql(
      {internal::MakeSingleUseTransaction(Transaction::ReadOnlyOptions()),
       std::move(statement)});
}

StatusOr<ResultSet> Client::ExecuteSql(
    Transaction::SingleUseOptions transaction_options, SqlStatement statement) {
  return conn_->ExecuteSql(
      {internal::MakeSingleUseTransaction(std::move(transaction_options)),
       std::move(statement)});
}

StatusOr<ResultSet> Client::ExecuteSql(Transaction transaction,
                                       SqlStatement statement) {
  return conn_->ExecuteSql({std::move(transaction), std::move(statement)});
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
                                      std::vector<Mutation> mutations) {
  return conn_->Commit({std::move(transaction), std::move(mutations)});
}

Status Client::Rollback(Transaction transaction) {
  return conn_->Rollback({std::move(transaction)});
}

std::string MakeDatabaseName(std::string const& project,
                             std::string const& instance,
                             std::string const& database_id) {
  return std::string("projects/") + project + "/instances/" + instance +
         "/databases/" + database_id;
}

std::shared_ptr<Connection> MakeConnection(
    std::string database,
    std::shared_ptr<grpc::ChannelCredentials> const& creds,
    std::string const& endpoint) {
  auto stub = internal::CreateDefaultSpannerStub(creds, endpoint);
  return std::make_shared<internal::ConnectionImpl>(std::move(database),
                                                    std::move(stub));
}

StatusOr<CommitResult> RunTransaction(
    Client client, Transaction::ReadWriteOptions const& opts,
    std::function<TransactionAction(Client, Transaction)> const& f) {
  Transaction txn = MakeReadWriteTransaction(opts);
  TransactionAction action;
#if GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
  try {
#endif
    action = f(client, txn);
#if GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
  } catch (...) {
    client.Rollback(txn);  // ignore error
    throw;
  }
#endif
  switch (action.action) {
    case TransactionAction::kCommit: {
      // TODO(#357): Automatically retry if Commit() aborts. Note: it is
      // not a good idea to simply cap the number of retries. Instead, it
      // is better to limit the total amount of wall time spent retrying.
      return client.Commit(txn, action.mutations);
    }
    case TransactionAction::kRollback: {
      auto status = client.Rollback(txn);
      if (!status.ok()) return status;
      return CommitResult{};  // TODO(#357): What should this return?
    }
  }
  return Status(StatusCode::kInvalidArgument, "bad TransactionAction");
}

}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner
}  // namespace cloud
}  // namespace google
