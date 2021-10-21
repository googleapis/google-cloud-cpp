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
#include "google/cloud/spanner/internal/defaults.h"
#include "google/cloud/spanner/internal/logging_result_set_reader.h"
#include "google/cloud/spanner/internal/partial_result_set_resume.h"
#include "google/cloud/spanner/internal/partial_result_set_source.h"
#include "google/cloud/spanner/internal/status_utils.h"
#include "google/cloud/spanner/options.h"
#include "google/cloud/spanner/query_partition.h"
#include "google/cloud/spanner/read_partition.h"
#include "google/cloud/grpc_error_delegate.h"
#include "google/cloud/internal/algorithm.h"
#include "google/cloud/internal/retry_loop.h"
#include "google/cloud/internal/retry_policy.h"
#include "google/cloud/options.h"
#include "absl/memory/memory.h"
#include <google/protobuf/util/time_util.h>
#include <grpcpp/grpcpp.h>

namespace google {
namespace cloud {
namespace spanner_internal {
inline namespace SPANNER_CLIENT_NS {

using google::cloud::internal::Idempotency;

class DefaultPartialResultSetReader : public PartialResultSetReader {
 public:
  DefaultPartialResultSetReader(
      std::unique_ptr<grpc::ClientContext> context,
      std::unique_ptr<
          grpc::ClientReaderInterface<google::spanner::v1::PartialResultSet>>
          reader)
      : context_(std::move(context)), reader_(std::move(reader)) {}

  ~DefaultPartialResultSetReader() override = default;

  void TryCancel() override { context_->TryCancel(); }

  absl::optional<google::spanner::v1::PartialResultSet> Read() override {
    google::spanner::v1::PartialResultSet result;
    bool success = reader_->Read(&result);
    if (!success) return {};
    return result;
  }

  Status Finish() override {
    return google::cloud::MakeStatusFromRpcError(reader_->Finish());
  }

 private:
  std::unique_ptr<grpc::ClientContext> context_;
  std::unique_ptr<
      grpc::ClientReaderInterface<google::spanner::v1::PartialResultSet>>
      reader_;
};

namespace spanner_proto = ::google::spanner::v1;

spanner_proto::TransactionOptions PartitionedDmlTransactionOptions() {
  spanner_proto::TransactionOptions options;
  *options.mutable_partitioned_dml() =
      spanner_proto::TransactionOptions_PartitionedDml();
  return options;
}

spanner_proto::RequestOptions_Priority ProtoRequestPriority(
    absl::optional<spanner::RequestPriority> const& request_priority) {
  if (request_priority) {
    switch (*request_priority) {
      case spanner::RequestPriority::kLow:
        return spanner_proto::RequestOptions::PRIORITY_LOW;
      case spanner::RequestPriority::kMedium:
        return spanner_proto::RequestOptions::PRIORITY_MEDIUM;
      case spanner::RequestPriority::kHigh:
        return spanner_proto::RequestOptions::PRIORITY_HIGH;
    }
  }
  return spanner_proto::RequestOptions::PRIORITY_UNSPECIFIED;
}

// Operations that set `TransactionSelector::begin` in the request and receive
// a malformed response that does not contain a `Transaction` should invalidate
// the transaction with and also return this status.
Status MissingTransactionStatus(std::string const& operation) {
  return Status(StatusCode::kInternal,
                "Begin transaction requested but no transaction returned (in " +
                    operation + ")");
}

ConnectionImpl::ConnectionImpl(spanner::Database db,
                               std::vector<std::shared_ptr<SpannerStub>> stubs,
                               Options const& opts)
    : db_(std::move(db)),
      retry_policy_prototype_(
          opts.get<spanner::SpannerRetryPolicyOption>()->clone()),
      backoff_policy_prototype_(
          opts.get<spanner::SpannerBackoffPolicyOption>()->clone()),
      background_threads_(opts.get<GrpcBackgroundThreadsFactoryOption>()()),
      session_pool_(MakeSessionPool(db_, std::move(stubs),
                                    background_threads_->cq(), opts)),
      rpc_stream_tracing_enabled_(internal::Contains(
          opts.get<TracingComponentsOption>(), "rpc-streams")),
      tracing_options_(opts.get<GrpcTracingOptionsOption>()) {}

spanner::RowStream ConnectionImpl::Read(ReadParams params) {
  return Visit(std::move(params.transaction),
               [this, &params](SessionHolder& session,
                               StatusOr<spanner_proto::TransactionSelector>& s,
                               std::int64_t) {
                 return ReadImpl(session, s, std::move(params));
               });
}

StatusOr<std::vector<spanner::ReadPartition>> ConnectionImpl::PartitionRead(
    PartitionReadParams params) {
  return Visit(std::move(params.read_params.transaction),
               [this, &params](SessionHolder& session,
                               StatusOr<spanner_proto::TransactionSelector>& s,
                               std::int64_t) {
                 return PartitionReadImpl(session, s, params.read_params,
                                          params.partition_options);
               });
}

spanner::RowStream ConnectionImpl::ExecuteQuery(SqlParams params) {
  return Visit(std::move(params.transaction),
               [this, &params](SessionHolder& session,
                               StatusOr<spanner_proto::TransactionSelector>& s,
                               std::int64_t seqno) {
                 return ExecuteQueryImpl(session, s, seqno, std::move(params));
               });
}

StatusOr<spanner::DmlResult> ConnectionImpl::ExecuteDml(SqlParams params) {
  return Visit(std::move(params.transaction),
               [this, &params](SessionHolder& session,
                               StatusOr<spanner_proto::TransactionSelector>& s,
                               std::int64_t seqno) {
                 return ExecuteDmlImpl(session, s, seqno, std::move(params));
               });
}

spanner::ProfileQueryResult ConnectionImpl::ProfileQuery(SqlParams params) {
  return Visit(std::move(params.transaction),
               [this, &params](SessionHolder& session,
                               StatusOr<spanner_proto::TransactionSelector>& s,
                               std::int64_t seqno) {
                 return ProfileQueryImpl(session, s, seqno, std::move(params));
               });
}

StatusOr<spanner::ProfileDmlResult> ConnectionImpl::ProfileDml(
    SqlParams params) {
  return Visit(std::move(params.transaction),
               [this, &params](SessionHolder& session,
                               StatusOr<spanner_proto::TransactionSelector>& s,
                               std::int64_t seqno) {
                 return ProfileDmlImpl(session, s, seqno, std::move(params));
               });
}

StatusOr<spanner::ExecutionPlan> ConnectionImpl::AnalyzeSql(SqlParams params) {
  return Visit(std::move(params.transaction),
               [this, &params](SessionHolder& session,
                               StatusOr<spanner_proto::TransactionSelector>& s,
                               std::int64_t seqno) {
                 return AnalyzeSqlImpl(session, s, seqno, std::move(params));
               });
}

StatusOr<spanner::PartitionedDmlResult> ConnectionImpl::ExecutePartitionedDml(
    ExecutePartitionedDmlParams params) {
  auto txn = spanner::MakeReadOnlyTransaction();
  return Visit(
      txn, [this, &params](SessionHolder& session,
                           StatusOr<spanner_proto::TransactionSelector>& s,
                           std::int64_t seqno) {
        return ExecutePartitionedDmlImpl(session, s, seqno, std::move(params));
      });
}

StatusOr<std::vector<spanner::QueryPartition>> ConnectionImpl::PartitionQuery(
    PartitionQueryParams params) {
  return Visit(std::move(params.transaction),
               [this, &params](SessionHolder& session,
                               StatusOr<spanner_proto::TransactionSelector>& s,
                               std::int64_t) {
                 return PartitionQueryImpl(session, s, params);
               });
}

StatusOr<spanner::BatchDmlResult> ConnectionImpl::ExecuteBatchDml(
    ExecuteBatchDmlParams params) {
  return Visit(std::move(params.transaction),
               [this, &params](SessionHolder& session,
                               StatusOr<spanner_proto::TransactionSelector>& s,
                               std::int64_t seqno) {
                 return ExecuteBatchDmlImpl(session, s, seqno,
                                            std::move(params));
               });
}

StatusOr<spanner::CommitResult> ConnectionImpl::Commit(CommitParams params) {
  return Visit(std::move(params.transaction),
               [this, &params](SessionHolder& session,
                               StatusOr<spanner_proto::TransactionSelector>& s,
                               std::int64_t) {
                 return this->CommitImpl(session, s, std::move(params));
               });
}

Status ConnectionImpl::Rollback(RollbackParams params) {
  return Visit(std::move(params.transaction),
               [this](SessionHolder& session,
                      StatusOr<spanner_proto::TransactionSelector>& s,
                      std::int64_t) { return this->RollbackImpl(session, s); });
}

class StatusOnlyResultSetSource : public ResultSourceInterface {
 public:
  explicit StatusOnlyResultSetSource(google::cloud::Status status)
      : status_(std::move(status)) {}
  ~StatusOnlyResultSetSource() override = default;

  StatusOr<spanner::Row> NextRow() override { return status_; }
  absl::optional<google::spanner::v1::ResultSetMetadata> Metadata() override {
    return {};
  }
  absl::optional<google::spanner::v1::ResultSetStats> Stats() const override {
    return {};
  }

 private:
  google::cloud::Status status_;
};

// Helper function to build and wrap a `StatusOnlyResultSetSource`.
template <typename ResultType>
ResultType MakeStatusOnlyResult(Status status) {
  return ResultType(
      absl::make_unique<StatusOnlyResultSetSource>(std::move(status)));
}

class DmlResultSetSource : public ResultSourceInterface {
 public:
  static StatusOr<std::unique_ptr<ResultSourceInterface>> Create(
      spanner_proto::ResultSet result_set) {
    return std::unique_ptr<ResultSourceInterface>(
        new DmlResultSetSource(std::move(result_set)));
  }

  explicit DmlResultSetSource(spanner_proto::ResultSet result_set)
      : result_set_(std::move(result_set)) {}
  ~DmlResultSetSource() override = default;

  StatusOr<spanner::Row> NextRow() override { return {}; }

  absl::optional<google::spanner::v1::ResultSetMetadata> Metadata() override {
    if (result_set_.has_metadata()) {
      return result_set_.metadata();
    }
    return {};
  }

  absl::optional<google::spanner::v1::ResultSetStats> Stats() const override {
    if (result_set_.has_stats()) {
      return result_set_.stats();
    }
    return {};
  }

 private:
  spanner_proto::ResultSet result_set_;
};

// Used as an intermediary for streaming PartitionedDml operations.
class StreamingPartitionedDmlResult {
 public:
  StreamingPartitionedDmlResult() = default;
  explicit StreamingPartitionedDmlResult(
      std::unique_ptr<ResultSourceInterface> source)
      : source_(std::move(source)) {}

  // This class is movable but not copyable.
  StreamingPartitionedDmlResult(StreamingPartitionedDmlResult&&) = default;
  StreamingPartitionedDmlResult& operator=(StreamingPartitionedDmlResult&&) =
      default;

  /**
   * Returns a lower bound on the number of rows modified by the DML statement
   * on success.
   */
  StatusOr<std::int64_t> RowsModifiedLowerBound() const {
    StatusOr<spanner::Row> row;
    do {
      row = source_->NextRow();
      if (!row) return std::move(row).status();
      // We don't expect to get any data; if we do just drop it.
      // Exit the loop when we get an empty row.
    } while (row->size() != 0);

    return source_->Stats()->row_count_lower_bound();
  }

 private:
  std::unique_ptr<ResultSourceInterface> source_;
};

/**
 * Helper function that ensures `session` holds a valid `Session`, or returns
 * an error if `session` is empty and no `Session` can be allocated.
 */
Status ConnectionImpl::PrepareSession(SessionHolder& session,
                                      bool dissociate_from_pool) {
  if (!session) {
    auto session_or = session_pool_->Allocate(dissociate_from_pool);
    if (!session_or) {
      return std::move(session_or).status();
    }
    session = std::move(*session_or);
  }
  return Status();
}

/**
 * Performs an explicit `BeginTransaction` in cases where that is needed.
 *
 * @param session identifies the Session to use.
 * @param options `TransactionOptions` to use in the request.
 * @param func identifies the calling function for logging purposes.
 *   It should generally be passed the value of `__func__`.
 */
StatusOr<spanner_proto::Transaction> ConnectionImpl::BeginTransaction(
    SessionHolder& session, spanner_proto::TransactionOptions options,
    char const* func) {
  spanner_proto::BeginTransactionRequest begin;
  begin.set_session(session->session_name());
  *begin.mutable_options() = std::move(options);
  // `begin.request_options.priority` is ignored. To set the priority
  // for a transaction, set it on the reads and writes that are part of
  // the transaction instead.

  auto stub = session_pool_->GetStub(*session);
  auto response = RetryLoop(
      retry_policy_prototype_->clone(), backoff_policy_prototype_->clone(),
      Idempotency::kIdempotent,
      [&stub](grpc::ClientContext& context,
              spanner_proto::BeginTransactionRequest const& request) {
        return stub->BeginTransaction(context, request);
      },
      begin, func);
  if (!response) {
    auto status = std::move(response).status();
    if (IsSessionNotFound(status)) session->set_bad();
    return status;
  }
  return *response;
}

spanner::RowStream ConnectionImpl::ReadImpl(
    SessionHolder& session, StatusOr<spanner_proto::TransactionSelector>& s,
    ReadParams params) {
  if (!s.ok()) {
    return MakeStatusOnlyResult<spanner::RowStream>(s.status());
  }

  auto prepare_status = PrepareSession(session);
  if (!prepare_status.ok()) {
    return MakeStatusOnlyResult<spanner::RowStream>(std::move(prepare_status));
  }

  spanner_proto::ReadRequest request;
  request.set_session(session->session_name());
  *request.mutable_transaction() = *s;
  request.set_table(std::move(params.table));
  request.set_index(std::move(params.read_options.index_name));
  for (auto&& column : params.columns) {
    request.add_columns(std::move(column));
  }
  *request.mutable_key_set() = ToProto(std::move(params.keys));
  request.set_limit(params.read_options.limit);
  if (params.partition_token) {
    request.set_partition_token(*std::move(params.partition_token));
  }
  request.mutable_request_options()->set_priority(
      ProtoRequestPriority(params.read_options.request_priority));

  // Capture a copy of `stub` to ensure the `shared_ptr<>` remains valid through
  // the lifetime of the lambda.
  auto stub = session_pool_->GetStub(*session);
  auto const tracing_enabled = rpc_stream_tracing_enabled_;
  auto const tracing_options = tracing_options_;
  auto factory = [stub, &request, tracing_enabled,
                  tracing_options](std::string const& resume_token) mutable {
    request.set_resume_token(resume_token);
    auto context = absl::make_unique<grpc::ClientContext>();
    std::unique_ptr<PartialResultSetReader> reader =
        absl::make_unique<DefaultPartialResultSetReader>(
            std::move(context), stub->StreamingRead(*context, request));
    if (tracing_enabled) {
      reader = absl::make_unique<LoggingResultSetReader>(std::move(reader),
                                                         tracing_options);
    }
    return reader;
  };
  for (;;) {
    auto rpc = absl::make_unique<PartialResultSetResume>(
        factory, Idempotency::kIdempotent, retry_policy_prototype_->clone(),
        backoff_policy_prototype_->clone());
    auto reader = PartialResultSetSource::Create(std::move(rpc));
    if (s->has_begin()) {
      if (reader.ok()) {
        auto metadata = (*reader)->Metadata();
        if (!metadata || !metadata->has_transaction()) {
          s = MissingTransactionStatus(__func__);
          return MakeStatusOnlyResult<spanner::RowStream>(s.status());
        }
        s->set_id(metadata->transaction().id());
      } else {
        auto begin = BeginTransaction(session, s->begin(), __func__);
        if (begin.ok()) {
          s->set_id(begin->id());
          *request.mutable_transaction() = *s;
          continue;
        }
        s = begin.status();  // invalidate the transaction
      }
    }

    if (!reader.ok()) {
      auto status = std::move(reader).status();
      if (IsSessionNotFound(status)) session->set_bad();
      return MakeStatusOnlyResult<spanner::RowStream>(std::move(status));
    }
    return spanner::RowStream(*std::move(reader));
  }
}

StatusOr<std::vector<spanner::ReadPartition>> ConnectionImpl::PartitionReadImpl(
    SessionHolder& session, StatusOr<spanner_proto::TransactionSelector>& s,
    ReadParams const& params,
    spanner::PartitionOptions const& partition_options) {
  if (!s.ok()) {
    return s.status();
  }

  // Since the session may be sent to other machines, it should not be returned
  // to the pool when the Transaction is destroyed.
  auto prepare_status = PrepareSession(session, /*dissociate_from_pool=*/true);
  if (!prepare_status.ok()) {
    return prepare_status;
  }

  spanner_proto::PartitionReadRequest request;
  request.set_session(session->session_name());
  *request.mutable_transaction() = *s;
  request.set_table(params.table);
  request.set_index(params.read_options.index_name);
  for (auto const& column : params.columns) {
    *request.add_columns() = column;
  }
  *request.mutable_key_set() = ToProto(params.keys);
  *request.mutable_partition_options() = ToProto(partition_options);

  auto stub = session_pool_->GetStub(*session);
  for (;;) {
    auto response = RetryLoop(
        retry_policy_prototype_->clone(), backoff_policy_prototype_->clone(),
        Idempotency::kIdempotent,
        [&stub](grpc::ClientContext& context,
                spanner_proto::PartitionReadRequest const& request) {
          return stub->PartitionRead(context, request);
        },
        request, __func__);
    if (s->has_begin()) {
      if (response.ok()) {
        if (!response->has_transaction()) {
          s = MissingTransactionStatus(__func__);
          return s.status();
        }
        s->set_id(response->transaction().id());
      } else {
        auto begin = BeginTransaction(session, s->begin(), __func__);
        if (begin.ok()) {
          s->set_id(begin->id());
          *request.mutable_transaction() = *s;
          continue;
        }
        s = begin.status();  // invalidate the transaction
      }
    }

    if (!response.ok()) {
      auto status = std::move(response).status();
      if (IsSessionNotFound(status)) session->set_bad();
      return status;
    }

    std::vector<spanner::ReadPartition> read_partitions;
    for (auto const& partition : response->partitions()) {
      read_partitions.push_back(MakeReadPartition(
          response->transaction().id(), session->session_name(),
          partition.partition_token(), params.table, params.keys,
          params.columns, params.read_options));
    }

    return read_partitions;
  }
}

template <typename ResultType>
StatusOr<ResultType> ConnectionImpl::ExecuteSqlImpl(
    SessionHolder& session, StatusOr<spanner_proto::TransactionSelector>& s,
    std::int64_t seqno, SqlParams params,
    google::spanner::v1::ExecuteSqlRequest::QueryMode query_mode,
    std::function<StatusOr<std::unique_ptr<ResultSourceInterface>>(
        google::spanner::v1::ExecuteSqlRequest& request)> const&
        retry_resume_fn) {
  if (!s.ok()) {
    return s.status();
  }

  spanner_proto::ExecuteSqlRequest request;
  request.set_session(session->session_name());
  *request.mutable_transaction() = *s;
  auto sql_statement = ToProto(std::move(params.statement));
  request.set_sql(std::move(*sql_statement.mutable_sql()));
  *request.mutable_params() = std::move(*sql_statement.mutable_params());
  *request.mutable_param_types() =
      std::move(*sql_statement.mutable_param_types());
  request.set_seqno(seqno);
  request.set_query_mode(query_mode);
  if (params.partition_token) {
    request.set_partition_token(*std::move(params.partition_token));
  }
  if (params.query_options.optimizer_version()) {
    request.mutable_query_options()->set_optimizer_version(
        *params.query_options.optimizer_version());
  }
  if (params.query_options.optimizer_statistics_package()) {
    request.mutable_query_options()->set_optimizer_statistics_package(
        *params.query_options.optimizer_statistics_package());
  }
  request.mutable_request_options()->set_priority(
      ProtoRequestPriority(params.query_options.request_priority()));

  for (;;) {
    auto reader = retry_resume_fn(request);
    if (s->has_begin()) {
      if (reader.ok()) {
        auto metadata = (*reader)->Metadata();
        if (!metadata || !metadata->has_transaction()) {
          s = MissingTransactionStatus(__func__);
          return s.status();
        }
        s->set_id(metadata->transaction().id());
      } else {
        auto begin = BeginTransaction(session, s->begin(), __func__);
        if (begin.ok()) {
          s->set_id(begin->id());
          *request.mutable_transaction() = *s;
          continue;
        }
        s = begin.status();  // invalidate the transaction
      }
    }
    if (!reader.ok()) {
      return std::move(reader).status();
    }
    return ResultType(std::move(*reader));
  }
}

template <typename ResultType>
ResultType ConnectionImpl::CommonQueryImpl(
    SessionHolder& session, StatusOr<spanner_proto::TransactionSelector>& s,
    std::int64_t seqno, SqlParams params,
    google::spanner::v1::ExecuteSqlRequest::QueryMode query_mode) {
  if (!s.ok()) {
    return MakeStatusOnlyResult<ResultType>(s.status());
  }

  auto prepare_status = PrepareSession(session);
  if (!prepare_status.ok()) {
    return MakeStatusOnlyResult<ResultType>(std::move(prepare_status));
  }
  // Capture a copy of of these to ensure the `shared_ptr<>` remains valid
  // through the lifetime of the lambda. Note that the local variables are a
  // reference to avoid increasing refcounts twice, but the capture is by value.
  auto stub = session_pool_->GetStub(*session);
  auto const& retry_policy = retry_policy_prototype_;
  auto const& backoff_policy = backoff_policy_prototype_;
  auto const tracing_enabled = rpc_stream_tracing_enabled_;
  auto const tracing_options = tracing_options_;
  auto retry_resume_fn =
      [stub, retry_policy, backoff_policy, tracing_enabled,
       tracing_options](spanner_proto::ExecuteSqlRequest& request) mutable
      -> StatusOr<std::unique_ptr<ResultSourceInterface>> {
    auto factory = [stub, request, tracing_enabled,
                    tracing_options](std::string const& resume_token) mutable {
      request.set_resume_token(resume_token);
      auto context = absl::make_unique<grpc::ClientContext>();
      std::unique_ptr<PartialResultSetReader> reader =
          absl::make_unique<DefaultPartialResultSetReader>(
              std::move(context), stub->ExecuteStreamingSql(*context, request));
      if (tracing_enabled) {
        reader = absl::make_unique<LoggingResultSetReader>(std::move(reader),
                                                           tracing_options);
      }
      return reader;
    };
    auto rpc = absl::make_unique<PartialResultSetResume>(
        std::move(factory), Idempotency::kIdempotent, retry_policy->clone(),
        backoff_policy->clone());

    return PartialResultSetSource::Create(std::move(rpc));
  };

  StatusOr<ResultType> response =
      ExecuteSqlImpl<ResultType>(session, s, seqno, std::move(params),
                                 query_mode, std::move(retry_resume_fn));
  if (!response) {
    auto status = std::move(response).status();
    if (IsSessionNotFound(status)) session->set_bad();
    return MakeStatusOnlyResult<ResultType>(std::move(status));
  }
  return std::move(*response);
}

spanner::RowStream ConnectionImpl::ExecuteQueryImpl(
    SessionHolder& session, StatusOr<spanner_proto::TransactionSelector>& s,
    std::int64_t seqno, SqlParams params) {
  return CommonQueryImpl<spanner::RowStream>(
      session, s, seqno, std::move(params),
      spanner_proto::ExecuteSqlRequest::NORMAL);
}

spanner::ProfileQueryResult ConnectionImpl::ProfileQueryImpl(
    SessionHolder& session, StatusOr<spanner_proto::TransactionSelector>& s,
    std::int64_t seqno, SqlParams params) {
  return CommonQueryImpl<spanner::ProfileQueryResult>(
      session, s, seqno, std::move(params),
      spanner_proto::ExecuteSqlRequest::PROFILE);
}

template <typename ResultType>
StatusOr<ResultType> ConnectionImpl::CommonDmlImpl(
    SessionHolder& session, StatusOr<spanner_proto::TransactionSelector>& s,
    std::int64_t seqno, SqlParams params,
    google::spanner::v1::ExecuteSqlRequest::QueryMode query_mode) {
  if (!s.ok()) {
    return s.status();
  }
  auto function_name = __func__;
  auto prepare_status = PrepareSession(session);
  if (!prepare_status.ok()) {
    return prepare_status;
  }
  // Capture a copy of of these to ensure the `shared_ptr<>` remains valid
  // through the lifetime of the lambda. Note that the local variables are a
  // reference to avoid increasing refcounts twice, but the capture is by value.
  auto stub = session_pool_->GetStub(*session);
  auto const& retry_policy = retry_policy_prototype_;
  auto const& backoff_policy = backoff_policy_prototype_;

  auto retry_resume_fn =
      [function_name, stub, retry_policy, backoff_policy,
       session](spanner_proto::ExecuteSqlRequest& request) mutable
      -> StatusOr<std::unique_ptr<ResultSourceInterface>> {
    StatusOr<spanner_proto::ResultSet> response = RetryLoop(
        retry_policy->clone(), backoff_policy->clone(),
        Idempotency::kIdempotent,
        [stub](grpc::ClientContext& context,
               spanner_proto::ExecuteSqlRequest const& request) {
          return stub->ExecuteSql(context, request);
        },
        request, function_name);
    if (!response) {
      auto status = std::move(response).status();
      if (IsSessionNotFound(status)) session->set_bad();
      return status;
    }
    return DmlResultSetSource::Create(std::move(*response));
  };
  return ExecuteSqlImpl<ResultType>(session, s, seqno, std::move(params),
                                    query_mode, std::move(retry_resume_fn));
}

StatusOr<spanner::DmlResult> ConnectionImpl::ExecuteDmlImpl(
    SessionHolder& session, StatusOr<spanner_proto::TransactionSelector>& s,
    std::int64_t seqno, SqlParams params) {
  return CommonDmlImpl<spanner::DmlResult>(
      session, s, seqno, std::move(params),
      spanner_proto::ExecuteSqlRequest::NORMAL);
}

StatusOr<spanner::ProfileDmlResult> ConnectionImpl::ProfileDmlImpl(
    SessionHolder& session, StatusOr<spanner_proto::TransactionSelector>& s,
    std::int64_t seqno, SqlParams params) {
  return CommonDmlImpl<spanner::ProfileDmlResult>(
      session, s, seqno, std::move(params),
      spanner_proto::ExecuteSqlRequest::PROFILE);
}

StatusOr<spanner::ExecutionPlan> ConnectionImpl::AnalyzeSqlImpl(
    SessionHolder& session, StatusOr<spanner_proto::TransactionSelector>& s,
    std::int64_t seqno, SqlParams params) {
  auto result = CommonDmlImpl<spanner::ProfileDmlResult>(
      session, s, seqno, std::move(params),
      spanner_proto::ExecuteSqlRequest::PLAN);
  if (result.status().ok()) {
    return *result->ExecutionPlan();
  }
  return result.status();
}

StatusOr<std::vector<spanner::QueryPartition>>
ConnectionImpl::PartitionQueryImpl(
    SessionHolder& session, StatusOr<spanner_proto::TransactionSelector>& s,
    PartitionQueryParams const& params) {
  if (!s.ok()) {
    return s.status();
  }

  // Since the session may be sent to other machines, it should not be returned
  // to the pool when the Transaction is destroyed.
  auto prepare_status = PrepareSession(session, /*dissociate_from_pool=*/true);
  if (!prepare_status.ok()) {
    return prepare_status;
  }

  spanner_proto::PartitionQueryRequest request;
  request.set_session(session->session_name());
  *request.mutable_transaction() = *s;
  auto sql_statement = ToProto(params.statement);
  request.set_sql(std::move(*sql_statement.mutable_sql()));
  *request.mutable_params() = std::move(*sql_statement.mutable_params());
  *request.mutable_param_types() =
      std::move(*sql_statement.mutable_param_types());
  *request.mutable_partition_options() =
      ToProto(std::move(params.partition_options));

  auto stub = session_pool_->GetStub(*session);
  for (;;) {
    auto response = RetryLoop(
        retry_policy_prototype_->clone(), backoff_policy_prototype_->clone(),
        Idempotency::kIdempotent,
        [&stub](grpc::ClientContext& context,
                spanner_proto::PartitionQueryRequest const& request) {
          return stub->PartitionQuery(context, request);
        },
        request, __func__);
    if (s->has_begin()) {
      if (response.ok()) {
        if (!response->has_transaction()) {
          s = MissingTransactionStatus(__func__);
          return s.status();
        }
        s->set_id(response->transaction().id());
      } else {
        auto begin = BeginTransaction(session, s->begin(), __func__);
        if (begin.ok()) {
          s->set_id(begin->id());
          *request.mutable_transaction() = *s;
          continue;
        }
        s = begin.status();  // invalidate the transaction
      }
    }
    if (!response.ok()) {
      auto status = std::move(response).status();
      if (IsSessionNotFound(status)) session->set_bad();
      return status;
    }

    std::vector<spanner::QueryPartition> query_partitions;
    for (auto const& partition : response->partitions()) {
      query_partitions.push_back(MakeQueryPartition(
          response->transaction().id(), session->session_name(),
          partition.partition_token(), params.statement));
    }
    return query_partitions;
  }
}

StatusOr<spanner::BatchDmlResult> ConnectionImpl::ExecuteBatchDmlImpl(
    SessionHolder& session, StatusOr<spanner_proto::TransactionSelector>& s,
    std::int64_t seqno, ExecuteBatchDmlParams params) {
  if (!s.ok()) {
    return s.status();
  }

  auto prepare_status = PrepareSession(session);
  if (!prepare_status.ok()) {
    return prepare_status;
  }

  spanner_proto::ExecuteBatchDmlRequest request;
  request.set_session(session->session_name());
  request.set_seqno(seqno);
  *request.mutable_transaction() = *s;
  for (auto& sql : params.statements) {
    *request.add_statements() = ToProto(std::move(sql));
  }
  request.mutable_request_options()->set_priority(
      params.options.has<spanner::RequestPriorityOption>()
          ? ProtoRequestPriority(
                params.options.lookup<spanner::RequestPriorityOption>())
          : spanner_proto::RequestOptions::PRIORITY_UNSPECIFIED);

  auto stub = session_pool_->GetStub(*session);
  for (;;) {
    auto response = RetryLoop(
        retry_policy_prototype_->clone(), backoff_policy_prototype_->clone(),
        Idempotency::kIdempotent,
        [&stub](grpc::ClientContext& context,
                spanner_proto::ExecuteBatchDmlRequest const& request) {
          return stub->ExecuteBatchDml(context, request);
        },
        request, __func__);
    if (s->has_begin()) {
      if (response.ok() && response->result_sets_size() > 0) {
        if (!response->result_sets(0).metadata().has_transaction()) {
          s = MissingTransactionStatus(__func__);
          return s.status();
        }
        s->set_id(response->result_sets(0).metadata().transaction().id());
      } else {
        auto begin = BeginTransaction(session, s->begin(), __func__);
        if (begin.ok()) {
          s->set_id(begin->id());
          *request.mutable_transaction() = *s;
          continue;
        }
        s = begin.status();  // invalidate the transaction
      }
    }
    if (!response) {
      auto status = std::move(response).status();
      if (IsSessionNotFound(status)) session->set_bad();
      return status;
    }
    spanner::BatchDmlResult result;
    result.status = google::cloud::MakeStatusFromRpcError(response->status());
    for (auto const& result_set : response->result_sets()) {
      result.stats.push_back({result_set.stats().row_count_exact()});
    }
    return result;
  }
}

StatusOr<spanner::PartitionedDmlResult>
ConnectionImpl::ExecutePartitionedDmlImpl(
    SessionHolder& session, StatusOr<spanner_proto::TransactionSelector>& s,
    std::int64_t seqno, ExecutePartitionedDmlParams params) {
  if (!s.ok()) {
    return s.status();
  }

  auto prepare_status = PrepareSession(session);
  if (!prepare_status.ok()) {
    return prepare_status;
  }
  auto begin =
      BeginTransaction(session, PartitionedDmlTransactionOptions(), __func__);
  if (!begin.ok()) {
    s = begin.status();  // invalidate the transaction
    return begin.status();
  }
  s->set_id(begin->id());

  SqlParams sql_params(
      {MakeTransactionFromIds(session->session_name(), begin->id()),
       std::move(params.statement), std::move(params.query_options),
       /*partition_token=*/{}});
  auto dml_result = CommonQueryImpl<StreamingPartitionedDmlResult>(
      session, s, seqno, std::move(sql_params),
      spanner_proto::ExecuteSqlRequest::NORMAL);
  auto rows_modified = dml_result.RowsModifiedLowerBound();
  if (!rows_modified.ok()) {
    auto status = std::move(rows_modified).status();
    if (IsSessionNotFound(status)) session->set_bad();
    return status;
  }
  spanner::PartitionedDmlResult result{0};
  result.row_count_lower_bound = *rows_modified;
  return result;
}

StatusOr<spanner::CommitResult> ConnectionImpl::CommitImpl(
    SessionHolder& session, StatusOr<spanner_proto::TransactionSelector>& s,
    CommitParams params) {
  if (!s.ok()) {
    // Fail the commit if the transaction has been invalidated.
    return s.status();
  }

  auto prepare_status = PrepareSession(session);
  if (!prepare_status.ok()) {
    return prepare_status;
  }

  spanner_proto::CommitRequest request;
  request.set_session(session->session_name());
  for (auto&& m : params.mutations) {
    *request.add_mutations() = std::move(m).as_proto();
  }
  request.set_return_commit_stats(params.options.return_stats());
  request.mutable_request_options()->set_priority(
      ProtoRequestPriority(params.options.request_priority()));

  if (s->selector_case() != spanner_proto::TransactionSelector::kId) {
    auto begin = BeginTransaction(
        session, s->has_begin() ? s->begin() : s->single_use(), __func__);
    if (!begin.ok()) {
      s = begin.status();  // invalidate the transaction
      return begin.status();
    }
    s->set_id(begin->id());
  }
  request.set_transaction_id(s->id());

  auto stub = session_pool_->GetStub(*session);
  auto response = RetryLoop(
      retry_policy_prototype_->clone(), backoff_policy_prototype_->clone(),
      Idempotency::kIdempotent,
      [&stub](grpc::ClientContext& context,
              spanner_proto::CommitRequest const& request) {
        return stub->Commit(context, request);
      },
      request, __func__);
  if (!response) {
    auto status = std::move(response).status();
    if (IsSessionNotFound(status)) session->set_bad();
    return status;
  }
  auto timestamp = spanner::MakeTimestamp(response->commit_timestamp());
  if (!timestamp) {
    // The response commit_timestamp is out of range, but the commit was
    // successful so we cannot indicate an error. This should not happen,
    // but if it does we set r.commit_timestamp to its maximal value.
    protobuf::Timestamp proto;
    proto.set_seconds(google::protobuf::util::TimeUtil::kTimestampMaxSeconds);
    proto.set_nanos(999999999);
    timestamp = spanner::MakeTimestamp(proto);
  }
  spanner::CommitResult r;
  r.commit_timestamp = *std::move(timestamp);
  if (response->has_commit_stats()) {
    r.commit_stats.emplace(
        spanner::CommitStats{response->commit_stats().mutation_count()});
  }
  return r;
}

Status ConnectionImpl::RollbackImpl(
    SessionHolder& session, StatusOr<spanner_proto::TransactionSelector>& s) {
  if (!s.ok()) {
    return s.status();
  }
  if (s->has_single_use()) {
    return Status(StatusCode::kInvalidArgument,
                  "Cannot rollback a single-use transaction");
  }

  auto prepare_status = PrepareSession(session);
  if (!prepare_status.ok()) {
    return prepare_status;
  }

  if (s->has_begin()) {
    auto begin = BeginTransaction(session, s->begin(), __func__);
    if (!begin.ok()) {
      s = begin.status();  // invalidate the transaction
      return begin.status();
    }
    s->set_id(begin->id());
  }

  spanner_proto::RollbackRequest request;
  request.set_session(session->session_name());
  request.set_transaction_id(s->id());
  auto stub = session_pool_->GetStub(*session);
  auto status = RetryLoop(
      retry_policy_prototype_->clone(), backoff_policy_prototype_->clone(),
      Idempotency::kIdempotent,
      [&stub](grpc::ClientContext& context,
              spanner_proto::RollbackRequest const& request) {
        return stub->Rollback(context, request);
      },
      request, __func__);
  if (IsSessionNotFound(status)) session->set_bad();
  return status;
}

}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner_internal
}  // namespace cloud
}  // namespace google
