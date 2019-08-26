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
#include "google/cloud/spanner/backoff_policy.h"
#include "google/cloud/spanner/internal/connection_impl.h"
#include "google/cloud/spanner/internal/retry_loop.h"
#include "google/cloud/spanner/internal/spanner_stub.h"
#include "google/cloud/spanner/retry_policy.h"
#include "google/cloud/log.h"
#include <grpcpp/grpcpp.h>
#include <thread>

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

StatusOr<ResultSet> Client::Read(ReadPartition const& read_partition) {
  return conn_->Read(internal::MakeReadParams(read_partition));
}

StatusOr<std::vector<ReadPartition>> Client::PartitionRead(
    Transaction transaction, std::string table, KeySet keys,
    std::vector<std::string> columns, ReadOptions read_options,
    PartitionOptions partition_options) {
  return conn_->PartitionRead(
      {{std::move(transaction), std::move(table), std::move(keys),
        std::move(columns), std::move(read_options)},
       std::move(partition_options)});
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

StatusOr<ResultSet> Client::ExecuteSql(QueryPartition const& /* partition */) {
  return Status(StatusCode::kUnimplemented, "not implemented");
}

StatusOr<std::vector<QueryPartition>> Client::PartitionQuery(
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
                                      Mutations mutations) {
  return conn_->Commit({std::move(transaction), std::move(mutations)});
}

Status Client::Rollback(Transaction transaction) {
  return conn_->Rollback({std::move(transaction)});
}

std::shared_ptr<Connection> MakeConnection(
    Database const& db, std::shared_ptr<grpc::ChannelCredentials> const& creds,
    std::string const& endpoint) {
  auto stub = internal::CreateDefaultSpannerStub(creds, endpoint);
  return std::make_shared<internal::ConnectionImpl>(db, std::move(stub));
}

namespace {

StatusOr<CommitResult> RunTransactionImpl(
    Client& client, Transaction::ReadWriteOptions const& opts,
    std::function<StatusOr<Mutations>(Client, Transaction)> const& f) {
  Transaction txn = MakeReadWriteTransaction(opts);
  StatusOr<Mutations> mutations;
#if GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
  try {
#endif
    mutations = f(client, txn);
#if GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
  } catch (...) {
    auto status = client.Rollback(txn);
    if (!status.ok()) {
      GCP_LOG(WARNING) << "Rollback() failure in RunTransaction(): "
                       << status.message();
    }
    throw;
  }
#endif
  if (!mutations) {
    auto status = client.Rollback(txn);
    if (!status.ok()) {
      GCP_LOG(WARNING) << "Rollback() failure in RunTransaction(): "
                       << status.message();
    }
    return mutations.status();
  }
  return client.Commit(txn, *mutations);
}

}  // namespace

StatusOr<CommitResult> RunTransaction(
    Client client, Transaction::ReadWriteOptions const& opts,
    std::function<StatusOr<Mutations>(Client, Transaction)> const& f) {
  ExponentialBackoffPolicy backoff_policy(std::chrono::milliseconds(100),
                                          std::chrono::minutes(5), 2.0);
  // TODO(#357,#442): It is not a good idea to simply cap the number of
  // retries. It is better to limit the total amount of time spent retrying.
  LimitedErrorCountRetryPolicy retry_policy(/*maximum_failures=*/2);

  Status last_status(
      StatusCode::kFailedPrecondition,
      "Retry policy should not be exhausted when retry loop starts");
  char const* reason = "Too many failures in ";
  while (!retry_policy.IsExhausted()) {
    auto result = RunTransactionImpl(client, opts, f);
    if (result) return result;
    last_status = std::move(result).status();
    if (!retry_policy.OnFailure(last_status)) {
      if (internal::SafeGrpcRetry::IsPermanentFailure(last_status)) {
        reason = "Permanent failure in ";
      }
      break;
    }
    std::this_thread::sleep_for(backoff_policy.OnCompletion());
  }
  return internal::RetryLoopError(reason, __func__, last_status);
}

}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner
}  // namespace cloud
}  // namespace google
