// Copyright 2019 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_COMMIT_RESULT_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_COMMIT_RESULT_H

#include "google/cloud/spanner/timestamp.h"
#include "google/cloud/spanner/version.h"
#include "google/cloud/status_or.h"
#include "google/cloud/stream_range.h"
#include "absl/types/optional.h"
#include <cstddef>
#include <cstdint>
#include <vector>

namespace google {
namespace cloud {
namespace spanner {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

/**
 * Statistics returned for a committed Transaction.
 */
struct CommitStats {
  std::int64_t mutation_count;  // total number of mutations
};

/**
 * The result of committing a Transaction.
 */
struct CommitResult {
  /// The Cloud Spanner timestamp at which the transaction committed.
  Timestamp commit_timestamp;

  /// Additional statistics about the committed transaction.
  absl::optional<CommitStats> commit_stats;
};

/**
 * The result of committing a Transaction containing a batch of mutation
 * groups.  See the batched form of `Client::CommitAtLeastOnce()`.
 */
struct BatchedCommitResult {
  /// The mutation groups applied in this batch. Each value is an index into
  /// the `std::vector<Mutations>` passed to `Client::CommitAtLeastOnce()`.
  std::vector<std::size_t> indexes;

  /// If OK, the Cloud Spanner timestamp at which the transaction committed,
  /// and otherwise the reason why the commit failed.
  StatusOr<Timestamp> commit_timestamp;
};

/**
 * Represents the stream of `BatchedCommitResult` objects returned from the
 * batched `Client::CommitAtLeastOnce()`.
 */
using BatchedCommitResultStream = StreamRange<BatchedCommitResult>;

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace spanner
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_COMMIT_RESULT_H
