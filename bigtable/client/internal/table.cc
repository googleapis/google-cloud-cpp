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

#include "bigtable/client/internal/table.h"

#include <thread>

#include "bigtable/client/internal/bulk_mutator.h"
#include "bigtable/client/internal/make_unique.h"

namespace btproto = ::google::bigtable::v2;

namespace bigtable {
inline namespace BIGTABLE_CLIENT_NS {
namespace noex {

// Call the `google.bigtable.v2.Bigtable.MutateRow` RPC repeatedly until
// successful, or until the policies in effect tell us to stop.
std::vector<FailedMutation> Table::Apply(SingleRowMutation&& mut) {
  // Copy the policies in effect for this operation.  Many policy classes change
  // their state as the operation makes progress (or fails to make progress), so
  // we need fresh instances.
  auto rpc_policy = rpc_retry_policy_->clone();
  auto backoff_policy = rpc_backoff_policy_->clone();
  auto idempotent_policy = idempotent_mutation_policy_->clone();

  // Build the RPC request, try to minimize copying.
  btproto::MutateRowRequest request;
  request.set_table_name(table_name_);
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
    rpc_policy->setup(client_context);
    backoff_policy->setup(client_context);
    status = client_->Stub()->MutateRow(&client_context, request, &response);
    if (status.ok()) {
      return failures;
    }
    // It is up to the policy to terminate this loop, it could run
    // forever, but that would be a bad policy (pun intended).
    if (not rpc_policy->on_failure(status) or not is_idempotent) {
      google::rpc::Status rpc_status;
      rpc_status.set_code(status.error_code());
      rpc_status.set_message(status.error_message());
      failures.emplace_back(SingleRowMutation(std::move(request)), rpc_status,
                            0);
      // TODO(#234) - just return the failures instead
      status = grpc::Status(
          status.error_code(),
          "Permanent (or too many transient) errors in Table::Apply()");
      return failures;
    }
    auto delay = backoff_policy->on_completion(status);
    std::this_thread::sleep_for(delay);
  }
}

// Call the `google.bigtable.v2.Bigtable.MutateRows` RPC repeatedly until
// successful, or until the policies in effect tell us to stop.  When the RPC
// is partially successful, this function retries only the mutations that did
// not succeed.
std::vector<FailedMutation> Table::BulkApply(BulkMutation&& mut,
                                             grpc::Status& status) {
  // Copy the policies in effect for this operation.  Many policy classes change
  // their state as the operation makes progress (or fails to make progress), so
  // we need fresh instances.
  auto backoff_policy = rpc_backoff_policy_->clone();
  auto retry_policy = rpc_retry_policy_->clone();
  auto idemponent_policy = idempotent_mutation_policy_->clone();

  internal::BulkMutator mutator(table_name_, *idemponent_policy,
                                std::forward<BulkMutation>(mut));
  while (mutator.HasPendingMutations()) {
    grpc::ClientContext client_context;
    backoff_policy->setup(client_context);
    retry_policy->setup(client_context);

    status = mutator.MakeOneRequest(*client_->Stub(), client_context);
    if (not status.ok() and not retry_policy->on_failure(status)) {
      break;
    }
    auto delay = backoff_policy->on_completion(status);
    std::this_thread::sleep_for(delay);
  }
  auto failures = mutator.ExtractFinalFailures();
  if (not status.ok()) {
    return failures;
  }
  if (not failures.empty()) {
    // TODO(#234) - just return the failures instead
    status = grpc::Status(
        grpc::StatusCode::INTERNAL,
        "Permanent (or too many transient) errors in Table::BulkApply()");
  }
  return failures;
}

RowReader Table::ReadRows(RowSet row_set, Filter filter, bool raise_on_error) {
  return RowReader(client_, table_name(), std::move(row_set),
                   RowReader::NO_ROWS_LIMIT, std::move(filter),
                   rpc_retry_policy_->clone(), rpc_backoff_policy_->clone(),
                   bigtable::internal::make_unique<
                       bigtable::internal::ReadRowsParserFactory>(),
                   raise_on_error);
}

RowReader Table::ReadRows(RowSet row_set, std::int64_t rows_limit,
                          Filter filter, bool raise_on_error) {
  return RowReader(client_, table_name(), std::move(row_set), rows_limit,
                   std::move(filter), rpc_retry_policy_->clone(),
                   rpc_backoff_policy_->clone(),
                   bigtable::internal::make_unique<
                       bigtable::internal::ReadRowsParserFactory>(),
                   raise_on_error);
}

std::pair<bool, Row> Table::ReadRow(std::string row_key, Filter filter,
                                    grpc::Status& status) {
  RowSet row_set(std::move(row_key));
  std::int64_t const rows_limit = 1;
  RowReader reader =
      ReadRows(std::move(row_set), rows_limit, std::move(filter));
  auto it = reader.begin();
  if (it == reader.end()) {
    status = reader.Finish();
    return std::make_pair(false, Row("", {}));
  }
  auto result = std::make_pair(true, std::move(*it));
  if (++it != reader.end()) {
    status =
        grpc::Status(grpc::StatusCode::INTERNAL,
                     "internal error - RowReader returned 2 rows in ReadRow()");
    return std::make_pair(false, Row("", {}));
  }
  return result;
}

}  // namespace noex
}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable
