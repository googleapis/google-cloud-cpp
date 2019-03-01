// Copyright 2018 Google Inc.
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

#include "google/cloud/bigtable/internal/table.h"
#include "google/cloud/bigtable/internal/bulk_mutator.h"
#include "google/cloud/bigtable/internal/unary_client_utils.h"
#include "google/cloud/internal/make_unique.h"
#include <thread>
#include <type_traits>

namespace btproto = ::google::bigtable::v2;

namespace google {
namespace cloud {
namespace bigtable {
inline namespace BIGTABLE_CLIENT_NS {
namespace noex {
using ClientUtils = bigtable::internal::noex::UnaryClientUtils<DataClient>;

static_assert(std::is_copy_assignable<bigtable::noex::Table>::value,
              "bigtable::noex::Table must be CopyAssignable");

// Call the `google.bigtable.v2.Bigtable.MutateRow` RPC repeatedly until
// successful, or until the policies in effect tell us to stop.
std::vector<FailedMutation> Table::Apply(SingleRowMutation mut) {
  // Copy the policies in effect for this operation.  Many policy classes change
  // their state as the operation makes progress (or fails to make progress), so
  // we need fresh instances.
  auto rpc_policy = rpc_retry_policy_->clone();
  auto backoff_policy = rpc_backoff_policy_->clone();
  auto idempotent_policy = idempotent_mutation_policy_->clone();

  // Build the RPC request, try to minimize copying.
  btproto::MutateRowRequest request;
  bigtable::internal::SetCommonTableOperationRequest<btproto::MutateRowRequest>(
      request, app_profile_id_.get(), table_name_.get());
  mut.MoveTo(request);

  bool const is_idempotent =
      std::all_of(request.mutations().begin(), request.mutations().end(),
                  [&idempotent_policy](btproto::Mutation const& m) {
                    return idempotent_policy->is_idempotent(m);
                  });

  btproto::MutateRowResponse response;
  std::vector<FailedMutation> failures;
  grpc::Status status;
  while (true) {
    grpc::ClientContext client_context;
    rpc_policy->Setup(client_context);
    backoff_policy->Setup(client_context);
    metadata_update_policy_.Setup(client_context);
    status = client_->MutateRow(&client_context, request, &response);
    if (status.ok()) {
      return failures;
    }
    // It is up to the policy to terminate this loop, it could run
    // forever, but that would be a bad policy (pun intended).
    if (!rpc_policy->OnFailure(status) || !is_idempotent) {
      google::rpc::Status rpc_status;
      rpc_status.set_code(status.error_code());
      rpc_status.set_message(status.error_message());
      failures.emplace_back(SingleRowMutation(std::move(request)), rpc_status,
                            0);
      status = grpc::Status(
          status.error_code(),
          "Permanent (or too many transient) errors in Table::Apply()");
      return failures;
    }
    auto delay = backoff_policy->OnCompletion(status);
    std::this_thread::sleep_for(delay);
  }
}

// Call the `google.bigtable.v2.Bigtable.MutateRows` RPC repeatedly until
// successful, or until the policies in effect tell us to stop.  When the RPC
// is partially successful, this function retries only the mutations that did
// not succeed.
std::vector<FailedMutation> Table::BulkApply(BulkMutation mut,
                                             grpc::Status& status) {
  // Copy the policies in effect for this operation.  Many policy classes change
  // their state as the operation makes progress (or fails to make progress), so
  // we need fresh instances.
  auto backoff_policy = rpc_backoff_policy_->clone();
  auto retry_policy = rpc_retry_policy_->clone();
  auto idemponent_policy = idempotent_mutation_policy_->clone();

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
  auto failures = mutator.ExtractFinalFailures();
  if (!status.ok()) {
    return failures;
  }
  if (!failures.empty()) {
    status = grpc::Status(
        grpc::StatusCode::INTERNAL,
        "Permanent (or too many transient) errors in Table::BulkApply()");
  }
  return failures;
}

RowReader Table::ReadRows(RowSet row_set, Filter filter) {
  return RowReader(client_, app_profile_id_, table_name_, std::move(row_set),
                   RowReader::NO_ROWS_LIMIT, std::move(filter),
                   rpc_retry_policy_->clone(), rpc_backoff_policy_->clone(),
                   metadata_update_policy_,
                   google::cloud::internal::make_unique<
                       bigtable::internal::ReadRowsParserFactory>());
}

RowReader Table::ReadRows(RowSet row_set, std::int64_t rows_limit,
                          Filter filter) {
  return RowReader(client_, app_profile_id_, table_name_, std::move(row_set),
                   rows_limit, std::move(filter), rpc_retry_policy_->clone(),
                   rpc_backoff_policy_->clone(), metadata_update_policy_,
                   google::cloud::internal::make_unique<
                       bigtable::internal::ReadRowsParserFactory>());
}

std::pair<bool, Row> Table::ReadRow(std::string row_key, Filter filter,
                                    Status& status) {
  RowSet row_set(std::move(row_key));
  std::int64_t const rows_limit = 1;
  RowReader reader =
      ReadRows(std::move(row_set), rows_limit, std::move(filter));
  auto it = reader.begin();
  if (it == reader.end()) {
    status = Status();
    return std::make_pair(false, Row("", {}));
  }
  if (!*it) {
    status = it->status();
    return std::make_pair(false, Row("", {}));
  }
  auto result = std::make_pair(true, std::move(**it));
  if (++it != reader.end()) {
    status = Status(StatusCode::kInternal,
                    "internal error - RowReader returned 2 rows in ReadRow()");
    return std::make_pair(false, Row("", {}));
  }
  return result;
}

bool Table::CheckAndMutateRow(std::string row_key, Filter filter,
                              std::vector<Mutation> true_mutations,
                              std::vector<Mutation> false_mutations,
                              grpc::Status& status) {
  btproto::CheckAndMutateRowRequest request;
  request.set_row_key(std::move(row_key));
  bigtable::internal::SetCommonTableOperationRequest<
      btproto::CheckAndMutateRowRequest>(request, app_profile_id_.get(),
                                         table_name_.get());
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
      *client_, rpc_retry_policy_->clone(), rpc_backoff_policy_->clone(),
      metadata_update_policy_, &DataClient::CheckAndMutateRow, request,
      "Table::CheckAndMutateRow", status, is_idempotent);

  return response.predicate_matched();
}

Row Table::CallReadModifyWriteRowRequest(
    btproto::ReadModifyWriteRowRequest const& request, grpc::Status& status) {
  auto response = ClientUtils::MakeNonIdemponentCall(
      *client_, rpc_retry_policy_->clone(), metadata_update_policy_,
      &DataClient::ReadModifyWriteRow, request, "ReadModifyWriteRowRequest",
      status);
  if (!status.ok()) {
    return Row("", {});
  }
  return internal::TransformReadModifyWriteRowResponse<
      btproto::ReadModifyWriteRowResponse>(response);
}

// Call the `google.bigtable.v2.Bigtable.SampleRowKeys` RPC until
// successful. When RPC is finished, this function returns the SampleRowKeys
// as a Collection specified by the user. If the RPC fails, it will keep
// retrying until the policies in effect tell us to stop.
void Table::SampleRowsImpl(
    std::function<void(bigtable::RowKeySample)> const& inserter,
    std::function<void()> const& clearer, grpc::Status& status) {
  // Copy the policies in effect for this operation.
  auto backoff_policy = rpc_backoff_policy_->clone();
  auto retry_policy = rpc_retry_policy_->clone();

  // Build the RPC request for SampleRowKeys
  btproto::SampleRowKeysRequest request;
  btproto::SampleRowKeysResponse response;
  bigtable::internal::SetCommonTableOperationRequest<
      btproto::SampleRowKeysRequest>(request, app_profile_id_.get(),
                                     table_name_.get());

  while (true) {
    grpc::ClientContext client_context;
    backoff_policy->Setup(client_context);
    retry_policy->Setup(client_context);
    metadata_update_policy_.Setup(client_context);

    auto stream = client_->SampleRowKeys(&client_context, request);
    while (stream->Read(&response)) {
      // Assuming collection will be either list or vector.
      bigtable::RowKeySample row_sample;
      row_sample.offset_bytes = response.offset_bytes();
      row_sample.row_key = std::move(*response.mutable_row_key());
      inserter(std::move(row_sample));
    }
    status = stream->Finish();
    if (status.ok()) {
      break;
    }
    if (!retry_policy->OnFailure(status)) {
      status = grpc::Status(grpc::StatusCode::INTERNAL,
                            "No more retries allowed as per policy.");
      return;
    }
    clearer();
    auto delay = backoff_policy->OnCompletion(status);
    std::this_thread::sleep_for(delay);
  }
}

}  // namespace noex
}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable
}  // namespace cloud
}  // namespace google
