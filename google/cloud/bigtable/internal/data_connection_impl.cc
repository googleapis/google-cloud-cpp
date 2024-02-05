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
#include "google/cloud/bigtable/internal/retry_context.h"
#include "google/cloud/bigtable/internal/retry_info_helper.h"
#include "google/cloud/bigtable/options.h"
#include "google/cloud/background_threads.h"
#include "google/cloud/grpc_options.h"
#include "google/cloud/idempotency.h"
#include "google/cloud/internal/async_retry_loop.h"
#include "google/cloud/internal/retry_loop.h"
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

inline std::unique_ptr<BackoffPolicy> backoff_policy(Options const& options) {
  return options.get<bigtable::DataBackoffPolicyOption>()->clone();
}

inline std::unique_ptr<bigtable::IdempotentMutationPolicy> idempotency_policy(
    Options const& options) {
  return options.get<bigtable::IdempotentMutationPolicyOption>()->clone();
}

inline bool enable_server_retries(Options const& options) {
  return options.get<internal::EnableServerRetriesOption>();
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

  RetryContext retry_context;
  auto sor = google::cloud::internal::RetryLoop(
      retry_policy(*current), backoff_policy(*current),
      is_idempotent ? Idempotency::kIdempotent : Idempotency::kNonIdempotent,
      [this, &retry_context](
          grpc::ClientContext& context,
          google::bigtable::v2::MutateRowRequest const& request) {
        retry_context.PreCall(context);
        auto s = stub_->MutateRow(context, request);
        retry_context.PostCall(context);
        return s;
      },
      request, __func__);
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

  return google::cloud::internal::AsyncRetryLoop(
             retry_policy(*current), backoff_policy(*current),
             is_idempotent ? Idempotency::kIdempotent
                           : Idempotency::kNonIdempotent,
             background_->cq(),
             [stub = stub_, retry_context = std::make_shared<RetryContext>()](
                 CompletionQueue& cq,
                 std::shared_ptr<grpc::ClientContext> context,
                 google::bigtable::v2::MutateRowRequest const& request) {
               retry_context->PreCall(*context);
               auto f = stub->AsyncMutateRow(cq, context, request);
               return f.then(
                   [retry_context, context = std::move(context)](auto f) {
                     auto s = f.get();
                     retry_context->PostCall(*context);
                     return s;
                   });
             },
             request, __func__)
      .then([](future<StatusOr<google::bigtable::v2::MutateRowResponse>> f) {
        auto sor = f.get();
        if (!sor) return std::move(sor).status();
        return Status{};
      });
}

std::vector<bigtable::FailedMutation> DataConnectionImpl::BulkApply(
    std::string const& table_name, bigtable::BulkMutation mut) {
  auto current = google::cloud::internal::SaveCurrentOptions();
  if (mut.empty()) return {};
  BulkMutator mutator(app_profile_id(*current), table_name,
                      *idempotency_policy(*current), std::move(mut));
  // We wait to allocate the policies until they are needed as a
  // micro-optimization.
  std::unique_ptr<bigtable::DataRetryPolicy> retry;
  std::unique_ptr<BackoffPolicy> backoff;
  while (true) {
    auto status = mutator.MakeOneRequest(*stub_, *limiter_);
    if (!mutator.HasPendingMutations()) break;
    if (!retry) retry = retry_policy(*current);
    if (!retry->OnFailure(status)) break;
    if (!backoff) backoff = backoff_policy(*current);
    auto delay = backoff->OnCompletion();
    std::this_thread::sleep_for(delay);
  }
  return std::move(mutator).OnRetryDone();
}

future<std::vector<bigtable::FailedMutation>>
DataConnectionImpl::AsyncBulkApply(std::string const& table_name,
                                   bigtable::BulkMutation mut) {
  auto current = google::cloud::internal::SaveCurrentOptions();
  return AsyncBulkApplier::Create(
      background_->cq(), stub_, limiter_, retry_policy(*current),
      backoff_policy(*current), *idempotency_policy(*current),
      app_profile_id(*current), table_name, std::move(mut));
}

