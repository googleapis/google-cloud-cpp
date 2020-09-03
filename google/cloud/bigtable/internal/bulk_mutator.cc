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
#include "google/cloud/bigtable/rpc_retry_policy.h"
#include "google/cloud/bigtable/table.h"
#include "google/cloud/log.h"
#include <numeric>

namespace google {
namespace cloud {
namespace bigtable {
inline namespace BIGTABLE_CLIENT_NS {
namespace internal {

namespace btproto = google::bigtable::v2;

using google::cloud::internal::Idempotency;

BulkMutatorState::BulkMutatorState(std::string const& app_profile_id,
                                   std::string const& table_name,
                                   IdempotentMutationPolicy& idempotent_policy,
                                   BulkMutation mut) {
  // Every time the client library calls MakeOneRequest(), the data in the
  // "pending_*" variables initializes the next request.  So in the constructor
  // we start by putting the data on the "pending_*" variables.
  // Move the mutations to the "pending" request proto, this is a zero copy
  // optimization.
  mut.MoveTo(&pending_mutations_);
  pending_mutations_.set_app_profile_id(app_profile_id);
  pending_mutations_.set_table_name(table_name);

  // As we receive successful responses, we shrink the size of the request (only
  // those pending are resent).  But if any fails we want to report their index
  // in the original sequence provided by the user. This vector maps from the
  // index in the current sequence of mutations to the index in the original
  // sequence of mutations.
  pending_annotations_.reserve(pending_mutations_.entries_size());

  // We save the idempotency of each mutation, to be used later as we decide if
  // they should be retried or not.
  int index = 0;
  for (auto const& e : pending_mutations_.entries()) {
    // This is a giant && across all the mutations for each row.
    auto is_idempotent =
        std::all_of(e.mutations().begin(), e.mutations().end(),
                    [&idempotent_policy](btproto::Mutation const& m) {
                      return idempotent_policy.is_idempotent(m);
                    });
    auto idempotency =
        is_idempotent ? Idempotency::kIdempotent : Idempotency::kNonIdempotent;
    pending_annotations_.push_back(Annotations{index++, idempotency, false});
  }
}

google::bigtable::v2::MutateRowsRequest const& BulkMutatorState::BeforeStart() {
  mutations_.Swap(&pending_mutations_);
  annotations_.swap(pending_annotations_);
  for (auto& a : annotations_) {
    a.has_mutation_result = false;
  }
  pending_mutations_ = {};
  pending_mutations_.set_app_profile_id(mutations_.app_profile_id());
  pending_mutations_.set_table_name(mutations_.table_name());
  pending_annotations_ = {};

  return mutations_;
}

std::vector<int> BulkMutatorState::OnRead(
    google::bigtable::v2::MutateRowsResponse& response) {
  std::vector<int> res;
  for (auto& entry : *response.mutable_entries()) {
    // The type of `entry.index()` is a 64-bit int. But we can never create more
    // than std::numeric_limits<std::size_t>::max() entries in the request
    // (which might be a 32-bit number, depending on the platform), so the
    // following test only fails if the server has a bug:
    if (entry.index() < 0 ||
        static_cast<std::size_t>(entry.index()) >= annotations_.size()) {
      // There is no sensible way to return an error from here, the server did
      // something completely unexpected.
      GCP_LOG(ERROR) << "Invalid mutation index received from the server, got="
                     << entry.index() << ", expected in range=[0,"
                     << annotations_.size() << ")";
      continue;
    }
    auto const index = static_cast<std::size_t>(entry.index());
    auto& annotation = annotations_[index];
    annotation.has_mutation_result = true;
    auto& status = entry.status();
    auto const code = static_cast<grpc::StatusCode>(status.code());
    // Successful responses are not even recorded, this class only reports
    // the failures.  The data for successful responses is discarded, because
    // this class takes ownership in the constructor.
    if (grpc::StatusCode::OK == code) {
      res.push_back(annotation.original_index);
      continue;
    }
    auto& original = *mutations_.mutable_entries(static_cast<int>(index));
    // Failed responses are handled according to the current policies.
    if (SafeGrpcRetry::IsTransientFailure(code) &&
        (annotation.idempotency == Idempotency::kIdempotent)) {
      // Retryable requests are saved in the pending mutations, along with the
      // mapping from their index in pending_mutations_ to the original
      // vector and other miscellanea.
      pending_mutations_.add_entries()->Swap(&original);
      pending_annotations_.push_back(annotation);
    } else {
      // Failures are saved for reporting, notice that we avoid copying, and
      // we use the original index in the first request, not the one where it
      // failed.
      failures_.emplace_back(std::move(*entry.mutable_status()),
                             annotation.original_index);
    }
  }
  return res;
}

void BulkMutatorState::OnFinish(google::cloud::Status finish_status) {
  last_status_ = std::move(finish_status);

  int index = 0;
  for (auto const& annotation : annotations_) {
    if (annotation.has_mutation_result) {
      ++index;
      continue;
    }
    // If there are any mutations with unknown state, they need to be handled.
    auto& original = *mutations_.mutable_entries(index);
    if (annotation.idempotency == Idempotency::kIdempotent) {
      // If the mutation was retryable, move it to the pending mutations to try
      // again, along with their index.
      pending_mutations_.add_entries()->Swap(&original);
      pending_annotations_.push_back(annotation);
    } else {
      if (last_status_.ok()) {
        google::cloud::Status status(
            google::cloud::StatusCode::kInternal,
            "The server never sent a confirmation for this mutation but the "
            "stream didn't fail either. This is most likely a bug, please "
            "report it at "
            "https://github.com/googleapis/google-cloud-cpp/issues/new");
        failures_.emplace_back(
            FailedMutation(status, annotation.original_index));
      } else {
        failures_.emplace_back(
            FailedMutation(last_status_, annotation.original_index));
      }
    }
    ++index;
  }
}

std::vector<FailedMutation> BulkMutatorState::ConsumeAccumulatedFailures() {
  std::vector<FailedMutation> res;
  res.swap(failures_);
  return res;
}

std::vector<FailedMutation> BulkMutatorState::OnRetryDone() && {
  std::vector<FailedMutation> result(std::move(failures_));

  auto size = pending_mutations_.mutable_entries()->size();
  for (int idx = 0; idx != size; idx++) {
    int original_index = pending_annotations_[idx].original_index;
    if (last_status_.ok()) {
      google::cloud::Status status(
          google::cloud::StatusCode::kInternal,
          "The server never sent a confirmation for this mutation but the "
          "stream didn't fail either. This is most likely a bug, please "
          "report it at "
          "https://github.com/googleapis/google-cloud-cpp/issues/new");
      result.emplace_back(status, original_index);
    } else {
      result.emplace_back(last_status_, original_index);
    }
  }

  return result;
}

BulkMutator::BulkMutator(std::string const& app_profile_id,
                         std::string const& table_name,
                         IdempotentMutationPolicy& idempotent_policy,
                         BulkMutation mut)
    : state_(app_profile_id, table_name, idempotent_policy, std::move(mut)) {}

grpc::Status BulkMutator::MakeOneRequest(bigtable::DataClient& client,
                                         grpc::ClientContext& client_context) {
  // Send the request to the server.
  auto const& mutations = state_.BeforeStart();
  auto stream = client.MutateRows(&client_context, mutations);
  // Read the stream of responses.
  btproto::MutateRowsResponse response;
  while (stream->Read(&response)) {
    state_.OnRead(response);
  }
  // Handle any errors in the stream.
  auto grpc_status = stream->Finish();
  state_.OnFinish(MakeStatusFromRpcError(grpc_status));
  return grpc_status;
}

std::vector<FailedMutation> BulkMutator::OnRetryDone() && {
  return std::move(state_).OnRetryDone();
}

}  // namespace internal
}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable
}  // namespace cloud
}  // namespace google
