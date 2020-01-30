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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INTERNAL_BULK_MUTATOR_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INTERNAL_BULK_MUTATOR_H

#include "google/cloud/bigtable/completion_queue.h"
#include "google/cloud/bigtable/data_client.h"
#include "google/cloud/bigtable/idempotent_mutation_policy.h"
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
  BulkMutatorState(std::string const& app_profile_id,
                   std::string const& table_name,
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
  BulkMutator(std::string const& app_profile_id, std::string const& table_name,
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

}  // namespace internal
}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INTERNAL_BULK_MUTATOR_H
