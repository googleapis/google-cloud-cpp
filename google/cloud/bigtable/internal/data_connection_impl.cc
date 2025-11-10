// Copyright 2022 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "google/cloud/bigtable/internal/data_connection_impl.h"
#include "google/cloud/bigtable/internal/async_bulk_apply.h"
#include "google/cloud/bigtable/internal/async_row_reader.h"
#include "google/cloud/bigtable/internal/async_row_sampler.h"
#include "google/cloud/bigtable/internal/bulk_mutator.h"
#include "google/cloud/bigtable/internal/default_row_reader.h"
#include "google/cloud/bigtable/internal/defaults.h"
#include "google/cloud/bigtable/internal/logging_result_set_reader.h"
#include "google/cloud/bigtable/internal/operation_context.h"
#include "google/cloud/bigtable/internal/partial_result_set_reader.h"
#include "google/cloud/bigtable/internal/partial_result_set_resume.h"
#include "google/cloud/bigtable/internal/partial_result_set_source.h"
#include "google/cloud/bigtable/internal/retry_traits.h"
#include "google/cloud/bigtable/internal/rpc_policy_parameters.h"
#include "google/cloud/bigtable/options.h"
#include "google/cloud/bigtable/results.h"
#include "google/cloud/bigtable/retry_policy.h"
#include "google/cloud/background_threads.h"
#include "google/cloud/grpc_options.h"
#include "google/cloud/idempotency.h"
#include "google/cloud/internal/async_retry_loop.h"
#include "google/cloud/internal/make_status.h"
#include "google/cloud/internal/random.h"
#include "google/cloud/internal/retry_loop.h"
#include "google/cloud/internal/streaming_read_rpc.h"
#include "absl/functional/bind_front.h"
#include <memory>
#include <string>

