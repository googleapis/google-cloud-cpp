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

#include "google/cloud/spanner/version.h"

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

 private:
  bool return_stats_ = false;
};

}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_COMMIT_OPTIONS_H
