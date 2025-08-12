// Copyright 2020 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_COMMIT_OPTIONS_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_COMMIT_OPTIONS_H

#include "google/cloud/spanner/request_priority.h"
#include "google/cloud/spanner/version.h"
#include "google/cloud/options.h"
#include "absl/types/optional.h"
#include <chrono>
#include <string>

namespace google {
namespace cloud {
namespace spanner {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

/**
 * Set options on calls to `spanner::Client::Commit()`.
 *
 * @deprecated Use [`Options`](@ref google::cloud::Options) instead,
 *     and set (as needed)
 *     [`CommitReturnStatsOption`](
 *     @ref google::cloud::spanner::CommitReturnStatsOption),
 *     [`RequestPriorityOption`](
 *     @ref google::cloud::spanner::RequestPriorityOption), or
 *     [`TransactionTagOption`](
 *     @ref google::cloud::spanner::TransactionTagOption).
 *
 * @par Example
 * @snippet samples.cc commit-options
 */
class CommitOptions {
 public:
  /// Default options: no stats.
  CommitOptions() = default;

  /**
   * Constructs from the new, recommended way to represent options
   * of all varieties, `google::cloud::Options`.
   */
  explicit CommitOptions(Options const& opts);

  /**
   * Converts to the new, recommended way to represent options of all
   * varieties, `google::cloud::Options`.
   */
  explicit operator Options() const;

  /// Set whether the `CommitResult` should contain `CommitStats`.
  CommitOptions& set_return_stats(bool return_stats) {
    return_stats_ = return_stats;
    return *this;
  }

  /// Whether the `CommitResult` should contain `CommitStats`.
  bool return_stats() const { return return_stats_; }

  /// Set the priority of the `spanner::Client::Commit()` call.
  CommitOptions& set_request_priority(
      absl::optional<RequestPriority> request_priority) {
    request_priority_ = std::move(request_priority);
    return *this;
  }

  /// The priority of the `spanner::Client::Commit()` call.
  absl::optional<RequestPriority> request_priority() const {
    return request_priority_;
  }

  /**
   * Set the transaction tag for the `spanner::Client::Commit()` call.
   * Ignored for the overload that already takes a `spanner::Transaction`.
   */
  CommitOptions& set_transaction_tag(
      absl::optional<std::string> transaction_tag) {
    transaction_tag_ = std::move(transaction_tag);
    return *this;
  }

  /// The transaction tag for the `spanner::Client::Commit()` call.
  absl::optional<std::string> const& transaction_tag() const {
    return transaction_tag_;
  }

  // Set the max commit delay of the `spanner::Client::Commit()` call.
  CommitOptions& set_max_commit_delay(
      absl::optional<std::chrono::milliseconds> max_commit_delay) {
    max_commit_delay_ = std::move(max_commit_delay);
    return *this;
  }

  // The max commit delay for the `spanner::Client::Commit()` call.
  absl::optional<std::chrono::milliseconds> const& max_commit_delay() const {
    return max_commit_delay_;
  }

  CommitOptions& set_exclude_txn_from_change_streams(bool exclude) {
    exclude_txn_from_change_streams_ = exclude;
    return *this;
  }

  absl::optional<bool> const& exclude_txn_from_change_streams() const {
    return exclude_txn_from_change_streams_;
  }

 private:
  // Note that CommitRequest.request_options.request_tag is ignored,
  // so we do not even provide a mechanism to specify one.
  bool return_stats_ = false;
  absl::optional<RequestPriority> request_priority_;
  absl::optional<std::string> transaction_tag_;
  absl::optional<std::chrono::milliseconds> max_commit_delay_;
  absl::optional<bool> exclude_txn_from_change_streams_;
};

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace spanner
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_COMMIT_OPTIONS_H
