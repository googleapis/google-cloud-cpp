// Copyright 2022 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_MUTATION_BRANCH_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_MUTATION_BRANCH_H

#include "google/cloud/bigtable/version.h"

namespace google {
namespace cloud {
namespace bigtable {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

/// The branch taken by a Table::CheckAndMutateRow operation.
enum class MutationBranch {
  /// The predicate provided to CheckAndMutateRow did not match and the
  /// `false_mutations` (if any) were applied.
  kPredicateNotMatched,
  /// The predicate provided to CheckAndMutateRow matched and the
  /// `true_mutations` (if any) were applied.
  kPredicateMatched,
};

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_MUTATION_BRANCH_H
