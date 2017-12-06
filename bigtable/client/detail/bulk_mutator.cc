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

#include "bigtable/client/detail/bulk_mutator.h"

#include <numeric>

#include <bigtable/client/rpc_retry_policy.h>

namespace bigtable {
inline namespace BIGTABLE_CLIENT_NS {
namespace detail {

namespace btproto = google::bigtable::v2;

BulkMutator::BulkMutator(std::string const &table_name,
                         IdempotentMutationPolicy &policy, BulkMutation &&mut) {
  mutations.set_table_name(table_name);
  pending_mutations.set_table_name(table_name);
  mut.MoveTo(&pending_mutations);
  pending_original_index.resize(pending_mutations.entries_size());
  std::iota(pending_original_index.begin(), pending_original_index.end(), 0);
  pending_is_idempotent.reserve(pending_mutations.entries_size());
  for (auto const &e : pending_mutations.entries()) {
    auto r = std::all_of(e.mutations().begin(), e.mutations().end(),
                         [&policy](btproto::Mutation const &m) {
                           return policy.is_idempotent(m);
                         });
    pending_is_idempotent.push_back(r);
  }
}

grpc::Status BulkMutator::MakeOneRequest(btproto::Bigtable::StubInterface &stub,
                                         grpc::ClientContext &client_context) {
  PrepareForRequest();
  auto stream = stub.MutateRows(&client_context, mutations);
  btproto::MutateRowsResponse response;
  while (stream->Read(&response)) {
    ProcessResponse(response);
  }
  FinishRequest();
  return stream->Finish();
}

void BulkMutator::PrepareForRequest() {
  mutations.Swap(&pending_mutations);
  original_index.swap(pending_original_index);
  is_idempotent.swap(pending_is_idempotent);
  has_mutation_result = std::vector<bool>(mutations.entries_size());
  pending_mutations = {};
  pending_original_index = {};
  pending_is_idempotent = {};
}

void BulkMutator::ProcessResponse(
    google::bigtable::v2::MutateRowsResponse &response) {
  for (auto &entry : *response.mutable_entries()) {
    auto index = entry.index();
    if (index < 0 or has_mutation_result.size() <= std::size_t(index)) {
      // TODO(#72) - decide how this is logged.
      continue;
    }
    has_mutation_result[index] = true;
    auto &status = entry.status();
    auto code = static_cast<grpc::StatusCode>(status.code());
    if (code == grpc::OK) {
      continue;
    }
    auto &original = *mutations.mutable_entries(index);
    if (IsRetryableStatusCode(code) and is_idempotent[index]) {
      pending_mutations.add_entries()->Swap(&original);
      pending_original_index.push_back(original_index[index]);
      pending_is_idempotent.push_back(is_idempotent[index]);
    } else {
      failures.emplace_back(SingleRowMutation(std::move(original)),
                            std::move(*entry.mutable_status()),
                            original_index[index]);
    }
  }
}

void BulkMutator::FinishRequest() {
  // ... if there are any mutations with unknown state, try again ...
  int index = 0;
  for (auto has_result : has_mutation_result) {
    if (has_result) {
      ++index;
      continue;
    }
    auto &original = *mutations.mutable_entries(index);
    if (is_idempotent[index]) {
      pending_mutations.add_entries()->Swap(&original);
      pending_original_index.push_back(original_index[index]);
      pending_is_idempotent.push_back(is_idempotent[index]);
    } else {
      google::rpc::Status ok_status;
      ok_status.set_code(grpc::StatusCode::OK);
      failures.emplace_back(
          FailedMutation(SingleRowMutation(std::move(original)), ok_status,
                         original_index[index]));
    }
    ++index;
  }
}

std::vector<FailedMutation> BulkMutator::ExtractFinalFailures() {
  std::vector<FailedMutation> result(std::move(failures));
  google::rpc::Status ok_status;
  ok_status.set_code(grpc::StatusCode::OK);
  for (auto &mutation : *pending_mutations.mutable_entries()) {
    result.emplace_back(
        FailedMutation(SingleRowMutation(std::move(mutation)), ok_status));
  }
  return result;
}

}  // namespace detail
}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable
