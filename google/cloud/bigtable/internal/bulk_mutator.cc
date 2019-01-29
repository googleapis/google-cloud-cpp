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

#include "google/cloud/bigtable/internal/bulk_mutator.h"
#include "google/cloud/bigtable/internal/table.h"
#include "google/cloud/bigtable/rpc_retry_policy.h"
#include "google/cloud/bigtable/table_strong_types.h"
#include <numeric>

namespace google {
namespace cloud {
namespace bigtable {
inline namespace BIGTABLE_CLIENT_NS {
namespace internal {

namespace btproto = google::bigtable::v2;

BulkMutator::BulkMutator(bigtable::AppProfileId const& app_profile_id,
                         bigtable::TableId const& table_name,
                         IdempotentMutationPolicy& idempotent_policy,
                         BulkMutation&& mut) {
  // Every time the client library calls MakeOneRequest(), the data in the
  // "pending_*" variables initializes the next request.  So in the constructor
  // we start by putting the data on the "pending_*" variables.
  // Move the mutations to the "pending" request proto, this is a zero copy
  // optimization.
  mut.MoveTo(&pending_mutations_);
  bigtable::internal::SetCommonTableOperationRequest<
      btproto::MutateRowsRequest>(pending_mutations_, app_profile_id.get(),
                                  table_name.get());
  // As we receive successful responses, we shrink the size of the request (only
  // those pending are resent).  But if any fails we want to report their index
  // in the original sequence provided by the user.  So this vector maps from
  // the index in the current array to the index in the original array.
  pending_annotations_.reserve(pending_mutations_.entries_size());
  int index = 0;
  for (auto const& e : pending_mutations_.entries()) {
    // This is a giant && across all the mutations for each row.
    auto r = std::all_of(e.mutations().begin(), e.mutations().end(),
                         [&idempotent_policy](btproto::Mutation const& m) {
                           return idempotent_policy.is_idempotent(m);
                         });
    pending_annotations_.push_back(Annotations{index++, r, false});
  }
}

grpc::Status BulkMutator::MakeOneRequest(bigtable::DataClient& client,
                                         grpc::ClientContext& client_context) {
  PrepareForRequest();
  // Send the request to the server and read the resulting result stream.
  auto stream = client.MutateRows(&client_context, mutations_);
  btproto::MutateRowsResponse response;
  while (stream->Read(&response)) {
    ProcessResponse(response);
  }
  FinishRequest();
  return stream->Finish();
}

void BulkMutator::PrepareForRequest() {
  mutations_.Swap(&pending_mutations_);
  annotations_.swap(pending_annotations_);
  for (auto& a : annotations_) {
    a.has_mutation_result = false;
  }
  pending_mutations_ = {};
  bigtable::internal::SetCommonTableOperationRequest<
      btproto::MutateRowsRequest>(
      pending_mutations_, mutations_.app_profile_id(), mutations_.table_name());
  pending_annotations_ = {};
}

void BulkMutator::ProcessResponse(
    google::bigtable::v2::MutateRowsResponse& response) {
  for (auto& entry : *response.mutable_entries()) {
    auto index = entry.index();
    if (index < 0 || annotations_.size() <= std::size_t(index)) {
      // TODO(#72) - decide how this is logged.
      continue;
    }
    auto& annotation = annotations_[index];
    annotation.has_mutation_result = true;
    auto& status = entry.status();
    auto const code = static_cast<grpc::StatusCode>(status.code());
    // Successful responses are not even recorded, this class only reports
    // the failures.  The data for successful responses is discarded, because
    // this class takes ownership in the constructor.
    if (grpc::StatusCode::OK == code) {
      continue;
    }
    auto& original = *mutations_.mutable_entries(index);
    // Failed responses are handled according to the current policies.
    if (SafeGrpcRetry::IsTransientFailure(code) && annotation.is_idempotent) {
      // Retryable requests are saved in the pending mutations, along with the
      // mapping from their index in pending_mutations_ to the original
      // vector and other miscellanea.
      pending_mutations_.add_entries()->Swap(&original);
      pending_annotations_.push_back(annotation);
    } else {
      // Failures are saved for reporting, notice that we avoid copying, and
      // we use the original index in the first request, not the one where it
      // failed.
      failures_.emplace_back(SingleRowMutation(std::move(original)),
                             std::move(*entry.mutable_status()),
                             annotation.original_index);
    }
  }
}

void BulkMutator::FinishRequest() {
  int index = 0;
  for (auto const& annotation : annotations_) {
    if (annotation.has_mutation_result) {
      ++index;
      continue;
    }
    // If there are any mutations with unknown state, they need to be handled.
    auto& original = *mutations_.mutable_entries(index);
    if (annotation.is_idempotent) {
      // If the mutation was retryable, move it to the pending mutations to try
      // again, along with their index.
      pending_mutations_.add_entries()->Swap(&original);
      pending_annotations_.push_back(annotation);
    } else {
      // These are most likely an effect of a broken stream. We do not know
      // their error code, and we cannot retry them. Report them as UNKNOWN in
      // the failure list.
      google::rpc::Status status;
      status.set_code(grpc::StatusCode::UNKNOWN);
      status.set_message(
          "Never got a confirmation for this mutation. Most likely stream was "
          "broken before its status was sent. It's not idempotent, so we can't "
          "retry it.");
      failures_.emplace_back(
          FailedMutation(SingleRowMutation(std::move(original)), status,
                         annotation.original_index));
    }
    ++index;
  }
}

std::vector<FailedMutation> BulkMutator::ExtractFinalFailures() {
  std::vector<FailedMutation> result(std::move(failures_));
  // These are most likely an effect of a broken stream. We do not know
  // their error code and there are not going to be any more retries. Report
  // them as UNKNOWN in the failure list.
  google::rpc::Status status;
  status.set_code(grpc::StatusCode::UNKNOWN);
  status.set_message(
      "Never got a confirmation for this mutation. Most likely stream was "
      "broken before its status was sent.");
  int idx = 0;
  for (auto& mutation : *pending_mutations_.mutable_entries()) {
    int original_index = pending_annotations_[idx++].original_index;
    result.emplace_back(FailedMutation(SingleRowMutation(std::move(mutation)),
                                       status, original_index));
  }
  return result;
}

}  // namespace internal
}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable
}  // namespace cloud
}  // namespace google
