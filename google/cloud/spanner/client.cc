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

#include "google/cloud/spanner/client.h"
#include "google/cloud/spanner/internal/connection_impl.h"
#include "google/cloud/spanner/internal/spanner_stub_factory.h"
#include "google/cloud/spanner/internal/status_utils.h"
#include "google/cloud/spanner/options.h"
#include "google/cloud/spanner/retry_policy.h"
#include "google/cloud/spanner/transaction.h"
#include "google/cloud/backoff_policy.h"
#include "google/cloud/credentials.h"
#include "google/cloud/grpc_options.h"
#include "google/cloud/internal/getenv.h"
#include "google/cloud/internal/make_status.h"
#include "google/cloud/internal/retry_loop.h"
#include "google/cloud/log.h"
#include "google/cloud/status.h"
#include "absl/types/optional.h"
#include <grpcpp/grpcpp.h>
#include <chrono>
#include <thread>

namespace google {
namespace cloud {
namespace spanner {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

namespace {

// Extracts (removes and returns) an option value from `opts`. If
// `OptionType` is not present, returns a default-constructed value.
template <typename OptionType>
typename OptionType::Type ExtractOpt(Options& opts) {
  auto option = internal::ExtractOption<OptionType>(opts);
  return option ? *std::move(option) : typename OptionType::Type();
}

}  // namespace

RowStream Client::Read(std::string table, KeySet keys,
                       std::vector<std::string> columns, Options opts) {
  opts = internal::MergeOptions(std::move(opts), opts_);
  auto directed_read_option = ExtractOpt<DirectedReadOption>(opts);
  internal::OptionsSpan span(std::move(opts));
  return conn_->Read({spanner_internal::MakeSingleUseTransaction(
                          Transaction::ReadOnlyOptions()),
                      std::move(table), std::move(keys), std::move(columns),
                      ToReadOptions(internal::CurrentOptions()), absl::nullopt,
                      false, std::move(directed_read_option)});
}

RowStream Client::Read(Transaction::SingleUseOptions transaction_options,
                       std::string table, KeySet keys,
                       std::vector<std::string> columns, Options opts) {
  opts = internal::MergeOptions(std::move(opts), opts_);
  auto directed_read_option = ExtractOpt<DirectedReadOption>(opts);
  internal::OptionsSpan span(std::move(opts));
  return conn_->Read({spanner_internal::MakeSingleUseTransaction(
                          std::move(transaction_options)),
                      std::move(table), std::move(keys), std::move(columns),
                      ToReadOptions(internal::CurrentOptions()), absl::nullopt,
                      false, std::move(directed_read_option)});
}

RowStream Client::Read(Transaction transaction, std::string table, KeySet keys,
                       std::vector<std::string> columns, Options opts) {
  opts = internal::MergeOptions(std::move(opts), opts_);
  auto directed_read_option = ExtractOpt<DirectedReadOption>(opts);
  internal::OptionsSpan span(std::move(opts));
  return conn_->Read({std::move(transaction), std::move(table), std::move(keys),
                      std::move(columns),
                      ToReadOptions(internal::CurrentOptions()), absl::nullopt,
                      false, std::move(directed_read_option)});
}

RowStream Client::Read(ReadPartition const& read_partition, Options opts) {
  opts = internal::MergeOptions(std::move(opts), opts_);
  auto directed_read_option = ExtractOpt<DirectedReadOption>(opts);
  internal::OptionsSpan span(std::move(opts));
  return conn_->Read(spanner_internal::MakeReadParams(
      read_partition, std::move(directed_read_option)));
}

StatusOr<std::vector<ReadPartition>> Client::PartitionRead(
    Transaction transaction, std::string table, KeySet keys,
    std::vector<std::string> columns, Options opts) {
  internal::OptionsSpan span(internal::MergeOptions(std::move(opts), opts_));
  return conn_->PartitionRead(
      {{std::move(transaction), std::move(table), std::move(keys),
        std::move(columns), ToReadOptions(internal::CurrentOptions()),
        absl::nullopt, false, DirectedReadOption::Type{}},
       ToPartitionOptions(internal::CurrentOptions())});
}

RowStream Client::ExecuteQuery(SqlStatement statement, Options opts) {
  opts = internal::MergeOptions(std::move(opts), opts_);
  auto directed_read_option = ExtractOpt<DirectedReadOption>(opts);
  internal::OptionsSpan span(std::move(opts));
  return conn_->ExecuteQuery(
      {spanner_internal::MakeSingleUseTransaction(
           Transaction::ReadOnlyOptions()),
       std::move(statement), QueryOptions(internal::CurrentOptions()),
       absl::nullopt, false, std::move(directed_read_option)});
}

RowStream Client::ExecuteQuery(
    Transaction::SingleUseOptions transaction_options, SqlStatement statement,
    Options opts) {
  opts = internal::MergeOptions(std::move(opts), opts_);
  auto directed_read_option = ExtractOpt<DirectedReadOption>(opts);
  internal::OptionsSpan span(std::move(opts));
  return conn_->ExecuteQuery(
      {spanner_internal::MakeSingleUseTransaction(
           std::move(transaction_options)),
       std::move(statement), QueryOptions(internal::CurrentOptions()),
       absl::nullopt, false, std::move(directed_read_option)});
}

RowStream Client::ExecuteQuery(Transaction transaction, SqlStatement statement,
                               Options opts) {
  opts = internal::MergeOptions(std::move(opts), opts_);
  auto directed_read_option = ExtractOpt<DirectedReadOption>(opts);
  internal::OptionsSpan span(std::move(opts));
  return conn_->ExecuteQuery({std::move(transaction), std::move(statement),
                              QueryOptions(internal::CurrentOptions()),
                              absl::nullopt, false,
                              std::move(directed_read_option)});
}

RowStream Client::ExecuteQuery(QueryPartition const& partition, Options opts) {
  opts = internal::MergeOptions(std::move(opts), opts_);
  auto directed_read_option = ExtractOpt<DirectedReadOption>(opts);
  internal::OptionsSpan span(std::move(opts));
  return conn_->ExecuteQuery(spanner_internal::MakeSqlParams(
      partition, QueryOptions(internal::CurrentOptions()),
      std::move(directed_read_option)));
}

ProfileQueryResult Client::ProfileQuery(SqlStatement statement, Options opts) {
  opts = internal::MergeOptions(std::move(opts), opts_);
  auto directed_read_option = ExtractOpt<DirectedReadOption>(opts);
  internal::OptionsSpan span(std::move(opts));
  return conn_->ProfileQuery(
      {spanner_internal::MakeSingleUseTransaction(
           Transaction::ReadOnlyOptions()),
       std::move(statement), QueryOptions(internal::CurrentOptions()),
       absl::nullopt, false, std::move(directed_read_option)});
}

ProfileQueryResult Client::ProfileQuery(
    Transaction::SingleUseOptions transaction_options, SqlStatement statement,
    Options opts) {
  opts = internal::MergeOptions(std::move(opts), opts_);
  auto directed_read_option = ExtractOpt<DirectedReadOption>(opts);
  internal::OptionsSpan span(std::move(opts));
  return conn_->ProfileQuery(
      {spanner_internal::MakeSingleUseTransaction(
           std::move(transaction_options)),
       std::move(statement), QueryOptions(internal::CurrentOptions()),
       absl::nullopt, false, std::move(directed_read_option)});
}

ProfileQueryResult Client::ProfileQuery(Transaction transaction,
                                        SqlStatement statement, Options opts) {
  opts = internal::MergeOptions(std::move(opts), opts_);
  auto directed_read_option = ExtractOpt<DirectedReadOption>(opts);
  internal::OptionsSpan span(std::move(opts));
  return conn_->ProfileQuery({std::move(transaction), std::move(statement),
                              QueryOptions(internal::CurrentOptions()),
                              absl::nullopt, false,
                              std::move(directed_read_option)});
}

StatusOr<std::vector<QueryPartition>> Client::PartitionQuery(
    Transaction transaction, SqlStatement statement, Options opts) {
  internal::OptionsSpan span(internal::MergeOptions(std::move(opts), opts_));
  return conn_->PartitionQuery(
      {std::move(transaction), std::move(statement),
       ToPartitionOptions(internal::CurrentOptions())});
}

StatusOr<DmlResult> Client::ExecuteDml(Transaction transaction,
                                       SqlStatement statement, Options opts) {
  internal::OptionsSpan span(internal::MergeOptions(std::move(opts), opts_));
  return conn_->ExecuteDml({std::move(transaction), std::move(statement),
                            QueryOptions(internal::CurrentOptions()),
                            absl::nullopt, false, DirectedReadOption::Type{}});
}

StatusOr<ProfileDmlResult> Client::ProfileDml(Transaction transaction,
                                              SqlStatement statement,
                                              Options opts) {
  internal::OptionsSpan span(internal::MergeOptions(std::move(opts), opts_));
  return conn_->ProfileDml({std::move(transaction), std::move(statement),
                            QueryOptions(internal::CurrentOptions()),
                            absl::nullopt, false, DirectedReadOption::Type{}});
}

StatusOr<ExecutionPlan> Client::AnalyzeSql(Transaction transaction,
                                           SqlStatement statement,
                                           Options opts) {
  internal::OptionsSpan span(internal::MergeOptions(std::move(opts), opts_));
  return conn_->AnalyzeSql({std::move(transaction), std::move(statement),
                            QueryOptions(internal::CurrentOptions()),
                            absl::nullopt, false, DirectedReadOption::Type{}});
}

StatusOr<BatchDmlResult> Client::ExecuteBatchDml(
    Transaction transaction, std::vector<SqlStatement> statements,
    Options opts) {
  internal::CheckExpectedOptions<RequestOptionList>(opts, __func__);
  internal::OptionsSpan span(internal::MergeOptions(std::move(opts), opts_));
  return conn_->ExecuteBatchDml({std::move(transaction), std::move(statements),
                                 internal::CurrentOptions()});
}

StatusOr<CommitResult> Client::Commit(
    std::function<StatusOr<Mutations>(Transaction)> const& mutator,
    std::unique_ptr<TransactionRerunPolicy> rerun_policy,
    std::unique_ptr<BackoffPolicy> backoff_policy, Options opts) {
  internal::OptionsSpan span(internal::MergeOptions(std::move(opts), opts_));

  // The status-code discriminator of TransactionRerunPolicy.
  using RerunnablePolicy = spanner_internal::SafeTransactionRerun;

  auto const txn_opts = Transaction::ReadWriteOptions().WithTag(
      internal::FetchOption<TransactionTagOption>(internal::CurrentOptions()));
  Transaction txn = MakeReadWriteTransaction(txn_opts);
  for (;;) {
    StatusOr<Mutations> mutations;
#if GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
    try {
#endif
      mutations = mutator(txn);
#if GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
    } catch (RuntimeStatusError const& error) {
      // Treat this like mutator() returned a bad Status.
      Status status = error.status();
      if (status.ok()) {
        status = internal::UnknownError("OK Status thrown from mutator",
                                        GCP_ERROR_INFO());
      }
      mutations = status;
    } catch (...) {
      auto rb_status = Rollback(txn, internal::CurrentOptions());
      if (!RerunnablePolicy::IsOk(rb_status)) {
        GCP_LOG(WARNING) << "Rollback() failure in Client::Commit(): "
                         << rb_status.message();
      }
      throw;
    }
#endif
    auto status = mutations.status();
    if (RerunnablePolicy::IsOk(status)) {
      auto result = Commit(txn, *mutations, internal::CurrentOptions());
      status = result.status();
      if (!RerunnablePolicy::IsTransientFailure(status)) {
        return result;
      }
    } else {
      if (!RerunnablePolicy::IsTransientFailure(status)) {
        auto rb_status = Rollback(txn, internal::CurrentOptions());
        if (!RerunnablePolicy::IsOk(rb_status)) {
          GCP_LOG(WARNING) << "Rollback() failure in Client::Commit(): "
                           << rb_status.message();
        }
        return status;
      }
    }
    // A transient failure (e.g., kAborted), so consider rerunning.
    if (!rerun_policy->OnFailure(status)) {
      return status;  // reruns exhausted
    }
    if (spanner_internal::IsSessionNotFound(status)) {
      // Marks the session bad and creates a new Transaction for the next loop.
      spanner_internal::Visit(
          txn, [](spanner_internal::SessionHolder& s,
                  StatusOr<google::spanner::v1::TransactionSelector> const&,
                  spanner_internal::TransactionContext const&) {
            if (s) s->set_bad();
            return true;
          });
      txn = MakeReadWriteTransaction(txn_opts);
    } else {
      // Create a new transaction for the next loop, but reuse the session
      // so that we have a slightly better chance of avoiding another abort.
      txn = MakeReadWriteTransaction(txn, txn_opts);
    }
    std::chrono::nanoseconds delay = backoff_policy->OnCompletion();
    if (internal::CurrentOptions().get<EnableServerRetriesOption>()) {
      if (auto retry_info = internal::GetRetryInfo(status)) {
        // Heed the `RetryInfo` from the service.
        delay = retry_info->retry_delay();
      }
    }
    std::this_thread::sleep_for(delay);
  }
}

StatusOr<CommitResult> Client::Commit(
    std::function<StatusOr<Mutations>(Transaction)> const& mutator,
    Options opts) {
  internal::OptionsSpan span(internal::MergeOptions(std::move(opts), opts_));
  auto const rerun_maximum_duration = std::chrono::minutes(10);
  auto default_commit_rerun_policy =
      LimitedTimeTransactionRerunPolicy(rerun_maximum_duration).clone();

  auto const backoff_initial_delay = std::chrono::milliseconds(100);
  auto const backoff_maximum_delay = std::chrono::minutes(5);
  auto const backoff_scaling = 2.0;
  auto default_commit_backoff_policy =
      ExponentialBackoffPolicy(backoff_initial_delay, backoff_maximum_delay,
                               backoff_scaling)
          .clone();

  return Commit(mutator, std::move(default_commit_rerun_policy),
                std::move(default_commit_backoff_policy),
                internal::CurrentOptions());
}

StatusOr<CommitResult> Client::Commit(Mutations mutations, Options opts) {
  return Commit([&mutations](Transaction const&) { return mutations; },
                std::move(opts));
}

StatusOr<CommitResult> Client::Commit(Transaction transaction,
                                      Mutations mutations, Options opts) {
  internal::OptionsSpan span(internal::MergeOptions(std::move(opts), opts_));
  return conn_->Commit({std::move(transaction), std::move(mutations),
                        CommitOptions(internal::CurrentOptions())});
}

StatusOr<CommitResult> Client::CommitAtLeastOnce(
    Transaction::ReadWriteOptions transaction_options, Mutations mutations,
    Options opts) {
  // Note: This implementation differs from `CommitAtLeastOnce({mutations})`
  // for historical reasons.
  internal::OptionsSpan span(internal::MergeOptions(std::move(opts), opts_));
  return conn_->Commit({spanner_internal::MakeSingleUseCommitTransaction(
                            std::move(transaction_options)),
                        std::move(mutations),
                        CommitOptions(internal::CurrentOptions())});
}

BatchedCommitResultStream Client::CommitAtLeastOnce(
    std::vector<Mutations> mutation_groups, Options opts) {
  internal::OptionsSpan span(internal::MergeOptions(std::move(opts), opts_));
  return conn_->BatchWrite(
      {std::move(mutation_groups), internal::CurrentOptions()});
}

Status Client::Rollback(Transaction transaction, Options opts) {
  internal::OptionsSpan span(internal::MergeOptions(std::move(opts), opts_));
  return conn_->Rollback({std::move(transaction)});
}

StatusOr<PartitionedDmlResult> Client::ExecutePartitionedDml(
    SqlStatement statement, Options opts) {
  internal::OptionsSpan span(internal::MergeOptions(std::move(opts), opts_));
  return conn_->ExecutePartitionedDml(
      {std::move(statement), QueryOptions(internal::CurrentOptions())});
}

std::shared_ptr<spanner::Connection> MakeConnection(spanner::Database const& db,
                                                    Options opts) {
  internal::CheckExpectedOptions<
      CommonOptionList, GrpcOptionList, UnifiedCredentialsOptionList,
      SessionPoolOptionList, spanner_internal::SessionPoolClockOption,
      SpannerPolicyOptionList>(opts, __func__);
  opts = spanner_internal::DefaultOptions(std::move(opts));

  auto background = internal::MakeBackgroundThreadsFactory(opts)();
  auto auth = internal::CreateAuthenticationStrategy(background->cq(), opts);
  std::vector<std::shared_ptr<spanner_internal::SpannerStub>> stubs(
      opts.get<GrpcNumChannelsOption>());
  int id = 0;
  std::generate(stubs.begin(), stubs.end(), [&db, &auth, &opts, &id] {
    return spanner_internal::CreateDefaultSpannerStub(db, auth, opts, id++);
  });
  return std::make_shared<spanner_internal::ConnectionImpl>(
      std::move(db), std::move(background), std::move(stubs), std::move(opts));
}

std::shared_ptr<Connection> MakeConnection(
    Database const& db, ConnectionOptions const& connection_options,
    SessionPoolOptions session_pool_options) {
  auto opts = internal::MergeOptions(
      internal::MakeOptions(connection_options),
      spanner_internal::MakeOptions(std::move(session_pool_options)));
  return MakeConnection(db, std::move(opts));
}

std::shared_ptr<Connection> MakeConnection(
    Database const& db, ConnectionOptions const& connection_options,
    SessionPoolOptions session_pool_options,
    std::unique_ptr<RetryPolicy> retry_policy,
    std::unique_ptr<BackoffPolicy> backoff_policy) {
  auto opts = internal::MergeOptions(
      internal::MakeOptions(connection_options),
      spanner_internal::MakeOptions(std::move(session_pool_options)));
  opts.set<SpannerRetryPolicyOption>(retry_policy->clone());
  opts.set<SpannerBackoffPolicyOption>(backoff_policy->clone());
  return MakeConnection(db, std::move(opts));
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace spanner
}  // namespace cloud
}  // namespace google
