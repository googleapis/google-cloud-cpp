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
#include "google/cloud/background_threads.h"
#include "google/cloud/idempotency.h"
#include "google/cloud/internal/async_retry_loop.h"
#include "google/cloud/internal/retry_loop.h"
#include <memory>

namespace google {
namespace cloud {
namespace bigtable_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

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
    std::shared_ptr<BigtableStub> stub, Options options)
    : background_(std::move(background)),
      stub_(std::move(stub)),
      options_(internal::MergeOptions(
          std::move(options),
          bigtable::internal::DefaultDataOptions(DataConnection::options()))) {}

Status DataConnectionImpl::Apply(std::string const& app_profile_id,
                                 std::string const& table_name,
                                 bigtable::SingleRowMutation mut) {
  google::bigtable::v2::MutateRowRequest request;
  request.set_app_profile_id(app_profile_id);
  request.set_table_name(table_name);
  mut.MoveTo(request);

  auto idempotent_policy = idempotency_policy();
  bool const is_idempotent = std::all_of(
      request.mutations().begin(), request.mutations().end(),
      [&idempotent_policy](google::bigtable::v2::Mutation const& m) {
        return idempotent_policy->is_idempotent(m);
      });

  auto sor = google::cloud::internal::RetryLoop(
      retry_policy(), backoff_policy(),
      is_idempotent ? Idempotency::kIdempotent : Idempotency::kNonIdempotent,
      [this](grpc::ClientContext& context,
             google::bigtable::v2::MutateRowRequest const& request) {
        return stub_->MutateRow(context, request);
      },
      request, __func__);
  if (!sor) return std::move(sor).status();
  return Status{};
}

future<Status> DataConnectionImpl::AsyncApply(std::string const& app_profile_id,
                                              std::string const& table_name,
                                              bigtable::SingleRowMutation mut) {
  google::bigtable::v2::MutateRowRequest request;
  request.set_app_profile_id(app_profile_id);
  request.set_table_name(table_name);
  mut.MoveTo(request);

  auto idempotent_policy = idempotency_policy();
  bool const is_idempotent = std::all_of(
      request.mutations().begin(), request.mutations().end(),
      [&idempotent_policy](google::bigtable::v2::Mutation const& m) {
        return idempotent_policy->is_idempotent(m);
      });

  auto& stub = stub_;
  return google::cloud::internal::AsyncRetryLoop(
             retry_policy(), backoff_policy(),
             is_idempotent ? Idempotency::kIdempotent
                           : Idempotency::kNonIdempotent,
             background_->cq(),
             [stub](CompletionQueue& cq,
                    std::unique_ptr<grpc::ClientContext> context,
                    google::bigtable::v2::MutateRowRequest const& request) {
               return stub->AsyncMutateRow(cq, std::move(context), request);
             },
             request, __func__)
      .then([](future<StatusOr<google::bigtable::v2::MutateRowResponse>> f) {
        auto sor = f.get();
        if (!sor) return std::move(sor).status();
        return Status{};
      });
}

std::vector<bigtable::FailedMutation> DataConnectionImpl::BulkApply(
    std::string const& app_profile_id, std::string const& table_name,
    bigtable::BulkMutation mut) {
  if (mut.empty()) return {};
  bigtable::internal::BulkMutator mutator(
      app_profile_id, table_name, *idempotency_policy(), std::move(mut));
  // We wait to allocate the policies until they are needed as a
  // micro-optimization.
  std::unique_ptr<DataRetryPolicy> retry;
  std::unique_ptr<BackoffPolicy> backoff;
  do {
    auto status = mutator.MakeOneRequest(*stub_);
    if (status.ok()) continue;
    if (!retry) retry = retry_policy();
    if (!retry->OnFailure(status)) break;
    if (!backoff) backoff = backoff_policy();
    auto delay = backoff->OnCompletion();
    std::this_thread::sleep_for(delay);
  } while (mutator.HasPendingMutations());
  return std::move(mutator).OnRetryDone();
}

future<std::vector<bigtable::FailedMutation>>
DataConnectionImpl::AsyncBulkApply(std::string const& app_profile_id,
                                   std::string const& table_name,
                                   bigtable::BulkMutation mut) {
  return AsyncBulkApplier::Create(background_->cq(), stub_, retry_policy(),
                                  backoff_policy(), *idempotency_policy(),
                                  app_profile_id, table_name, std::move(mut));
}

bigtable::RowReader DataConnectionImpl::ReadRows(
    std::string const& app_profile_id, std::string const& table_name,
    bigtable::RowSet row_set, std::int64_t rows_limit,
    bigtable::Filter filter) {
  auto impl = std::make_shared<DefaultRowReader>(
      stub_, app_profile_id, table_name, std::move(row_set), rows_limit,
      std::move(filter), retry_policy(), backoff_policy());
  return MakeRowReader(std::move(impl));
}

StatusOr<std::pair<bool, bigtable::Row>> DataConnectionImpl::ReadRow(
    std::string const& app_profile_id, std::string const& table_name,
    std::string row_key, bigtable::Filter filter) {
  bigtable::RowSet row_set(std::move(row_key));
  std::int64_t const rows_limit = 1;
  auto reader = ReadRows(app_profile_id, table_name, std::move(row_set),
                         rows_limit, std::move(filter));

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
    std::string const& app_profile_id, std::string const& table_name,
    std::string row_key, bigtable::Filter filter,
    std::vector<bigtable::Mutation> true_mutations,
    std::vector<bigtable::Mutation> false_mutations) {
  google::bigtable::v2::CheckAndMutateRowRequest request;
  request.set_app_profile_id(app_profile_id);
  request.set_table_name(table_name);
  request.set_row_key(std::move(row_key));
  *request.mutable_predicate_filter() = std::move(filter).as_proto();
  for (auto& m : true_mutations) {
    *request.add_true_mutations() = std::move(m.op);
  }
  for (auto& m : false_mutations) {
    *request.add_false_mutations() = std::move(m.op);
  }
  auto const idempotency = idempotency_policy()->is_idempotent(request)
                               ? Idempotency::kIdempotent
                               : Idempotency::kNonIdempotent;
  auto sor = google::cloud::internal::RetryLoop(
      retry_policy(), backoff_policy(), idempotency,
      [this](grpc::ClientContext& context,
             google::bigtable::v2::CheckAndMutateRowRequest const& request) {
        return stub_->CheckAndMutateRow(context, request);
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
    std::string const& app_profile_id, std::string const& table_name,
    std::string row_key, bigtable::Filter filter,
    std::vector<bigtable::Mutation> true_mutations,
    std::vector<bigtable::Mutation> false_mutations) {
  google::bigtable::v2::CheckAndMutateRowRequest request;
  request.set_app_profile_id(app_profile_id);
  request.set_table_name(table_name);
  request.set_row_key(std::move(row_key));
  *request.mutable_predicate_filter() = std::move(filter).as_proto();
  for (auto& m : true_mutations) {
    *request.add_true_mutations() = std::move(m.op);
  }
  for (auto& m : false_mutations) {
    *request.add_false_mutations() = std::move(m.op);
  }
  auto const idempotency = idempotency_policy()->is_idempotent(request)
                               ? Idempotency::kIdempotent
                               : Idempotency::kNonIdempotent;

  auto& stub = stub_;
  return google::cloud::internal::AsyncRetryLoop(
             retry_policy(), backoff_policy(), idempotency, background_->cq(),
             [stub](CompletionQueue& cq,
                    std::unique_ptr<grpc::ClientContext> context,
                    google::bigtable::v2::CheckAndMutateRowRequest const&
                        request) {
               return stub->AsyncCheckAndMutateRow(cq, std::move(context),
                                                   request);
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
    std::string const& app_profile_id, std::string const& table_name) {
  google::bigtable::v2::SampleRowKeysRequest request;
  request.set_app_profile_id(app_profile_id);
  request.set_table_name(table_name);

  Status status;
  std::vector<bigtable::RowKeySample> samples;
  std::unique_ptr<DataRetryPolicy> retry;
  std::unique_ptr<BackoffPolicy> backoff;
  while (true) {
    auto context = absl::make_unique<grpc::ClientContext>();
    internal::ConfigureContext(*context, internal::CurrentOptions());
    auto stream = stub_->SampleRowKeys(std::move(context), request);

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
    if (!retry) retry = retry_policy();
    if (!retry->OnFailure(status)) {
      return Status(status.code(),
                    "Retry policy exhausted: " + status.message());
    }
    // A new stream invalidates previously returned samples.
    samples.clear();
    if (!backoff) backoff = backoff_policy();
    auto delay = backoff->OnCompletion();
    std::this_thread::sleep_for(delay);
  }
  return samples;
}

future<StatusOr<std::vector<bigtable::RowKeySample>>>
DataConnectionImpl::AsyncSampleRows(std::string const& app_profile_id,
                                    std::string const& table_name) {
  return AsyncRowSampler::Create(background_->cq(), stub_, retry_policy(),
                                 backoff_policy(), app_profile_id, table_name);
}

StatusOr<bigtable::Row> DataConnectionImpl::ReadModifyWriteRow(
    google::bigtable::v2::ReadModifyWriteRowRequest request) {
  auto sor = google::cloud::internal::RetryLoop(
      retry_policy(), backoff_policy(), Idempotency::kNonIdempotent,
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
  auto& stub = stub_;
  return google::cloud::internal::AsyncRetryLoop(
             retry_policy(), backoff_policy(), Idempotency::kNonIdempotent,
             background_->cq(),
             [stub](CompletionQueue& cq,
                    std::unique_ptr<grpc::ClientContext> context,
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

void DataConnectionImpl::AsyncReadRows(std::string const& app_profile_id,
                                       std::string const& table_name,
                                       bigtable::RowFunctor on_row,
                                       bigtable::FinishFunctor on_finish,
                                       bigtable::RowSet row_set,
                                       std::int64_t rows_limit,
                                       bigtable::Filter filter) {
  bigtable_internal::AsyncRowReader::Create(
      background_->cq(), stub_, app_profile_id, table_name, std::move(on_row),
      std::move(on_finish), std::move(row_set), rows_limit, std::move(filter),
      retry_policy(), backoff_policy());
}

future<StatusOr<std::pair<bool, bigtable::Row>>>
DataConnectionImpl::AsyncReadRow(std::string const& app_profile_id,
                                 std::string const& table_name,
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
      app_profile_id, table_name,
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
