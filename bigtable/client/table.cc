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

#include "bigtable/client/table.h"

#include <bigtable/client/internal/unary_rpc_utils.h>
#include <thread>

#include "bigtable/client/internal/bulk_mutator.h"
#include "bigtable/client/internal/make_unique.h"

namespace btproto = ::google::bigtable::v2;

namespace {
[[noreturn]] void ReportPermanentFailures(
    char const* msg, grpc::Status const& status,
    std::vector<bigtable::FailedMutation> failures) {
#if GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
  throw bigtable::PermanentMutationFailure(msg, status, std::move(failures));
#else
  std::cerr << msg << "\n"
            << "Status: " << status.error_message() << " ["
            << status.error_code() << "] - " << status.error_details()
            << std::endl;
  for (auto const& failed : failures) {
    std::cerr << "Mutation " << failed.original_index() << " failed with"
              << failed.status().error_message() << " ["
              << failed.status().error_code() << "]" << std::endl;
  }
  std::cerr << "Aborting because exceptions are disabled." << std::endl;
  std::abort();
#endif  // GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
}
}  // namespace

namespace bigtable {
inline namespace BIGTABLE_CLIENT_NS {
// Call the `google.bigtable.v2.Bigtable.MutateRow` RPC repeatedly until
// successful, or until the policies in effect tell us to stop.
void Table::Apply(SingleRowMutation&& mut) {
  // Copy the policies in effect for this operation.  Many policy classes change
  // their state as the operation makes progress (or fails to make progress), so
  // we need fresh instances.
  auto rpc_policy = rpc_retry_policy_->clone();
  auto backoff_policy = rpc_backoff_policy_->clone();
  auto idempotent_policy = idempotent_mutation_policy_->clone();

  // Build the RPC request, try to minimize copying.
  btproto::MutateRowRequest request;
  request.set_table_name(table_name_);
  request.set_row_key(std::move(mut.row_key_));
  request.mutable_mutations()->Swap(&mut.ops_);
  bool const is_idempotent =
      std::all_of(request.mutations().begin(), request.mutations().end(),
                  [&idempotent_policy](btproto::Mutation const& m) {
                    return idempotent_policy->is_idempotent(m);
                  });

  btproto::MutateRowResponse response;
  while (true) {
    grpc::ClientContext client_context;
    rpc_policy->setup(client_context);
    backoff_policy->setup(client_context);
    metadata_update_policy_.setup(client_context);
    grpc::Status status =
        client_->Stub()->MutateRow(&client_context, request, &response);
    if (status.ok()) {
      return;
    }
    // It is up to the policy to terminate this loop, it could run
    // forever, but that would be a bad policy (pun intended).
    if (not rpc_policy->on_failure(status) or not is_idempotent) {
      std::vector<FailedMutation> failures;
      google::rpc::Status rpc_status;
      rpc_status.set_code(status.error_code());
      rpc_status.set_message(status.error_message());
      failures.emplace_back(SingleRowMutation(std::move(request)), rpc_status,
                            0);
      // TODO(#234) - just return the failures instead
      ReportPermanentFailures(
          "Permanent (or too many transient) errors in Table::Apply()", status,
          std::move(failures));
    }
    auto delay = backoff_policy->on_completion(status);
    std::this_thread::sleep_for(delay);
  }
}

// Call the `google.bigtable.v2.Bigtable.MutateRows` RPC repeatedly until
// successful, or until the policies in effect tell us to stop.  When the RPC
// is partially successful, this function retries only the mutations that did
// not succeed.
void Table::BulkApply(BulkMutation&& mut) {
  // Copy the policies in effect for this operation.  Many policy classes change
  // their state as the operation makes progress (or fails to make progress), so
  // we need fresh instances.
  auto backoff_policy = rpc_backoff_policy_->clone();
  auto retry_policy = rpc_retry_policy_->clone();
  auto idemponent_policy = idempotent_mutation_policy_->clone();

  internal::BulkMutator mutator(table_name_, *idemponent_policy,
                                std::forward<BulkMutation>(mut));

  grpc::Status status = grpc::Status::OK;
  while (mutator.HasPendingMutations()) {
    grpc::ClientContext client_context;
    backoff_policy->setup(client_context);
    retry_policy->setup(client_context);
    metadata_update_policy_.setup(client_context);

    status = mutator.MakeOneRequest(*client_->Stub(), client_context);
    if (not status.ok() and not retry_policy->on_failure(status)) {
      break;
    }
    auto delay = backoff_policy->on_completion(status);
    std::this_thread::sleep_for(delay);
  }
  auto failures = mutator.ExtractFinalFailures();
  if (not failures.empty()) {
    // TODO(#234) - just return the failures instead
    ReportPermanentFailures(
        "Permanent (or too many transient) errors in Table::BulkApply()",
        status, std::move(failures));
  }
}

RowReader Table::ReadRows(RowSet row_set, Filter filter) {
  return RowReader(client_, table_name(), std::move(row_set),
                   RowReader::NO_ROWS_LIMIT, std::move(filter),
                   rpc_retry_policy_->clone(), rpc_backoff_policy_->clone(),
                   metadata_update_policy_,
                   bigtable::internal::make_unique<
                       bigtable::internal::ReadRowsParserFactory>());
}

RowReader Table::ReadRows(RowSet row_set, std::int64_t rows_limit,
                          Filter filter) {
  if (rows_limit <= 0) {
    internal::RaiseInvalidArgument("rows_limit must be >0");
  }
  return RowReader(client_, table_name(), std::move(row_set), rows_limit,
                   std::move(filter), rpc_retry_policy_->clone(),
                   rpc_backoff_policy_->clone(), metadata_update_policy_,
                   bigtable::internal::make_unique<
                       bigtable::internal::ReadRowsParserFactory>());
}

std::pair<bool, Row> Table::ReadRow(std::string row_key, Filter filter) {
  RowSet row_set(std::move(row_key));
  std::int64_t const rows_limit = 1;
  RowReader reader =
      ReadRows(std::move(row_set), rows_limit, std::move(filter));
  auto it = reader.begin();
  if (it == reader.end()) {
    return std::make_pair(false, Row("", {}));
  }
  auto result = std::make_pair(true, std::move(*it));
  if (++it != reader.end()) {
    internal::RaiseRuntimeError(
        "internal error - RowReader returned 2 rows in ReadRow()");
  }
  return result;
}

bool Table::CheckAndMutateRow(std::string row_key, Filter filter,
                              std::vector<Mutation> true_mutations,
                              std::vector<Mutation> false_mutations) {
  using RpcUtils = internal::UnaryRpcUtils<DataClient>;
  using StubType = RpcUtils::StubType;
  btproto::CheckAndMutateRowRequest request;
  request.set_table_name(table_name());
  request.set_row_key(std::move(row_key));
  *request.mutable_predicate_filter() = filter.as_proto_move();
  for (auto& m : true_mutations) {
    *request.add_true_mutations() = std::move(m.op);
  }
  for (auto& m : false_mutations) {
    *request.add_false_mutations() = std::move(m.op);
  }
  auto response = RpcUtils::CallWithoutRetry(
      *client_, rpc_retry_policy_->clone(), metadata_update_policy_,
      &StubType::CheckAndMutateRow, request, "Table::CheckAndMutateRow");
  return response.predicate_matched();
}

Row Table::CallReadModifyWriteRowRequest(
    btproto::ReadModifyWriteRowRequest row_request) {
  auto error_message =
      "ReadModifyWriteRowRequest(" + row_request.table_name() + ")";
  auto response_row = RpcUtils::CallWithoutRetry(
      *client_, rpc_retry_policy_->clone(), metadata_update_policy_, &StubType::ReadModifyWriteRow,
      row_request, error_message.c_str());

  std::vector<bigtable::Cell> cells;
  for (auto& family : response_row.row().families()) {
    for (auto& column : family.columns()) {
      for (auto cell : column.cells()) {
        std::vector<std::string> labels;

        std::move(cell.mutable_labels()->begin(), cell.mutable_labels()->end(),
                  std::back_inserter(labels));
        bigtable::Cell new_cell(std::move(response_row.mutable_row()->key()),
                                std::move(family.name()),
                                std::move(column.qualifier()),
                                cell.timestamp_micros(),
                                std::move(cell.value()), std::move(labels));

        cells.emplace_back(std::move(new_cell));
      }
    }
  }

  return Row(response_row.row().key(), std::move(cells));
}

}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable
