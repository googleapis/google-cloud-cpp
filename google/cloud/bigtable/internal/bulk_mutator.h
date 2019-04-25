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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INTERNAL_BULK_MUTATOR_H_
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INTERNAL_BULK_MUTATOR_H_

#include "google/cloud/bigtable/async_operation.h"
#include "google/cloud/bigtable/bigtable_strong_types.h"
#include "google/cloud/bigtable/completion_queue.h"
#include "google/cloud/bigtable/data_client.h"
#include "google/cloud/bigtable/idempotent_mutation_policy.h"
#include "google/cloud/bigtable/table_strong_types.h"
#include "google/cloud/bigtable/version.h"
#include "google/cloud/internal/invoke_result.h"
#include "google/cloud/internal/make_unique.h"

namespace google {
namespace cloud {
namespace bigtable {
inline namespace BIGTABLE_CLIENT_NS {
namespace internal {
class BulkMutatorState {
 public:
  BulkMutatorState(bigtable::AppProfileId const& app_profile_id,
                   bigtable::TableId const& table_name,
                   IdempotentMutationPolicy& idempotent_policy,
                   BulkMutation mut);

  bool HasPendingMutations() const {
    return pending_mutations_.entries_size() != 0;
  }

  /// Returns the Request parameter for the next MutateRows() RPC.
  google::bigtable::v2::MutateRowsRequest const& BeforeStart();

  /**
   * Handle the result of a `Read()` operation on the MutateRows RPC.
   *
   * Returns the original index of any successful operations.
   */
  std::vector<int> OnRead(google::bigtable::v2::MutateRowsResponse& response);

  /// Handle the result of a `Finish()` operation on the MutateRows() RPC.
  void OnFinish(google::cloud::Status finish_status);

  /**
   * Return the permanently failed mutations.
   *
   * This will return all the mutations which we've learned are definite
   * failures since since the last call to this member function.
   *
   * Whatever is returned, will not be returned by `OnRetryDone()`.
   */
  std::vector<FailedMutation> ConsumeAccumulatedFailures();

  /// Terminate the retry loop and return all the failures.
  std::vector<FailedMutation> OnRetryDone() &&;

 private:
  /// The current request proto.
  google::bigtable::v2::MutateRowsRequest mutations_;

  /**
   * The status of the last MutateRows() RPC
   *
   * This is useful when the RPC terminates before the state of each mutation is
   * known, the result of the RPC is applied to any mutation with an unknown
   * result.
   */
  google::cloud::Status last_status_;

  /// Accumulate any permanent failures and the list of mutations we gave up on.
  std::vector<FailedMutation> failures_;

  /**
   * A small type to keep the annotations about pending mutations.
   *
   * As we process a MutateRows RPC we need to track the partial results for
   * each mutation in the request. This class groups them in a small POD-type.
   */
  struct Annotations {
    /**
     * The index of this mutation in the original request.
     *
     * Each time the request is retried the operations might be reordered, but
     * we want to report any permanent failures using the index in the original
     * request provided by the application.
     */
    int original_index;
    bool is_idempotent;
    /// Set to `false` if the result is unknown.
    bool has_mutation_result;
  };

  /// The annotations about the current bulk request.
  std::vector<Annotations> annotations_;

  /// Accumulate mutations for the next request.
  google::bigtable::v2::MutateRowsRequest pending_mutations_;

  /// Accumulate annotations for the next request.
  std::vector<Annotations> pending_annotations_;
};

/// Keep the state in the Table::BulkApply() member function.
class BulkMutator {
 public:
  BulkMutator(bigtable::AppProfileId const& app_profile_id,
              bigtable::TableId const& table_name,
              IdempotentMutationPolicy& idempotent_policy, BulkMutation mut);

  /// Return true if there are pending mutations in the mutator
  bool HasPendingMutations() const { return state_.HasPendingMutations(); }

  /// Synchronously send one batch request to the given stub.
  grpc::Status MakeOneRequest(bigtable::DataClient& client,
                              grpc::ClientContext& client_context);

  /// Give up on any pending mutations, move them to the failures array.
  std::vector<FailedMutation> OnRetryDone() &&;

 protected:
  BulkMutatorState state_;
};

/**
 * Async-friendly version BulkMutator.
 *
 * It satisfies the requirements to be used in `AsyncRetryOp`.
 *
 * It extends the normal BulkMutator with logic to do its job asynchronously.
 * Conceptually it reimplements MakeOneRequest in an async way.
 */
class AsyncBulkMutatorNoex {
 public:
  using MutationsSucceededFunctor =
      std::function<void(CompletionQueue&, std::vector<int>)>;
  using MutationsFailedFunctor =
      std::function<void(CompletionQueue&, std::vector<FailedMutation>)>;
  using AttemptFinishedFunctor =
      std::function<void(CompletionQueue&, grpc::Status&)>;

  AsyncBulkMutatorNoex(std::shared_ptr<bigtable::DataClient> client,
                       bigtable::AppProfileId const& app_profile_id,
                       bigtable::TableId const& table_name,
                       IdempotentMutationPolicy& idempotent_policy,
                       BulkMutation mut)
      : state_(app_profile_id, table_name, idempotent_policy, std::move(mut)),
        client_(std::move(client)) {}

  AsyncBulkMutatorNoex(std::shared_ptr<bigtable::DataClient> client,
                       bigtable::AppProfileId const& app_profile_id,
                       bigtable::TableId const& table_name,
                       IdempotentMutationPolicy& idempotent_policy,
                       MutationsSucceededFunctor mutations_succeeded_callback,
                       MutationsFailedFunctor mutations_failed_callback,
                       AttemptFinishedFunctor attempt_finished_callback,
                       BulkMutation mut)
      : state_(app_profile_id, table_name, idempotent_policy, std::move(mut)),
        client_(std::move(client)),
        mutations_succeeded_callback_(std::move(mutations_succeeded_callback)),
        mutations_failed_callback_(std::move(mutations_failed_callback)),
        attempt_finished_callback_(std::move(attempt_finished_callback)) {}

  using Request = google::bigtable::v2::MutateRowsRequest;
  using Response = std::vector<FailedMutation>;

  template <typename Functor,
            typename std::enable_if<
                google::cloud::internal::is_invocable<Functor, CompletionQueue&,
                                                      grpc::Status&>::value,
                int>::type valid_callback_type = 0>
  std::shared_ptr<AsyncOperation> Start(
      CompletionQueue& cq, std::unique_ptr<grpc::ClientContext> context,
      Functor&& callback) {
    auto const& mutations = state_.BeforeStart();
    return cq.MakeUnaryStreamRpc(
        *client_, &DataClient::AsyncMutateRows, mutations, std::move(context),
        [this](CompletionQueue& cq, const grpc::ClientContext&,
               google::bigtable::v2::MutateRowsResponse& response) {
          std::vector<int> succeeded_mutations = state_.OnRead(response);
          if (mutations_succeeded_callback_) {
            mutations_succeeded_callback_(cq, std::move(succeeded_mutations));
          }
          if (mutations_failed_callback_) {
            mutations_failed_callback_(cq, state_.ConsumeAccumulatedFailures());
          }
        },
        FinishedCallback<Functor>(*this, std::forward<Functor>(callback)));
  }

  std::vector<FailedMutation> AccumulatedResult() {
    return std::move(state_).OnRetryDone();
  }

 private:
  template <typename Functor,
            typename std::enable_if<
                google::cloud::internal::is_invocable<Functor, CompletionQueue&,
                                                      grpc::Status&>::value,
                int>::type valid_callback_type = 0>
  struct FinishedCallback {
    FinishedCallback(AsyncBulkMutatorNoex& parent, Functor callback)
        : parent_(parent), callback_(callback) {}

    void operator()(CompletionQueue& cq, grpc::ClientContext& context,
                    grpc::Status& status) {
      parent_.state_.OnFinish(MakeStatusFromRpcError(status));

      if (parent_.state_.HasPendingMutations() && status.ok()) {
        status = grpc::Status(grpc::StatusCode::UNAVAILABLE,
                              "Some mutations were not confirmed");
      }
      if (parent_.attempt_finished_callback_) {
        grpc::Status status_copy(status);
        parent_.attempt_finished_callback_(cq, status_copy);
      }
      callback_(cq, status);
    }

    // The user of AsyncBulkMutatorNoex has to make sure that it is not
    // destructed before all callbacks return, so we have a guarantee that this
    // reference is valid for as long as we don't call callback_.
    AsyncBulkMutatorNoex& parent_;
    Functor callback_;
  };

 private:
  BulkMutatorState state_;
  std::shared_ptr<bigtable::DataClient> client_;
  MutationsSucceededFunctor mutations_succeeded_callback_;
  MutationsFailedFunctor mutations_failed_callback_;
  AttemptFinishedFunctor attempt_finished_callback_;
};

}  // namespace internal
}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INTERNAL_BULK_MUTATOR_H_
