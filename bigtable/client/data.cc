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

#include "bigtable/client/data.h"

#include <cstdlib>
#include <iostream>
#include <thread>

#include <absl/memory/memory.h>
#include <bigtable/client/detail/bulk_mutator.h>

namespace btproto = ::google::bigtable::v2;

namespace bigtable {
inline namespace BIGTABLE_CLIENT_NS {
std::unique_ptr<Table> Client::Open(const std::string& table_id) {
  std::string table_name = std::string("projects/") + project_ + "/instances/" +
                           instance_ + "/tables/" + table_id;
  std::unique_ptr<Table> table(new Table(this, table_name));
  return table;
}

void Table::Apply(SingleRowMutation&& mut) {
  // Copy the policies in effect for the operation.
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
    grpc::Status status =
        client_->Stub().MutateRow(&client_context, request, &response);
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
      throw PermanentMutationFailure(
          "retry policy exhausted or permanent error", std::move(failures));
    }
    auto delay = backoff_policy->on_completion(status);
    std::this_thread::sleep_for(delay);
  }
}

void Table::BulkApply(BulkMutation&& mut) {
  auto backoff_policy = rpc_backoff_policy_->clone();
  auto retry_policy = rpc_retry_policy_->clone();
  auto idemponent_policy = idempotent_mutation_policy_->clone();

  detail::BulkMutator mutator(table_name_, *idemponent_policy,
                              std::forward<BulkMutation>(mut));

  while (mutator.HasPendingMutations()) {
    grpc::ClientContext client_context;
    backoff_policy->setup(client_context);
    retry_policy->setup(client_context);

    auto status = mutator.MakeOneRequest(client_->Stub(), client_context);
    if (not status.ok() and not retry_policy->on_failure(status)) {
      break;
    }
    auto delay = backoff_policy->on_completion(status);
    std::this_thread::sleep_for(delay);
  }
  auto failures = mutator.ExtractFinalFailures();
  if (not failures.empty()) {
    throw PermanentMutationFailure("Permanent errors in Table::BulkApply()",
                                   std::move(failures));
  }
}

}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable
