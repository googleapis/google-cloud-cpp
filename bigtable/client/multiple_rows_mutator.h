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

#ifndef BIGTABLE_CLIENT_MULTIPLE_ROWS_MUTATOR_H_
#define BIGTABLE_CLIENT_MULTIPLE_ROWS_MUTATOR_H_

#include <bigtable/client/idempotent_mutation_policy.h>

#include <google/bigtable/v2/bigtable.grpc.pb.h>

namespace bigtable {
inline namespace BIGTABLE_CLIENT_NS {
namespace detail {
/// Keep the state in the Apply(MultipleRowMutations&&) member function.
class MultipleRowsMutator {
 public:
  MultipleRowsMutator(std::string const& table_name,
                      IdempotentMutationPolicy& idempotent_policy,
                      MultipleRowMutations&& mut);

  /// Return true if there are pending mutations in the mutator
  bool has_pending_mutations() const {
    return pending_mutations.entries_size() != 0;
  }

  /// Send one batch request to the given stub...
  grpc::Status make_one_request(
      google::bigtable::v2::Bigtable::StubInterface& stub,
      grpc::ClientContext& client_context);

  /// Give up on any pending mutations, move them to the failures array.
  std::vector<FailedMutation> extract_final_failures();

 private:
  /// Get ready for a new request
  void prepare_for_request();

  /// Process a single response.
  void process_response(google::bigtable::v2::MutateRowsResponse& response);

  /// A request has finished and we have processed all the responses.
  void finish_request();

  /// Accumulate any permanent failures and the list of mutations we gave up on.
  std::vector<FailedMutation> failures;

  /// The current request
  google::bigtable::v2::MutateRowsRequest mutations;

  /// Mapping from the index in `current_request` to the index in the original
  /// request
  std::vector<int> original_index;

  /// If true, the corresponding mutation is idempotent according to the policies in effect.
  std::vector<bool> is_idempotent;

  /// If true, the result for that mutation, in the current_request is known,
  /// used to find missing results.
  std::vector<bool> has_mutation_result;

  /// Accumulate mutations for the next request.
  google::bigtable::v2::MutateRowsRequest pending_mutations;

  /// Accumulate the indices of mutations for the next request.
  std::vector<int> pending_original_index;

  /// Accumulate the idempotency of mutations for the next request.
  std::vector<bool> pending_is_idempotent;
};
}  // namespace detail
}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable

#endif  // BIGTABLE_CLIENT_MULTIPLE_ROWS_MUTATOR_H_
