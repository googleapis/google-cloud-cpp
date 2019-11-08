// Copyright 2017 Google Inc.
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

#include "google/cloud/bigtable/table.h"
#include "google/cloud/bigtable/internal/async_bulk_apply.h"
#include "google/cloud/bigtable/internal/async_retry_unary_rpc.h"
#include "google/cloud/bigtable/internal/bulk_mutator.h"
#include "google/cloud/bigtable/internal/unary_client_utils.h"
#include "google/cloud/grpc_utils/grpc_error_delegate.h"
#include <thread>
#include <type_traits>

namespace btproto = ::google::bigtable::v2;
namespace google {
namespace cloud {
namespace bigtable {
inline namespace BIGTABLE_CLIENT_NS {
namespace {
template <typename Request>
void SetCommonTableOperationRequest(Request& request,
                                    std::string const& app_profile_id,
                                    std::string const& table_name) {
  request.set_app_profile_id(app_profile_id);
  request.set_table_name(table_name);
}

template <typename Response>
Row TransformReadModifyWriteRowResponse(Response& response) {
  std::vector<bigtable::Cell> cells;
  auto& row = *response.mutable_row();
  for (auto& family : *row.mutable_families()) {
    for (auto& column : *family.mutable_columns()) {
      for (auto& cell : *column.mutable_cells()) {
        std::vector<std::string> labels;
        std::move(cell.mutable_labels()->begin(), cell.mutable_labels()->end(),
                  std::back_inserter(labels));
        bigtable::Cell new_cell(row.key(), family.name(), column.qualifier(),
                                cell.timestamp_micros(),
                                std::move(*cell.mutable_value()),
                                std::move(labels));

        cells.emplace_back(std::move(new_cell));
      }
    }
  }

  return Row(std::move(*row.mutable_key()), std::move(cells));
}

}  // namespace

using ClientUtils = bigtable::internal::UnaryClientUtils<DataClient>;

static_assert(std::is_copy_assignable<bigtable::Table>::value,
              "bigtable::Table must be CopyAssignable");

Status Table::Apply(SingleRowMutation mut) {
  // Copy the policies in effect for this operation.  Many policy classes change
  // their state as the operation makes progress (or fails to make progress), so
  // we need fresh instances.
  auto rpc_policy = clone_rpc_retry_policy();
  auto backoff_policy = clone_rpc_backoff_policy();
  auto idempotent_policy = clone_idempotent_mutation_policy();

  // Build the RPC request, try to minimize copying.
  btproto::MutateRowRequest request;
  SetCommonTableOperationRequest<btproto::MutateRowRequest>(
      request, app_profile_id_, table_name_);
  mut.MoveTo(request);

  bool const is_idempotent =
      std::all_of(request.mutations().begin(), request.mutations().end(),
                  [&idempotent_policy](btproto::Mutation const& m) {
                    return idempotent_policy->is_idempotent(m);
                  });

  btproto::MutateRowResponse response;
  grpc::Status status;
  while (true) {
    grpc::ClientContext client_context;
    rpc_policy->Setup(client_context);
    backoff_policy->Setup(client_context);
    metadata_update_policy_.Setup(client_context);
    status = client_->MutateRow(&client_context, request, &response);

    if (status.ok()) {
      return google::cloud::Status{};
    }
    // It is up to the policy to terminate this loop, it could run
    // forever, but that would be a bad policy (pun intended).
    if (!rpc_policy->OnFailure(status) || !is_idempotent) {
      return grpc_utils::MakeStatusFromRpcError(status);
    }
    auto delay = backoff_policy->OnCompletion(status);
    std::this_thread::sleep_for(delay);
  }
}

future<Status> Table::AsyncApply(SingleRowMutation mut, CompletionQueue& cq) {
  google::bigtable::v2::MutateRowRequest request;
  SetCommonTableOperationRequest<google::bigtable::v2::MutateRowRequest>(
      request, app_profile_id_, table_name_);
  mut.MoveTo(request);
  auto context = google::cloud::internal::make_unique<grpc::ClientContext>();

  // Determine if all the mutations are idempotent. The idempotency of the
  // mutations won't change as the retry loop executes, so we can just compute
  // it once and use a constant value for the loop.
  auto idempotent_mutation_policy = clone_idempotent_mutation_policy();
  bool const is_idempotent = std::all_of(
      request.mutations().begin(), request.mutations().end(),
      [&idempotent_mutation_policy](google::bigtable::v2::Mutation const& m) {
        return idempotent_mutation_policy->is_idempotent(m);
      });

  auto client = client_;
  return internal::StartRetryAsyncUnaryRpc(
             __func__, clone_rpc_retry_policy(), clone_rpc_backoff_policy(),
             internal::ConstantIdempotencyPolicy(is_idempotent),
             clone_metadata_update_policy(),
             [client](grpc::ClientContext* context,
                      google::bigtable::v2::MutateRowRequest const& request,
                      grpc::CompletionQueue* cq) {
               return client->AsyncMutateRow(context, request, cq);
             },
             std::move(request), cq)
      .then([](future<StatusOr<google::bigtable::v2::MutateRowResponse>> r) {
        return r.get().status();
      });
}

std::vector<FailedMutation> Table::BulkApply(BulkMutation mut) {
  grpc::Status status;

  // Copy the policies in effect for this operation.  Many policy classes change
  // their state as the operation makes progress (or fails to make progress), so
  // we need fresh instances.
  auto backoff_policy = clone_rpc_backoff_policy();
  auto retry_policy = clone_rpc_retry_policy();
  auto idemponent_policy = clone_idempotent_mutation_policy();

  bigtable::internal::BulkMutator mutator(app_profile_id_, table_name_,
                                          *idemponent_policy, std::move(mut));
  while (mutator.HasPendingMutations()) {
    grpc::ClientContext client_context;
    backoff_policy->Setup(client_context);
    retry_policy->Setup(client_context);
    metadata_update_policy_.Setup(client_context);
    status = mutator.MakeOneRequest(*client_, client_context);
    if (!status.ok() && !retry_policy->OnFailure(status)) {
      break;
    }
    auto delay = backoff_policy->OnCompletion(status);
    std::this_thread::sleep_for(delay);
  }
  return std::move(mutator).OnRetryDone();
}

future<std::vector<FailedMutation>> Table::AsyncBulkApply(BulkMutation mut,
                                                          CompletionQueue& cq) {
  auto mutation_policy = clone_idempotent_mutation_policy();
  return internal::AsyncRetryBulkApply::Create(
      cq, clone_rpc_retry_policy(), clone_rpc_backoff_policy(),
      *mutation_policy, clone_metadata_update_policy(), client_,
      app_profile_id_, table_name(), std::move(mut));
}

RowReader Table::ReadRows(RowSet row_set, Filter filter) {
  return RowReader(client_, app_profile_id_, table_name_, std::move(row_set),
                   RowReader::NO_ROWS_LIMIT, std::move(filter),
                   clone_rpc_retry_policy(), clone_rpc_backoff_policy(),
                   metadata_update_policy_,
                   google::cloud::internal::make_unique<
                       bigtable::internal::ReadRowsParserFactory>());
}

RowReader Table::ReadRows(RowSet row_set, std::int64_t rows_limit,
                          Filter filter) {
  return RowReader(client_, app_profile_id_, table_name_, std::move(row_set),
                   rows_limit, std::move(filter), clone_rpc_retry_policy(),
                   clone_rpc_backoff_policy(), metadata_update_policy_,
                   google::cloud::internal::make_unique<
                       bigtable::internal::ReadRowsParserFactory>());
}

StatusOr<std::pair<bool, Row>> Table::ReadRow(std::string row_key,
                                              Filter filter) {
  RowSet row_set(std::move(row_key));
  std::int64_t const rows_limit = 1;
  RowReader reader =
      ReadRows(std::move(row_set), rows_limit, std::move(filter));

  auto it = reader.begin();
  if (it == reader.end()) {
    return std::make_pair(false, Row("", {}));
  }
  if (!*it) {
    return it->status();
  }
  auto result = std::make_pair(true, std::move(**it));
  if (++it != reader.end()) {
    return Status(StatusCode::kInternal,
                  "internal error - RowReader returned 2 rows in ReadRow()");
  }
  return result;
}

StatusOr<MutationBranch> Table::CheckAndMutateRow(
    std::string row_key, Filter filter, std::vector<Mutation> true_mutations,
    std::vector<Mutation> false_mutations) {
  grpc::Status status;
  btproto::CheckAndMutateRowRequest request;
  request.set_row_key(std::move(row_key));
  SetCommonTableOperationRequest<btproto::CheckAndMutateRowRequest>(
      request, app_profile_id_, table_name_);
  *request.mutable_predicate_filter() = std::move(filter).as_proto();
  for (auto& m : true_mutations) {
    *request.add_true_mutations() = std::move(m.op);
  }
  for (auto& m : false_mutations) {
    *request.add_false_mutations() = std::move(m.op);
  }
  bool const is_idempotent =
      idempotent_mutation_policy_->is_idempotent(request);
  auto response = ClientUtils::MakeCall(
      *client_, clone_rpc_retry_policy(), clone_rpc_backoff_policy(),
      metadata_update_policy_, &DataClient::CheckAndMutateRow, request,
      "Table::CheckAndMutateRow", status, is_idempotent);

  if (!status.ok()) {
    return grpc_utils::MakeStatusFromRpcError(status);
  }
  return response.predicate_matched() ? MutationBranch::kPredicateMatched
                                      : MutationBranch::kPredicateNotMatched;
}

future<StatusOr<MutationBranch>> Table::AsyncCheckAndMutateRow(
    std::string row_key, Filter filter, std::vector<Mutation> true_mutations,
    std::vector<Mutation> false_mutations, CompletionQueue& cq) {
  btproto::CheckAndMutateRowRequest request;
  request.set_row_key(std::move(row_key));
  SetCommonTableOperationRequest<btproto::CheckAndMutateRowRequest>(
      request, app_profile_id_, table_name_);
  *request.mutable_predicate_filter() = std::move(filter).as_proto();
  for (auto& m : true_mutations) {
    *request.add_true_mutations() = std::move(m.op);
  }
  for (auto& m : false_mutations) {
    *request.add_false_mutations() = std::move(m.op);
  }
  bool const is_idempotent =
      idempotent_mutation_policy_->is_idempotent(request);

  auto client = client_;
  return internal::StartRetryAsyncUnaryRpc(
             __func__, clone_rpc_retry_policy(), clone_rpc_backoff_policy(),
             internal::ConstantIdempotencyPolicy(is_idempotent),
             clone_metadata_update_policy(),
             [client](grpc::ClientContext* context,
                      btproto::CheckAndMutateRowRequest const& request,
                      grpc::CompletionQueue* cq) {
               return client->AsyncCheckAndMutateRow(context, request, cq);
             },
             std::move(request), cq)
      .then([](future<StatusOr<btproto::CheckAndMutateRowResponse>> f)
                -> StatusOr<MutationBranch> {
        auto response = f.get();
        if (!response) {
          return response.status();
        }
        return response->predicate_matched()
                   ? MutationBranch::kPredicateMatched
                   : MutationBranch::kPredicateNotMatched;
      });
}

// Call the `google.bigtable.v2.Bigtable.SampleRowKeys` RPC until
// successful. When RPC is finished, this function returns the SampleRowKeys
// as a std::vector<>. If the RPC fails, it will keep retrying until the
// policies in effect tell us to stop. Note that each retry must clear the
// samples otherwise the result is an inconsistent set of samples row keys.
StatusOr<std::vector<bigtable::RowKeySample>> Table::SampleRows() {
  // Copy the policies in effect for this operation.
  auto backoff_policy = clone_rpc_backoff_policy();
  auto retry_policy = clone_rpc_retry_policy();
  std::vector<bigtable::RowKeySample> samples;

  // Build the RPC request for SampleRowKeys
  btproto::SampleRowKeysRequest request;
  btproto::SampleRowKeysResponse response;
  SetCommonTableOperationRequest<btproto::SampleRowKeysRequest>(
      request, app_profile_id_, table_name_);

  while (true) {
    grpc::ClientContext client_context;
    backoff_policy->Setup(client_context);
    retry_policy->Setup(client_context);
    clone_metadata_update_policy().Setup(client_context);

    auto stream = client_->SampleRowKeys(&client_context, request);
    while (stream->Read(&response)) {
      bigtable::RowKeySample row_sample;
      row_sample.offset_bytes = response.offset_bytes();
      row_sample.row_key = std::move(*response.mutable_row_key());
      samples.emplace_back(std::move(row_sample));
    }
    auto status = stream->Finish();
    if (status.ok()) {
      break;
    }
    if (!retry_policy->OnFailure(status)) {
      return grpc_utils::MakeStatusFromRpcError(
          status.error_code(),
          "Retry policy exhausted: " + status.error_message());
    }
    samples.clear();
    auto delay = backoff_policy->OnCompletion(status);
    std::this_thread::sleep_for(delay);
  }
  return samples;
}

StatusOr<Row> Table::ReadModifyWriteRowImpl(
    btproto::ReadModifyWriteRowRequest request) {
  SetCommonTableOperationRequest<
      ::google::bigtable::v2::ReadModifyWriteRowRequest>(
      request, app_profile_id_, table_name_);

  grpc::Status status;
  auto response = ClientUtils::MakeNonIdemponentCall(
      *(client_), clone_rpc_retry_policy(), clone_metadata_update_policy(),
      &DataClient::ReadModifyWriteRow, request, "ReadModifyWriteRowRequest",
      status);
  if (!status.ok()) {
    return grpc_utils::MakeStatusFromRpcError(status);
  }
  return TransformReadModifyWriteRowResponse<
      btproto::ReadModifyWriteRowResponse>(response);
}

future<StatusOr<Row>> Table::AsyncReadModifyWriteRowImpl(
    CompletionQueue& cq,
    ::google::bigtable::v2::ReadModifyWriteRowRequest request) {
  SetCommonTableOperationRequest<
      ::google::bigtable::v2::ReadModifyWriteRowRequest>(
      request, app_profile_id_, table_name_);

  auto client = client_;
  return internal::StartRetryAsyncUnaryRpc(
             __func__, clone_rpc_retry_policy(), clone_rpc_backoff_policy(),
             internal::ConstantIdempotencyPolicy(false),
             clone_metadata_update_policy(),
             [client](grpc::ClientContext* context,
                      btproto::ReadModifyWriteRowRequest const& request,
                      grpc::CompletionQueue* cq) {
               return client->AsyncReadModifyWriteRow(context, request, cq);
             },
             std::move(request), cq)
      .then([](future<StatusOr<btproto::ReadModifyWriteRowResponse>> fut)
                -> StatusOr<Row> {
        auto result = fut.get();
        if (!result) {
          return result.status();
        }
        return TransformReadModifyWriteRowResponse<
            btproto::ReadModifyWriteRowResponse>(*result);
      });
}

future<StatusOr<std::pair<bool, Row>>> Table::AsyncReadRow(CompletionQueue& cq,
                                                           std::string row_key,
                                                           Filter filter) {
  class AsyncReadRowHandler {
   public:
    AsyncReadRowHandler() : row_("", {}) {}

    future<StatusOr<std::pair<bool, Row>>> GetFuture() {
      return row_promise_.get_future();
    }

    future<bool> OnRow(Row row) {
      // assert(!row_received_);
      row_ = std::move(row);
      row_received_ = true;
      // Don't satisfy the promise before `OnStreamFinished`.
      //
      // The `CompletionQueue`, which this object holds a reference to, should
      // not be shut down before `OnStreamFinished` is called. In order to make
      // sure of that, satisying the `promise<>` is deferred until then - the
      // user shouldn't shutown the `CompleetionQue` before this whole
      // operations is done.
      return make_ready_future(false);
    }

    void OnStreamFinished(Status status) {
      if (row_received_) {
        // If we got a row we don't need to care about the stream status.
        row_promise_.set_value(std::make_pair(true, std::move(row_)));
        return;
      }
      if (status.ok()) {
        row_promise_.set_value(std::make_pair(false, Row("", {})));
      } else {
        row_promise_.set_value(std::move(status));
      }
    }

   private:
    Row row_;
    bool row_received_{};
    promise<StatusOr<std::pair<bool, Row>>> row_promise_;
  };

  RowSet row_set(std::move(row_key));
  std::int64_t const rows_limit = 1;
  auto handler = std::make_shared<AsyncReadRowHandler>();
  AsyncReadRows(cq,
                [handler](Row row) { return handler->OnRow(std::move(row)); },
                [handler](Status status) {
                  handler->OnStreamFinished(std::move(status));
                },
                std::move(row_set), rows_limit, std::move(filter));
  return handler->GetFuture();
}

}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable
}  // namespace cloud
}  // namespace google
