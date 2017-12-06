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

#include "bigtable/client/rpc_retry_policy.h"

namespace bigtable {
inline namespace BIGTABLE_CLIENT_NS {
namespace detail {

namespace btproto = google::bigtable::v2;

BulkMutator::BulkMutator(std::string const &table_name,
                         IdempotentMutationPolicy &policy, BulkMutation &&mut) {
  // Every time the client library calls MakeOneRequest() the data in the
  // "pending_*" variables initializes the next request.  So in the constructor
  // we start by putting the data on the "pending_*" variables.
  pending_mutations_.set_table_name(table_name);
  // Move the mutations to the "pending" request proto, this is a zero copy
  // optimization.
  mut.MoveTo(&pending_mutations_);
  // As we receive successful responses we shrink the size of the request (only
  // those pending are resent).  But if any fails we want to report their index
  // in the original sequence provided by the user.  So this vector maps from
  // the index in the current array, to the index in the original array.
  pending_original_index_.resize(pending_mutations_.entries_size());
  std::iota(pending_original_index_.begin(), pending_original_index_.end(), 0);
  // We also want to know which mutations are idempotent.
  pending_is_idempotent_.reserve(pending_mutations_.entries_size());
  for (auto const &e : pending_mutations_.entries()) {
    // This is a giant && across all the mutations for each row.
    auto r = std::all_of(e.mutations().begin(), e.mutations().end(),
                         [&policy](btproto::Mutation const &m) {
                           return policy.is_idempotent(m);
                         });
    pending_is_idempotent_.push_back(r);
  }

  // Make a copy of the table name into the request proto, that way we do not
  // have to initialize it ever again.
  mutations_.set_table_name(table_name);
}

grpc::Status BulkMutator::MakeOneRequest(btproto::Bigtable::StubInterface &stub,
                                         grpc::ClientContext &client_context) {
  PrepareForRequest();
  // Send the request to the server and read the resulting result stream.
  auto stream = stub.MutateRows(&client_context, mutations_);
  btproto::MutateRowsResponse response;
  while (stream->Read(&response)) {
    ProcessResponse(response);
  }
  FinishRequest();
  return stream->Finish();
}

void BulkMutator::PrepareForRequest() {
  mutations_.Swap(&pending_mutations_);
  original_index_.swap(pending_original_index_);
  is_idempotent_.swap(pending_is_idempotent_);
  has_mutation_result_ = std::vector<bool>(mutations_.entries_size());
  pending_mutations_ = {};
  pending_original_index_ = {};
  pending_is_idempotent_ = {};
}

void BulkMutator::ProcessResponse(
    google::bigtable::v2::MutateRowsResponse &response) {
  for (auto &entry : *response.mutable_entries()) {
    auto index = entry.index();
    if (index < 0 or has_mutation_result_.size() <= std::size_t(index)) {
      // TODO(#72) - decide how this is logged.
      continue;
    }
    has_mutation_result_[index] = true;
    auto &status = entry.status();
    auto const code = static_cast<grpc::StatusCode>(status.code());
    // Successful responses are not even recorded, this class only reports
    // the failures.  The data for successful responses is discarded, because
    // this class takes ownership in the constructor.
    if (grpc::OK == code) {
      continue;
    }
    auto &original = *mutations_.mutable_entries(index);
    // Failed responses are handled according to the current policies.
    if (IsRetryableStatusCode(code) and is_idempotent_[index]) {
      // Retryable requests are saved in the pending mutations, along with the
      // mapping from their index in pending_mutations_ to the original
      // vector and other miscellanea.
      pending_mutations_.add_entries()->Swap(&original);
      pending_original_index_.push_back(original_index_[index]);
      pending_is_idempotent_.push_back(is_idempotent_[index]);
    } else {
      // Failures are saved for reporting, notice that we avoid copying, and
      // we use the original index in the first request, not the one where it
      // failed.
      failures_.emplace_back(SingleRowMutation(std::move(original)),
                            std::move(*entry.mutable_status()),
                            original_index_[index]);
    }
  }
}

void BulkMutator::FinishRequest() {
  int index = 0;
  for (auto has_result : has_mutation_result_) {
    if (has_result) {
      ++index;
      continue;
    }
    // If there are any mutations with unknown state, they need to be handled.
    auto &original = *mutations_.mutable_entries(index);
    if (is_idempotent_[index]) {
      // If the mutation was retryable, move it to the pending mutations to try
      // again, along with their index.
      pending_mutations_.add_entries()->Swap(&original);
      pending_original_index_.push_back(original_index_[index]);
      pending_is_idempotent_.push_back(is_idempotent_[index]);
    } else {
      // These are weird failures.  We do not know their error code, and we
      // cannot retry them.  Report them as OK in the failure list.
      google::rpc::Status ok_status;
      ok_status.set_code(grpc::StatusCode::OK);
      failures_.emplace_back(
          FailedMutation(SingleRowMutation(std::move(original)), ok_status,
                         original_index_[index]));
    }
    ++index;
  }
}

std::vector<FailedMutation> BulkMutator::ExtractFinalFailures() {
  std::vector<FailedMutation> result(std::move(failures_));
  google::rpc::Status ok_status;
  ok_status.set_code(grpc::StatusCode::OK);
  for (auto &mutation : *pending_mutations_.mutable_entries()) {
    result.emplace_back(
        FailedMutation(SingleRowMutation(std::move(mutation)), ok_status));
  }
  return result;
}

}  // namespace detail
}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable
