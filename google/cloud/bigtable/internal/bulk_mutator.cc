// Copyright 2017 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "google/cloud/bigtable/internal/bulk_mutator.h"
#include "google/cloud/bigtable/rpc_retry_policy.h"
#include "google/cloud/bigtable/table.h"
#include "google/cloud/internal/make_status.h"
#include "google/cloud/log.h"
#include <numeric>

namespace google {
namespace cloud {
namespace bigtable_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

namespace btproto = ::google::bigtable::v2;

BulkMutatorState::BulkMutatorState(
    std::string const& app_profile_id, std::string const& table_name,
    bigtable::IdempotentMutationPolicy& idempotent_policy,
    bigtable::BulkMutation mut) {
  // Every time the client library calls MakeOneRequest(), the data in the
  // "pending_*" variables initializes the next request.  So in the constructor
  // we start by putting the data on the "pending_*" variables.
  // Move the mutations to the "pending" request proto, this is a zero copy
  // optimization.
  mut.MoveTo(&pending_mutations_);
  pending_mutations_.set_app_profile_id(app_profile_id);
  pending_mutations_.set_table_name(table_name);

  // As we receive successful responses, we shrink the size of the request (only
  // those pending are present).  But if any fails we want to report their index
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
    // NOLINTNEXTLINE(modernize-use-emplace) - brace initializer
    pending_annotations_.push_back(
        Annotations{index++, idempotency, false, Status()});
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
  pending_annotations_.clear();

  return mutations_;
}

void BulkMutatorState::OnRead(
    google::bigtable::v2::MutateRowsResponse response) {
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
    // Note that we do not need to heed `RetryInfo` for the status of individual
    // entries. The server only ever includes `RetryInfo` as the final status of
    // the stream.
    auto status = MakeStatusFromRpcError(entry.status());
    // Successful responses are not even recorded, this class only reports
    // the failures.  The data for successful responses is discarded, because
    // this class takes ownership in the constructor.
    if (status.ok()) continue;
    auto& original = *mutations_.mutable_entries(static_cast<int>(index));
    // Failed responses are handled according to the current policies.
    if (SafeGrpcRetry::IsTransientFailure(status) &&
        (annotation.idempotency == Idempotency::kIdempotent)) {
      // Retryable requests are saved in the pending mutations, along with the
      // mapping from their index in pending_mutations_ to the original
      // vector and other miscellanea.
      pending_mutations_.add_entries()->Swap(&original);
      pending_annotations_.push_back(
          Annotations{annotation.original_index, annotation.idempotency,
                      annotation.has_mutation_result, std::move(status)});
    } else {
      // Failures are saved for reporting, notice that we avoid copying, and
      // we use the original index in the first request, not the one where it
      // failed.
      failures_.emplace_back(std::move(status), annotation.original_index);
    }
  }
}

void BulkMutatorState::OnFinish(Status finish_status,
                                bool enable_server_retries) {
  last_status_ = std::move(finish_status);
  bool const retryable =
      enable_server_retries &&
      google::cloud::internal::GetRetryInfo(last_status_).has_value();

  int index = 0;
  for (auto& annotation : annotations_) {
    if (annotation.has_mutation_result) {
      ++index;
      continue;
    }
    // If there are any mutations with unknown state, they need to be handled.
    auto& original = *mutations_.mutable_entries(index);
    if (retryable || annotation.idempotency == Idempotency::kIdempotent) {
      // If the mutation was retryable, move it to the pending mutations to try
      // again, along with their index.
      pending_mutations_.add_entries()->Swap(&original);
      pending_annotations_.push_back(std::move(annotation));
    } else {
      if (last_status_.ok()) {
        auto status = internal::InternalError(
            "The server never sent a confirmation for this mutation but the "
            "stream didn't fail either. This is most likely a bug, please "
            "report it at "
            "https://github.com/googleapis/google-cloud-cpp/issues/new",
            GCP_ERROR_INFO());
        failures_.emplace_back(std::move(status), annotation.original_index);
      } else {
        failures_.emplace_back(last_status_, annotation.original_index);
      }
    }
    ++index;
  }
}

std::vector<bigtable::FailedMutation> BulkMutatorState::OnRetryDone() && {
  std::vector<bigtable::FailedMutation> result(std::move(failures_));

  auto size = pending_mutations_.mutable_entries()->size();
  for (int idx = 0; idx != size; idx++) {
    auto& annotation = pending_annotations_[idx];
    if (annotation.has_mutation_result) {
      result.emplace_back(std::move(annotation.status),
                          annotation.original_index);
    } else if (!last_status_.ok()) {
      result.emplace_back(last_status_, annotation.original_index);
    } else {
      auto status = internal::InternalError(
          "The server never sent a confirmation for this mutation but the "
          "stream didn't fail either. This is most likely a bug, please "
          "report it at "
          "https://github.com/googleapis/google-cloud-cpp/issues/new",
          GCP_ERROR_INFO());
      result.emplace_back(std::move(status), annotation.original_index);
    }
  }

  return result;
}

BulkMutator::BulkMutator(std::string const& app_profile_id,
                         std::string const& table_name,
                         bigtable::IdempotentMutationPolicy& idempotent_policy,
                         bigtable::BulkMutation mut,
                         std::shared_ptr<OperationContext> operation_context)
    : state_(app_profile_id, table_name, idempotent_policy, std::move(mut)),
      operation_context_(std::move(operation_context)) {}

grpc::Status BulkMutator::MakeOneRequest(bigtable::DataClient& client,
                                         grpc::ClientContext& client_context) {
  // Send the request to the server.
  auto const& mutations = state_.BeforeStart();
  auto stream = client.MutateRows(&client_context, mutations);
  // Read the stream of responses.
  btproto::MutateRowsResponse response;
  while (stream->Read(&response)) {
    state_.OnRead(std::move(response));
  }
  // Handle any errors in the stream.
  auto grpc_status = stream->Finish();
  state_.OnFinish(MakeStatusFromRpcError(grpc_status));
  return grpc_status;
}

Status BulkMutator::MakeOneRequest(BigtableStub& stub,
                                   MutateRowsLimiter& limiter,
                                   Options const& options) {
  // Send the request to the server.
  auto const& mutations = state_.BeforeStart();

  // Configure the context
  auto context = std::make_shared<grpc::ClientContext>();
  google::cloud::internal::ConfigureContext(*context, options);
  operation_context_->PreCall(*context);
  bool enable_server_retries = options.get<EnableServerRetriesOption>();

  struct UnpackVariant {
    BulkMutatorState& state;
    MutateRowsLimiter& limiter;
    bool enable_server_retries;

    bool operator()(btproto::MutateRowsResponse r) {
      limiter.Update(r);
      state.OnRead(std::move(r));
      return true;
    }
    bool operator()(Status s) {
      state.OnFinish(std::move(s), enable_server_retries);
      return false;
    }
  };

  // Potentially throttle the request
  limiter.Acquire();

  // Read the stream of responses.
  auto stream = stub.MutateRows(context, options, mutations);
  while (absl::visit(UnpackVariant{state_, limiter, enable_server_retries},
                     stream->Read())) {
  }
  operation_context_->PostCall(*context, state_.last_status());
  return state_.last_status();
}

std::vector<bigtable::FailedMutation> BulkMutator::OnRetryDone() && {
  return std::move(state_).OnRetryDone();
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable_internal
}  // namespace cloud
}  // namespace google
