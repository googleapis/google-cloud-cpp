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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INTERNAL_BULK_MUTATOR_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INTERNAL_BULK_MUTATOR_H

#include "google/cloud/bigtable/completion_queue.h"
#include "google/cloud/bigtable/data_client.h"
#include "google/cloud/bigtable/idempotent_mutation_policy.h"
#include "google/cloud/bigtable/internal/bigtable_stub.h"
#include "google/cloud/bigtable/internal/mutate_rows_limiter.h"
#include "google/cloud/bigtable/internal/retry_context.h"
#include "google/cloud/bigtable/version.h"
#include "google/cloud/internal/invoke_result.h"
#include "google/cloud/status.h"
#include <string>
#include <vector>

namespace google {
namespace cloud {
namespace bigtable_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

class BulkMutatorState {
 public:
  BulkMutatorState(std::string const& app_profile_id,
                   std::string const& table_name,
                   bigtable::IdempotentMutationPolicy& idempotent_policy,
                   bigtable::BulkMutation mut);

  bool HasPendingMutations() const {
    return pending_mutations_.entries_size() != 0;
  }

  /// Returns the Request parameter for the next MutateRows() RPC.
  google::bigtable::v2::MutateRowsRequest const& BeforeStart();

  /// Handle the result of a `Read()` operation on the MutateRows RPC.
  void OnRead(google::bigtable::v2::MutateRowsResponse response);

  /// Handle the result of a `Finish()` operation on the MutateRows() RPC.
  void OnFinish(google::cloud::Status finish_status);

  /// Terminate the retry loop and return all the failures.
  std::vector<bigtable::FailedMutation> OnRetryDone() &&;

  /// The status of the most recent stream.
  Status last_status() const { return last_status_; };

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
  std::vector<bigtable::FailedMutation> failures_;

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
    Idempotency idempotency;
    /// Set to `false` if the result is unknown.
    bool has_mutation_result;
    /**
     * The last known status for this annotation.
     *
     * If the final stream attempt has failing mutations, but ends with an OK
     * status, we return a `FailedMutation` made from `original_index` and
     * `status`. The value is meaningless if `has_mutation_result` is false.
     */
    Status status;
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
              bigtable::IdempotentMutationPolicy& idempotent_policy,
              bigtable::BulkMutation mut);

  /// Return true if there are pending mutations in the mutator
  bool HasPendingMutations() const { return state_.HasPendingMutations(); }

  /// Synchronously send one batch request to the given stub.
  grpc::Status MakeOneRequest(bigtable::DataClient& client,
                              grpc::ClientContext& client_context);

  /// Synchronously send one batch request to the given stub.
  Status MakeOneRequest(BigtableStub& stub, MutateRowsLimiter& limiter);

  /// Give up on any pending mutations, move them to the failures array.
  std::vector<bigtable::FailedMutation> OnRetryDone() &&;

 protected:
  BulkMutatorState state_;
  RetryContext retry_context_;
};

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INTERNAL_BULK_MUTATOR_H
