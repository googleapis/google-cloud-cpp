// Copyright 2020 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_COMMIT_OPTIONS_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_COMMIT_OPTIONS_H

#include "google/cloud/spanner/request_priority.h"
#include "google/cloud/spanner/version.h"
#include "absl/types/optional.h"
#include <string>

namespace google {
namespace cloud {
namespace spanner {
inline namespace SPANNER_CLIENT_NS {

/**
 * Set options on calls to `spanner::Client::Commit()`.
 *
 * @par Example
 * @snippet samples.cc commit-options
 */
class CommitOptions {
 public:
  /// Default options: no stats.
  CommitOptions() = default;

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

 private:
  // Note that CommitRequest.request_options.request_tag is ignored,
  // so we do not even provide a mechanism to specify one.
  bool return_stats_ = false;
  absl::optional<RequestPriority> request_priority_;
  absl::optional<std::string> transaction_tag_;
};

}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_COMMIT_OPTIONS_H