bigtable::RowReader DataConnectionImpl::ReadRowsFull(
    bigtable::ReadRowsParams params) {
  auto current = google::cloud::internal::SaveCurrentOptions();
  auto impl = std::make_shared<DefaultRowReader>(
      stub_, std::move(params.app_profile_id), std::move(params.table_name),
      std::move(params.row_set), params.rows_limit, std::move(params.filter),
      params.reverse, retry_policy(*current), backoff_policy(*current),
      // TODO(#13514) - use Option value.
      false);
  return MakeRowReader(std::move(impl));
}

StatusOr<std::pair<bool, bigtable::Row>> DataConnectionImpl::ReadRow(
    std::string const& table_name, std::string row_key,
    bigtable::Filter filter) {
  auto current = google::cloud::internal::SaveCurrentOptions();
  bigtable::RowSet row_set(std::move(row_key));
  std::int64_t const rows_limit = 1;
  auto reader = ReadRowsFull(bigtable::ReadRowsParams{
      table_name, app_profile_id(*current), std::move(row_set), rows_limit,
      std::move(filter)});

  auto it = reader.begin();
  if (it == reader.end()) return std::make_pair(false, bigtable::Row("", {}));
  if (!*it) return it->status();
  auto result = std::make_pair(true, std::move(**it));
  if (++it != reader.end()) {
    return Status(
        StatusCode::kInternal,
        "internal error - RowReader returned more than one row in ReadRow()");
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
  RetryContext retry_context;
  auto sor = google::cloud::internal::RetryLoop(
      retry_policy(*current), backoff_policy(*current), idempotency,
      [this, &retry_context](
          grpc::ClientContext& context,
          google::bigtable::v2::CheckAndMutateRowRequest const& request) {
        retry_context.PreCall(context);
        auto s = stub_->CheckAndMutateRow(context, request);
        retry_context.PostCall(context);
        return s;
      },
      request, __func__);
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

  return google::cloud::internal::AsyncRetryLoop(
             retry_policy(*current), backoff_policy(*current), idempotency,
             background_->cq(),
             [stub = stub_, retry_context = std::make_shared<RetryContext>()](
                 CompletionQueue& cq,
                 std::shared_ptr<grpc::ClientContext> context,
                 google::bigtable::v2::CheckAndMutateRowRequest const&
                     request) {
               retry_context->PreCall(*context);
               auto f = stub->AsyncCheckAndMutateRow(cq, context, request);
               return f.then(
                   [retry_context, context = std::move(context)](auto f) {
                     auto s = f.get();
                     retry_context->PostCall(*context);
                     return s;
                   });
             },
             request, __func__)
      .then([](future<StatusOr<google::bigtable::v2::CheckAndMutateRowResponse>>
                   f) -> StatusOr<bigtable::MutationBranch> {
        auto sor = f.get();
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

  Status status;
  std::vector<bigtable::RowKeySample> samples;
  std::unique_ptr<bigtable::DataRetryPolicy> retry;
  std::unique_ptr<BackoffPolicy> backoff;
  RetryContext retry_context;
  while (true) {
    auto context = std::make_shared<grpc::ClientContext>();
    internal::ConfigureContext(*context, internal::CurrentOptions());
    retry_context.PreCall(*context);
    auto stream = stub_->SampleRowKeys(context, Options{}, request);

    struct UnpackVariant {
      Status& status;
      std::vector<bigtable::RowKeySample>& samples;
      bool operator()(Status s) {
        status = std::move(s);
        return false;
      }
      bool operator()(google::bigtable::v2::SampleRowKeysResponse r) {
        bigtable::RowKeySample row_sample;
        row_sample.offset_bytes = r.offset_bytes();
        row_sample.row_key = std::move(*r.mutable_row_key());
        samples.emplace_back(std::move(row_sample));
        return true;
      }
    };
    while (absl::visit(UnpackVariant{status, samples}, stream->Read())) {
    }
    if (status.ok()) break;
    // We wait to allocate the policies until they are needed as a
    // micro-optimization.
    if (!retry) retry = retry_policy(*current);
    if (!backoff) backoff = backoff_policy(*current);
    auto delay = BackoffOrBreak(enable_server_retries(*current), status, *retry,
                                *backoff);
    if (!delay) {
      return Status(status.code(),
                    "Retry policy exhausted: " + status.message());
    }
    retry_context.PostCall(*context);
    // A new stream invalidates previously returned samples.
    samples.clear();
    std::this_thread::sleep_for(*delay);
  }
  return samples;
}

future<StatusOr<std::vector<bigtable::RowKeySample>>>
DataConnectionImpl::AsyncSampleRows(std::string const& table_name) {
  auto current = google::cloud::internal::SaveCurrentOptions();
  return AsyncRowSampler::Create(
      background_->cq(), stub_, retry_policy(*current),
      backoff_policy(*current), app_profile_id(*current), table_name);
}

StatusOr<bigtable::Row> DataConnectionImpl::ReadModifyWriteRow(
    google::bigtable::v2::ReadModifyWriteRowRequest request) {
  auto current = google::cloud::internal::SaveCurrentOptions();
  auto sor = google::cloud::internal::RetryLoop(
      retry_policy(*current), backoff_policy(*current),
      Idempotency::kNonIdempotent,
      [this](grpc::ClientContext& context,
             google::bigtable::v2::ReadModifyWriteRowRequest const& request) {
        return stub_->ReadModifyWriteRow(context, request);
      },
      request, __func__);
  if (!sor) return std::move(sor).status();
  return TransformReadModifyWriteRowResponse(*std::move(sor));
}

future<StatusOr<bigtable::Row>> DataConnectionImpl::AsyncReadModifyWriteRow(
    google::bigtable::v2::ReadModifyWriteRowRequest request) {
  auto current = google::cloud::internal::SaveCurrentOptions();
  return google::cloud::internal::AsyncRetryLoop(
             retry_policy(*current), backoff_policy(*current),
             Idempotency::kNonIdempotent, background_->cq(),
             [stub = stub_](
                 CompletionQueue& cq,
                 std::shared_ptr<grpc::ClientContext> context,
                 google::bigtable::v2::ReadModifyWriteRowRequest const&
                     request) {
               return stub->AsyncReadModifyWriteRow(cq, std::move(context),
                                                    request);
             },
             request, __func__)
      .then(
          [](future<StatusOr<google::bigtable::v2::ReadModifyWriteRowResponse>>
                 f) -> StatusOr<bigtable::Row> {
            auto sor = f.get();
            if (!sor) return std::move(sor).status();
            return TransformReadModifyWriteRowResponse(*std::move(sor));
          });
}

void DataConnectionImpl::AsyncReadRows(
    std::string const& table_name,
    std::function<future<bool>(bigtable::Row)> on_row,
    std::function<void(Status)> on_finish, bigtable::RowSet row_set,
    std::int64_t rows_limit, bigtable::Filter filter) {
  auto current = google::cloud::internal::SaveCurrentOptions();
  auto reverse = internal::CurrentOptions().get<bigtable::ReverseScanOption>();
  bigtable_internal::AsyncRowReader::Create(
      background_->cq(), stub_, app_profile_id(*current), table_name,
      std::move(on_row), std::move(on_finish), std::move(row_set), rows_limit,
      std::move(filter), reverse, retry_policy(*current),
      backoff_policy(*current));
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

  bigtable::RowSet row_set(std::move(row_key));
  std::int64_t const rows_limit = 1;
  auto handler = std::make_shared<AsyncReadRowHandler>();
  AsyncReadRows(
      table_name,
      [handler](bigtable::Row row) { return handler->OnRow(std::move(row)); },
      [handler](Status status) {
        handler->OnStreamFinished(std::move(status));
      },
      std::move(row_set), rows_limit, std::move(filter));
  return handler->GetFuture();
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable_internal
}  // namespace cloud
}  // namespace google