namespace google {
namespace cloud {
namespace bigtable_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

inline std::string const& app_profile_id(Options const& options) {
  return options.get<bigtable::AppProfileIdOption>();
}

inline std::unique_ptr<bigtable::DataRetryPolicy> retry_policy(
    Options const& options) {
  return options.get<bigtable::DataRetryPolicyOption>()->clone();
}

inline std::unique_ptr<bigtable::DataRetryPolicy>
query_plan_refresh_retry_policy(Options const& options) {
  return options
      .get<bigtable::experimental::QueryPlanRefreshRetryPolicyOption>()
      ->clone();
}

inline std::unique_ptr<BackoffPolicy> backoff_policy(Options const& options) {
  return options.get<bigtable::DataBackoffPolicyOption>()->clone();
}

inline std::unique_ptr<bigtable::IdempotentMutationPolicy> idempotency_policy(
    Options const& options) {
  return options.get<bigtable::IdempotentMutationPolicyOption>()->clone();
}

inline bool enable_server_retries(Options const& options) {
  return options.get<EnableServerRetriesOption>();
}

inline bool RpcStreamTracingEnabled() {
  return internal::Contains(
      internal::CurrentOptions().get<LoggingComponentsOption>(), "rpc-streams");
}

inline TracingOptions const& RpcTracingOptions() {
  return internal::CurrentOptions().get<GrpcTracingOptionsOption>();
}

// This function allows for ReadRow and ReadRowsFull to provide an instance of
// an OperationContext specific to that operation.
bigtable::RowReader ReadRowsHelper(
    std::shared_ptr<BigtableStub>& stub,
    internal::ImmutableOptions const& current,
    bigtable::ReadRowsParams
        params,  // NOLINT(performance-unnecessary-value-param)
    std::shared_ptr<OperationContext>
        operation_context) {  // NOLINT(performance-unnecessary-value-param)
  auto impl = std::make_shared<DefaultRowReader>(
      stub, std::move(params.app_profile_id), std::move(params.table_name),
      std::move(params.row_set), params.rows_limit, std::move(params.filter),
      params.reverse, retry_policy(*current), backoff_policy(*current),
      enable_server_retries(*current), std::move(operation_context));
  return MakeRowReader(std::move(impl));
}

class DefaultPartialResultSetReader
    : public bigtable_internal::PartialResultSetReader {
 public:
  DefaultPartialResultSetReader(std::shared_ptr<grpc::ClientContext> context,
                                std::unique_ptr<internal::StreamingReadRpc<
                                    google::bigtable::v2::ExecuteQueryResponse>>
                                    reader)
      : context_(std::move(context)), reader_(std::move(reader)) {}

  ~DefaultPartialResultSetReader() override = default;

  void TryCancel() override { context_->TryCancel(); }

  bool Read(absl::optional<std::string> const&,
            bigtable_internal::UnownedPartialResultSet& result_set) override {
    while (true) {
      google::bigtable::v2::ExecuteQueryResponse response;
      absl::optional<google::cloud::Status> status = reader_->Read(&response);

      if (status.has_value()) {
        // Stream has ended or an error occurred.
        final_status_ = *std::move(status);
        return false;
      }

      // Message successfully read into response.
      if (response.has_results()) {
        result_set.result = std::move(*response.mutable_results());
        result_set.resumption = false;
        return true;
      }

      // Ignore metadata from the stream because PartialResultSetSource already
      // has it set (in ExecuteQuery).
      // TODO(#15701): Investigate expected behavior for processing metadata.
      if (response.has_metadata()) {
        continue;
      }

      final_status_ = google::cloud::Status(
          google::cloud::StatusCode::kInternal,
          "Empty ExecuteQueryResponse received from stream");
      return false;
    }
  }

  grpc::ClientContext const& context() const override { return *context_; }

  Status Finish() override {
    //    operation_context_->OnDone(final_status_);
    return final_status_;
  }

 private:
  std::shared_ptr<grpc::ClientContext> context_;
  std::unique_ptr<
      internal::StreamingReadRpc<google::bigtable::v2::ExecuteQueryResponse>>
      reader_;
  Status final_status_;
};

template <typename ResultType>
ResultType MakeStatusOnlyResult(Status status) {
  return ResultType(
      std::make_unique<StatusOnlyResultSetSource>(std::move(status)));
}

}  // namespace

bigtable::Row TransformReadModifyWriteRowResponse(
    google::bigtable::v2::ReadModifyWriteRowResponse response) {
  std::vector<bigtable::Cell> cells;
  auto& row = *response.mutable_row();
  for (auto& family : *row.mutable_families()) {
    for (auto& column : *family.mutable_columns()) {
      for (auto& cell : *column.mutable_cells()) {
        std::vector<std::string> labels;
        std::move(cell.mutable_labels()->begin(), cell.mutable_labels()->end(),
                  std::back_inserter(labels));
        cells.emplace_back(row.key(), family.name(), column.qualifier(),
                           cell.timestamp_micros(),
                           std::move(*cell.mutable_value()), std::move(labels));
      }
    }
  }

  return bigtable::Row(std::move(*row.mutable_key()), std::move(cells));
}

DataConnectionImpl::DataConnectionImpl(
    std::unique_ptr<BackgroundThreads> background,
    std::shared_ptr<BigtableStub> stub,
    std::shared_ptr<MutateRowsLimiter> limiter, Options options)
    : background_(std::move(background)),
      stub_(std::move(stub)),
      limiter_(std::move(limiter)),
      options_(internal::MergeOptions(std::move(options),
                                      DataConnection::options())) {
#ifdef GOOGLE_CLOUD_CPP_BIGTABLE_WITH_OTEL_METRICS
  if (options_.get<bigtable::EnableMetricsOption>()) {
    // The client_uid is eventually used in conjunction with other data labels
    // to identify metric data points. This pseudorandom string is used to aid
    // in disambiguation.
    auto gen = internal::MakeDefaultPRNG();
    std::string client_uid =
        internal::Sample(gen, 16, "abcdefghijklmnopqrstuvwxyz0123456789");
    operation_context_factory_ =
        std::make_unique<MetricsOperationContextFactory>(std::move(client_uid),
                                                         options_);
  } else {
    operation_context_factory_ =
        std::make_unique<SimpleOperationContextFactory>();
  }
#else
  operation_context_factory_ =
      std::make_unique<SimpleOperationContextFactory>();
#endif
}

DataConnectionImpl::DataConnectionImpl(
    std::unique_ptr<BackgroundThreads> background,
    std::shared_ptr<BigtableStub> stub,
    std::unique_ptr<OperationContextFactory> operation_context_factory,
    std::shared_ptr<MutateRowsLimiter> limiter, Options options)
    : background_(std::move(background)),
      stub_(std::move(stub)),
      operation_context_factory_(std::move(operation_context_factory)),
      limiter_(std::move(limiter)),
      options_(internal::MergeOptions(std::move(options),
                                      DataConnection::options())) {}

Status DataConnectionImpl::Apply(std::string const& table_name,
                                 bigtable::SingleRowMutation mut) {
  auto current = google::cloud::internal::SaveCurrentOptions();
  google::bigtable::v2::MutateRowRequest request;
  request.set_app_profile_id(app_profile_id(*current));
  request.set_table_name(table_name);
  mut.MoveTo(request);

  auto idempotent_policy = idempotency_policy(*current);
  bool const is_idempotent = std::all_of(
      request.mutations().begin(), request.mutations().end(),
      [&idempotent_policy](google::bigtable::v2::Mutation const& m) {
        return idempotent_policy->is_idempotent(m);
      });

  auto operation_context = operation_context_factory_->MutateRow(
      table_name, app_profile_id(*current));
  auto sor = google::cloud::internal::RetryLoop(
      retry_policy(*current), backoff_policy(*current),
      is_idempotent ? Idempotency::kIdempotent : Idempotency::kNonIdempotent,
      [this, &operation_context](
          grpc::ClientContext& context, Options const& options,
          google::bigtable::v2::MutateRowRequest const& request) {
        operation_context->PreCall(context);
        auto s = stub_->MutateRow(context, options, request);
        operation_context->PostCall(context, s.status());
        return s;
      },
      *current, request, __func__);
  operation_context->OnDone(sor.status());
  if (!sor) return std::move(sor).status();
  return Status{};
}

future<Status> DataConnectionImpl::AsyncApply(std::string const& table_name,
                                              bigtable::SingleRowMutation mut) {
  auto current = google::cloud::internal::SaveCurrentOptions();
  google::bigtable::v2::MutateRowRequest request;
  request.set_app_profile_id(app_profile_id(*current));
  request.set_table_name(table_name);
  mut.MoveTo(request);

  auto idempotent_policy = idempotency_policy(*current);
  bool const is_idempotent = std::all_of(
      request.mutations().begin(), request.mutations().end(),
      [&idempotent_policy](google::bigtable::v2::Mutation const& m) {
        return idempotent_policy->is_idempotent(m);
      });

  auto operation_context = operation_context_factory_->MutateRow(
      table_name, app_profile_id(*current));
  auto retry = retry_policy(*current);
  auto backoff = backoff_policy(*current);
  return google::cloud::internal::AsyncRetryLoop(
             std::move(retry), std::move(backoff),
             is_idempotent ? Idempotency::kIdempotent
                           : Idempotency::kNonIdempotent,
             background_->cq(),
             [stub = stub_, operation_context](
                 CompletionQueue& cq,
                 std::shared_ptr<grpc::ClientContext> context,
                 google::cloud::internal::ImmutableOptions options,
                 google::bigtable::v2::MutateRowRequest const& request) {
               operation_context->PreCall(*context);
               auto f = stub->AsyncMutateRow(cq, context, std::move(options),
                                             request);
               return f.then(
                   [operation_context, context = std::move(context)](auto f) {
                     auto s = f.get();
                     operation_context->PostCall(*context, s.status());
                     return s;
                   });
             },
             std::move(current), request, __func__)
      .then([operation_context](
                future<StatusOr<google::bigtable::v2::MutateRowResponse>> f) {
        auto sor = f.get();
        operation_context->OnDone(sor.status());
        if (!sor) return std::move(sor).status();
        return Status{};
      });
}

std::vector<bigtable::FailedMutation> DataConnectionImpl::BulkApply(
    std::string const& table_name, bigtable::BulkMutation mut) {
  auto current = google::cloud::internal::SaveCurrentOptions();
  if (mut.empty()) return {};
  auto operation_context = operation_context_factory_->MutateRows(
      table_name, app_profile_id(*current));
  BulkMutator mutator(app_profile_id(*current), table_name,
                      *idempotency_policy(*current), std::move(mut),
                      operation_context);
  // We wait to allocate the policies until they are needed as a
  // micro-optimization.
  std::unique_ptr<bigtable::DataRetryPolicy> retry;
  std::unique_ptr<BackoffPolicy> backoff;
  Status status;
  while (true) {
    status = mutator.MakeOneRequest(*stub_, *limiter_, *current);
    if (!mutator.HasPendingMutations()) break;
    if (!retry) retry = retry_policy(*current);
    if (!backoff) backoff = backoff_policy(*current);
    auto delay = internal::Backoff(status, "BulkApply", *retry, *backoff,
                                   Idempotency::kIdempotent,
                                   enable_server_retries(*current));
    if (!delay) break;
    std::this_thread::sleep_for(*delay);
  }
  operation_context->OnDone(status);
  return std::move(mutator).OnRetryDone();
}

future<std::vector<bigtable::FailedMutation>>
DataConnectionImpl::AsyncBulkApply(std::string const& table_name,
                                   bigtable::BulkMutation mut) {
  auto current = google::cloud::internal::SaveCurrentOptions();
  auto operation_context = operation_context_factory_->MutateRows(
      table_name, app_profile_id(*current));
  return AsyncBulkApplier::Create(
      background_->cq(), stub_, limiter_, retry_policy(*current),
      backoff_policy(*current), enable_server_retries(*current),
      *idempotency_policy(*current), app_profile_id(*current), table_name,
      std::move(mut), std::move(operation_context));
}

bigtable::RowReader DataConnectionImpl::ReadRowsFull(
    bigtable::ReadRowsParams params) {
  auto current = google::cloud::internal::SaveCurrentOptions();
  auto operation_context = operation_context_factory_->ReadRows(
      params.table_name, params.app_profile_id);
  return ReadRowsHelper(stub_, current, std::move(params),
                        std::move(operation_context));
}

StatusOr<std::pair<bool, bigtable::Row>> DataConnectionImpl::ReadRow(
    std::string const& table_name, std::string row_key,
    bigtable::Filter filter) {
  auto current = google::cloud::internal::SaveCurrentOptions();
  bigtable::RowSet row_set(std::move(row_key));
  std::int64_t const rows_limit = 1;
  // TODO(sdhart): ensure OperationContext::OnDone is called correctly.
  // TODO(sdhart): add ReadRow tests once we call
  //  OperationContextFactory::ReadRow to create the operation_context.
  auto operation_context =
      operation_context_factory_->ReadRow(table_name, app_profile_id(*current));
  auto reader =
      ReadRowsHelper(stub_, current,
                     bigtable::ReadRowsParams{
                         table_name, app_profile_id(*current),
                         std::move(row_set), rows_limit, std::move(filter)},
                     std::move(operation_context));

  auto it = reader.begin();
  if (it == reader.end()) return std::make_pair(false, bigtable::Row("", {}));
  if (!*it) return it->status();
  auto result = std::make_pair(true, std::move(**it));
  if (++it != reader.end()) {
    return internal::InternalError(
        "internal error - RowReader returned more than one row in ReadRow(, "
        ")",
        GCP_ERROR_INFO());
  }
  return result;
}

StatusOr<bigtable::MutationBranch> DataConnectionImpl::CheckAndMutateRow(
    std::string const& table_name, std::string row_key, bigtable::Filter filter,
    std::vector<bigtable::Mutation> true_mutations,
    std::vector<bigtable::Mutation> false_mutations) {
  auto current = google::cloud::internal::SaveCurrentOptions();
  google::bigtable::v2::CheckAndMutateRowRequest request;
  request.set_app_profile_id(app_profile_id(*current));
  request.set_table_name(table_name);
  request.set_row_key(std::move(row_key));
  *request.mutable_predicate_filter() = std::move(filter).as_proto();
  for (auto& m : true_mutations) {
    *request.add_true_mutations() = std::move(m.op);
  }
  for (auto& m : false_mutations) {
    *request.add_false_mutations() = std::move(m.op);
  }
  auto const idempotency = idempotency_policy(*current)->is_idempotent(request)
                               ? Idempotency::kIdempotent
                               : Idempotency::kNonIdempotent;
  auto operation_context = operation_context_factory_->CheckAndMutateRow(
      table_name, app_profile_id(*current));
  auto sor = google::cloud::internal::RetryLoop(
      retry_policy(*current), backoff_policy(*current), idempotency,
      [this, &operation_context](
          grpc::ClientContext& context, Options const& options,
          google::bigtable::v2::CheckAndMutateRowRequest const& request) {
        operation_context->PreCall(context);
        auto s = stub_->CheckAndMutateRow(context, options, request);
        operation_context->PostCall(context, s.status());
        return s;
      },
      *current, request, __func__);
  operation_context->OnDone(sor.status());
  if (!sor) return std::move(sor).status();
  auto response = *std::move(sor);
  return response.predicate_matched()
             ? bigtable::MutationBranch::kPredicateMatched
             : bigtable::MutationBranch::kPredicateNotMatched;
}

future<StatusOr<bigtable::MutationBranch>>
DataConnectionImpl::AsyncCheckAndMutateRow(
    std::string const& table_name, std::string row_key, bigtable::Filter filter,
    std::vector<bigtable::Mutation> true_mutations,
    std::vector<bigtable::Mutation> false_mutations) {
  auto current = google::cloud::internal::SaveCurrentOptions();
  google::bigtable::v2::CheckAndMutateRowRequest request;
  request.set_app_profile_id(app_profile_id(*current));
  request.set_table_name(table_name);
  request.set_row_key(std::move(row_key));
  *request.mutable_predicate_filter() = std::move(filter).as_proto();
  for (auto& m : true_mutations) {
    *request.add_true_mutations() = std::move(m.op);
  }
  for (auto& m : false_mutations) {
    *request.add_false_mutations() = std::move(m.op);
  }
  auto const idempotency = idempotency_policy(*current)->is_idempotent(request)
                               ? Idempotency::kIdempotent
                               : Idempotency::kNonIdempotent;

  auto retry = retry_policy(*current);
  auto backoff = backoff_policy(*current);
  auto operation_context = operation_context_factory_->CheckAndMutateRow(
      table_name, app_profile_id(*current));
  return google::cloud::internal::AsyncRetryLoop(
             std::move(retry), std::move(backoff), idempotency,
             background_->cq(),
             [stub = stub_, operation_context](
                 CompletionQueue& cq,
                 std::shared_ptr<grpc::ClientContext> context,
                 google::cloud::internal::ImmutableOptions options,
                 google::bigtable::v2::CheckAndMutateRowRequest const&
                     request) {
               operation_context->PreCall(*context);
               auto f = stub->AsyncCheckAndMutateRow(
                   cq, context, std::move(options), request);
               return f.then(
                   [operation_context, context = std::move(context)](auto f) {
                     auto s = f.get();
                     operation_context->PostCall(*context, s.status());
                     return s;
                   });
             },
             std::move(current), request, __func__)
      .then(
          [operation_context](
              future<StatusOr<google::bigtable::v2::CheckAndMutateRowResponse>>
                  f) -> StatusOr<bigtable::MutationBranch> {
            auto sor = f.get();
            operation_context->OnDone(sor.status());
            if (!sor) return std::move(sor).status();
            auto response = *std::move(sor);
            return response.predicate_matched()
                       ? bigtable::MutationBranch::kPredicateMatched
                       : bigtable::MutationBranch::kPredicateNotMatched;
          });
}

StatusOr<std::vector<bigtable::RowKeySample>> DataConnectionImpl::SampleRows(
    std::string const& table_name) {
  auto current = google::cloud::internal::SaveCurrentOptions();
  google::bigtable::v2::SampleRowKeysRequest request;
  request.set_app_profile_id(app_profile_id(*current));
  request.set_table_name(table_name);

  std::vector<bigtable::RowKeySample> samples;
  std::unique_ptr<bigtable::DataRetryPolicy> retry;
  std::unique_ptr<BackoffPolicy> backoff;
  auto operation_context = operation_context_factory_->SampleRowKeys(
      table_name, app_profile_id(*current));
  while (true) {
    auto context = std::make_shared<grpc::ClientContext>();
    internal::ConfigureContext(*context, internal::CurrentOptions());
    operation_context->PreCall(*context);
    auto stream = stub_->SampleRowKeys(context, Options{}, request);

    absl::optional<Status> status;
    while (true) {
      google::bigtable::v2::SampleRowKeysResponse r;
      status = stream->Read(&r);
      if (status.has_value()) {
        break;
      }
      bigtable::RowKeySample row_sample;
      row_sample.offset_bytes = r.offset_bytes();
      row_sample.row_key = std::move(*r.mutable_row_key());
      samples.emplace_back(std::move(row_sample));
    }
    operation_context->PostCall(*context, *status);
    if (status->ok()) break;
    // We wait to allocate the policies until they are needed as a
    // micro-optimization.
    if (!retry) retry = retry_policy(*current);
    if (!backoff) backoff = backoff_policy(*current);
    auto delay = internal::Backoff(*status, "SampleRows", *retry, *backoff,
                                   Idempotency::kIdempotent,
                                   enable_server_retries(*current));
    if (!delay) {
      operation_context->OnDone(delay.status());
      return std::move(delay).status();
    }
    // A new stream invalidates previously returned samples.
    samples.clear();
    std::this_thread::sleep_for(*delay);
  }
  operation_context->OnDone({});
  return samples;
}

future<StatusOr<std::vector<bigtable::RowKeySample>>>
DataConnectionImpl::AsyncSampleRows(std::string const& table_name) {
  auto current = google::cloud::internal::SaveCurrentOptions();
  auto operation_context = operation_context_factory_->SampleRowKeys(
      table_name, app_profile_id(*current));
  return AsyncRowSampler::Create(
      background_->cq(), stub_, retry_policy(*current),
      backoff_policy(*current), enable_server_retries(*current),
      app_profile_id(*current), table_name, std::move(operation_context));
}

StatusOr<bigtable::Row> DataConnectionImpl::ReadModifyWriteRow(
    google::bigtable::v2::ReadModifyWriteRowRequest request) {
  auto current = google::cloud::internal::SaveCurrentOptions();
  auto operation_context = operation_context_factory_->ReadModifyWriteRow(
      request.table_name(), app_profile_id(*current));
  auto sor = google::cloud::internal::RetryLoop(
      retry_policy(*current), backoff_policy(*current),
      Idempotency::kNonIdempotent,
      [this, operation_context](
          grpc::ClientContext& context, Options const& options,
          google::bigtable::v2::ReadModifyWriteRowRequest const& request) {
        operation_context->PreCall(context);
        auto result = stub_->ReadModifyWriteRow(context, options, request);
        operation_context->PostCall(context, result.status());
        return result;
      },
      *current, request, __func__);
  operation_context->OnDone(sor.status());
  if (!sor) return std::move(sor).status();
  return TransformReadModifyWriteRowResponse(*std::move(sor));
}

future<StatusOr<bigtable::Row>> DataConnectionImpl::AsyncReadModifyWriteRow(
    google::bigtable::v2::ReadModifyWriteRowRequest request) {
  auto current = google::cloud::internal::SaveCurrentOptions();
  auto operation_context = operation_context_factory_->ReadModifyWriteRow(
      request.table_name(), app_profile_id(*current));
  auto retry = retry_policy(*current);
  auto backoff = backoff_policy(*current);
  return google::cloud::internal::AsyncRetryLoop(
             std::move(retry), std::move(backoff), Idempotency::kNonIdempotent,
             background_->cq(),
             [stub = stub_, operation_context](
                 CompletionQueue& cq,
                 std::shared_ptr<grpc::ClientContext> context,
                 google::cloud::internal::ImmutableOptions options,
                 google::bigtable::v2::ReadModifyWriteRowRequest const&
                     request) {
               operation_context->PreCall(*context);
               auto f = stub->AsyncReadModifyWriteRow(
                   cq, context, std::move(options), request);
               return f.then(
                   [operation_context, context = std::move(context)](auto f) {
                     auto s = f.get();
                     operation_context->PostCall(*context, s.status());
                     return s;
                   });
             },
             std::move(current), request, __func__)
      .then(
          [operation_context](
              future<StatusOr<google::bigtable::v2::ReadModifyWriteRowResponse>>
                  f) -> StatusOr<bigtable::Row> {
            auto sor = f.get();
            operation_context->OnDone(sor.status());
            if (!sor) return std::move(sor).status();
            return TransformReadModifyWriteRowResponse(*std::move(sor));
          });
}

void DataConnectionImpl::AsyncReadRowsHelper(
    std::string const& table_name,
    std::function<future<bool>(bigtable::Row)> on_row,
    std::function<void(Status)> on_finish, bigtable::RowSet row_set,
    std::int64_t rows_limit, bigtable::Filter filter,
    internal::ImmutableOptions const& current,
    std::shared_ptr<OperationContext> operation_context) {
  auto reverse = internal::CurrentOptions().get<bigtable::ReverseScanOption>();
  bigtable_internal::AsyncRowReader::Create(
      background_->cq(), stub_, app_profile_id(*current), table_name,
      std::move(on_row), std::move(on_finish), std::move(row_set), rows_limit,
      std::move(filter), reverse, retry_policy(*current),
      backoff_policy(*current), enable_server_retries(*current),
      std::move(operation_context));
}

void DataConnectionImpl::AsyncReadRows(
    std::string const& table_name,
    std::function<future<bool>(bigtable::Row)> on_row,
    std::function<void(Status)> on_finish, bigtable::RowSet row_set,
    std::int64_t rows_limit, bigtable::Filter filter) {
  auto current = google::cloud::internal::SaveCurrentOptions();
  auto operation_context = operation_context_factory_->ReadRows(
      table_name, app_profile_id(*current));
  AsyncReadRowsHelper(table_name, std::move(on_row), std::move(on_finish),
                      std::move(row_set), rows_limit, std::move(filter),
                      current, std::move(operation_context));
}

future<StatusOr<std::pair<bool, bigtable::Row>>>
DataConnectionImpl::AsyncReadRow(std::string const& table_name,
                                 std::string row_key, bigtable::Filter filter) {
  class AsyncReadRowHandler {
   public:
    AsyncReadRowHandler() : row_("", {}) {}

    future<StatusOr<std::pair<bool, bigtable::Row>>> GetFuture() {
      return row_promise_.get_future();
    }

    future<bool> OnRow(bigtable::Row row) {
      // assert(!row_received_);
      row_ = std::move(row);
      row_received_ = true;
      // Don't satisfy the promise before `OnStreamFinished`.
      //
      // The `CompletionQueue`, which this object holds a reference to, should
      // not be shut down before `OnStreamFinished` is called. In order to make
      // sure of that, satisying the `promise<>` is deferred until then - the
      // user shouldn't shutdown the `CompletionQueue` before this whole
      // operation is done.
      return make_ready_future(false);
    }

    void OnStreamFinished(Status status) {
      if (row_received_) {
        // If we got a row we don't need to care about the stream status.
        row_promise_.set_value(std::make_pair(true, std::move(row_)));
        return;
      }
      if (status.ok()) {
        row_promise_.set_value(std::make_pair(false, bigtable::Row("", {})));
      } else {
        row_promise_.set_value(std::move(status));
      }
    }

   private:
    bigtable::Row row_;
    bool row_received_{};
    promise<StatusOr<std::pair<bool, bigtable::Row>>> row_promise_;
  };

  auto current = google::cloud::internal::SaveCurrentOptions();
  auto operation_context =
      operation_context_factory_->ReadRow(table_name, app_profile_id(*current));

  bigtable::RowSet row_set(std::move(row_key));
  std::int64_t const rows_limit = 1;
  auto handler = std::make_shared<AsyncReadRowHandler>();
  AsyncReadRowsHelper(
      table_name,
      [handler](bigtable::Row row) { return handler->OnRow(std::move(row)); },
      [handler](Status status) {
        handler->OnStreamFinished(std::move(status));
      },
      std::move(row_set), rows_limit, std::move(filter), current,
      std::move(operation_context));
  return handler->GetFuture();
}

StatusOr<bigtable::PreparedQuery> DataConnectionImpl::PrepareQuery(
    bigtable::PrepareQueryParams const& params) {
  auto current = google::cloud::internal::SaveCurrentOptions();
  google::bigtable::v2::PrepareQueryRequest request;
  auto instance_full_name = params.instance.FullName();
  request.set_instance_name(instance_full_name);
  request.set_app_profile_id(app_profile_id(*current));
  request.set_query(params.sql_statement.sql());
  for (auto const& p : params.sql_statement.params()) {
    (*request.mutable_param_types())[p.first] = p.second.type();
  }
  auto operation_context = operation_context_factory_->PrepareQuery(
      instance_full_name, app_profile_id(*current));
  auto response = google::cloud::internal::RetryLoop(
      retry_policy(*current), backoff_policy(*current),
      Idempotency::kIdempotent,
      [this, operation_context](
          grpc::ClientContext& context, Options const& options,
          google::bigtable::v2::PrepareQueryRequest const& request) {
        operation_context->PreCall(context);
        auto const& result = stub_->PrepareQuery(context, options, request);
        operation_context->PostCall(context, result.status());
        return result;
      },
      *current, request, __func__);
  operation_context->OnDone(response.status());
  if (!response) {
    return std::move(response).status();
  }
  auto const* func = __func__;
  auto refresh_fn = [this, request, current, func]() mutable {
    auto retry = retry_policy(*current);
    auto backoff = backoff_policy(*current);
    return google::cloud::internal::AsyncRetryLoop(
        std::move(retry), std::move(backoff), Idempotency::kIdempotent,
        background_->cq(),
        [this](CompletionQueue& cq,
               std::shared_ptr<grpc::ClientContext> context,
               google::cloud::internal::ImmutableOptions options,
               google::bigtable::v2::PrepareQueryRequest const& request) {
          return stub_->AsyncPrepareQuery(cq, std::move(context),
                                          std::move(options), request);
        },
        std::move(current), request, func);
  };
  auto query_plan = QueryPlan::Create(background_->cq(), *std::move(response),
                                      std::move(refresh_fn));
  return bigtable::PreparedQuery(
      params.instance, std::move(params.sql_statement), std::move(query_plan));
}

future<StatusOr<bigtable::PreparedQuery>> DataConnectionImpl::AsyncPrepareQuery(
    bigtable::PrepareQueryParams const& params) {
  auto current = google::cloud::internal::SaveCurrentOptions();
  google::bigtable::v2::PrepareQueryRequest request;
  auto instance_full_name = params.instance.FullName();
  request.set_instance_name(instance_full_name);
  request.set_app_profile_id(app_profile_id(*current));
  request.set_query(params.sql_statement.sql());
  for (auto const& p : params.sql_statement.params()) {
    (*request.mutable_param_types())[p.first] = p.second.type();
  }
  auto retry = retry_policy(*current);
  auto backoff = backoff_policy(*current);
  auto operation_context = operation_context_factory_->PrepareQuery(
      instance_full_name, app_profile_id(*current));
  auto const* func = __func__;
  return google::cloud::internal::AsyncRetryLoop(
             std::move(retry), std::move(backoff), Idempotency::kIdempotent,
             background_->cq(),
             [this, operation_context](
                 CompletionQueue& cq,
                 std::shared_ptr<grpc::ClientContext> context,
                 google::cloud::internal::ImmutableOptions options,
                 google::bigtable::v2::PrepareQueryRequest const& request) {
               operation_context->PreCall(*context);
               auto f = stub_->AsyncPrepareQuery(cq, context,
                                                 std::move(options), request);
               return f.then(
                   [operation_context, context = std::move(context)](auto f) {
                     auto s = f.get();
                     operation_context->PostCall(*context, s.status());
                     return s;
                   });
             },
             current, request, func)
      .then([this, request, operation_context, current,
             params = std::move(params),
             func](future<StatusOr<google::bigtable::v2::PrepareQueryResponse>>
                       future) -> StatusOr<bigtable::PreparedQuery> {
        auto response = future.get();
        operation_context->OnDone(response.status());
        if (!response) {
          return std::move(response).status();
        }

        auto refresh_fn = [this, request, current, func]() mutable {
          auto retry = retry_policy(*current);
          auto backoff = backoff_policy(*current);
          return google::cloud::internal::AsyncRetryLoop(
              std::move(retry), std::move(backoff), Idempotency::kIdempotent,
              background_->cq(),
              [this](CompletionQueue& cq,
                     std::shared_ptr<grpc::ClientContext> context,
                     google::cloud::internal::ImmutableOptions options,
                     google::bigtable::v2::PrepareQueryRequest const& request) {
                return stub_->AsyncPrepareQuery(cq, std::move(context),
                                                std::move(options), request);
              },
              std::move(current), request, func);
        };

        auto query_plan = QueryPlan::Create(background_->cq(),
                                            *std::move(response), refresh_fn);
        return bigtable::PreparedQuery(params.instance, params.sql_statement,
                                       std::move(query_plan));
      });
}

bigtable::RowStream DataConnectionImpl::ExecuteQuery(
    bigtable::ExecuteQueryParams params) {
  auto current = google::cloud::internal::SaveCurrentOptions();
  google::bigtable::v2::ExecuteQueryRequest request =
      params.bound_query.ToRequestProto();
  request.set_app_profile_id(app_profile_id(*current));

  auto const tracing_enabled = RpcStreamTracingEnabled();
  auto const& tracing_options = RpcTracingOptions();

  auto retry_resume_fn =
      [stub = stub_, tracing_enabled, tracing_options](
          google::bigtable::v2::ExecuteQueryRequest& request,
          google::bigtable::v2::ResultSetMetadata metadata,
          std::unique_ptr<bigtable::DataRetryPolicy> retry_policy_prototype,
          std::unique_ptr<BackoffPolicy> backoff_policy_prototype,
          std::shared_ptr<OperationContext> const& operation_context) mutable
      -> StatusOr<std::unique_ptr<bigtable::ResultSourceInterface>> {
    auto factory =
        [stub, request, tracing_enabled, tracing_options,
         operation_context](std::string const& resume_token) mutable {
          if (!resume_token.empty()) request.set_resume_token(resume_token);
          auto context = std::make_shared<grpc::ClientContext>();
          auto const& options = internal::CurrentOptions();
          internal::ConfigureContext(*context, options);
          auto stream = stub->ExecuteQuery(context, options, request);
          std::unique_ptr<PartialResultSetReader> reader =
              std::make_unique<DefaultPartialResultSetReader>(
                  std::move(context), std::move(stream));
          if (tracing_enabled) {
            reader = std::make_unique<LoggingResultSetReader>(std::move(reader),
                                                              tracing_options);
          }
          return reader;
        };

    auto rpc = std::make_unique<PartialResultSetResume>(
        std::move(factory), Idempotency::kIdempotent,
        retry_policy_prototype->clone(), backoff_policy_prototype->clone());

    return PartialResultSetSource::Create(std::move(metadata),
                                          operation_context, std::move(rpc));
  };

  auto operation_context = std::make_shared<OperationContext>();

  auto query_plan = params.bound_query.query_plan_;
  auto query_plan_retry_policy = query_plan_refresh_retry_policy(*current);
  auto query_plan_backoff_policy = backoff_policy(*current);
  Status last_status;

  // TODO(sdhart): OperationContext needs to be plumbed through the QueryPlan
  // refresh_fn so that it's shared with the ExecuteQuery attempts.
  while (!query_plan_retry_policy->IsExhausted()) {
    // Snapshot query_plan data.
    // This access could cause a query plan refresh to occur.
    StatusOr<google::bigtable::v2::PrepareQueryResponse> query_plan_data =
        query_plan->response();

    if (query_plan_data.ok()) {
      request.set_prepared_query(query_plan_data->prepared_query());
      auto reader = retry_resume_fn(
          request, query_plan_data->metadata(), retry_policy(*current),
          backoff_policy(*current), operation_context);
      if (reader.ok()) {
        return bigtable::RowStream(*std::move(reader));
      }
      if (SafeGrpcRetryAllowingQueryPlanRefresh::IsQueryPlanExpired(
              reader.status())) {
        query_plan->Invalidate(reader.status(),
                               query_plan_data->prepared_query());
      }
      last_status = reader.status();
    } else {
      last_status = query_plan_data.status();
    }

    auto delay =
        internal::Backoff(last_status, __func__, *query_plan_retry_policy,
                          *query_plan_backoff_policy, Idempotency::kIdempotent,
                          false /* enable_server_retries */);
    if (!delay) break;
    std::this_thread::sleep_for(*delay);
  }
  return bigtable::RowStream(
      std::make_unique<StatusOnlyResultSetSource>(internal::RetryLoopError(
          last_status, __func__, query_plan_retry_policy->IsExhausted())));
}
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable_internal
}  // namespace cloud
}  // namespace google
