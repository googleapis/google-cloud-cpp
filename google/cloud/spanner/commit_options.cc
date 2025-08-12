// Copyright 2021 Google LLC
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

#include "google/cloud/spanner/commit_options.h"
#include "google/cloud/spanner/options.h"

namespace google {
namespace cloud {
namespace spanner {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

CommitOptions::CommitOptions(Options const& opts)
    : return_stats_(opts.has<CommitReturnStatsOption>()
                        ? opts.get<CommitReturnStatsOption>()
                        : false) {
  if (opts.has<RequestPriorityOption>()) {
    request_priority_ = opts.get<RequestPriorityOption>();
  }
  if (opts.has<TransactionTagOption>()) {
    transaction_tag_ = opts.get<TransactionTagOption>();
  }
  if (opts.has<MaxCommitDelayOption>()) {
    max_commit_delay_ = opts.get<MaxCommitDelayOption>();
  }
  if (opts.has<ExcludeTransactionFromChangeStreamsOption>()) {
    exclude_txn_from_change_streams_ =
        opts.get<ExcludeTransactionFromChangeStreamsOption>();
  }
}

CommitOptions::operator Options() const {
  Options opts;
  if (return_stats_) opts.set<CommitReturnStatsOption>(true);
  if (request_priority_) opts.set<RequestPriorityOption>(*request_priority_);
  if (transaction_tag_) opts.set<TransactionTagOption>(*transaction_tag_);
  if (max_commit_delay_) opts.set<MaxCommitDelayOption>(*max_commit_delay_);
  if (exclude_txn_from_change_streams_) {
    opts.set<ExcludeTransactionFromChangeStreamsOption>(
        *exclude_txn_from_change_streams_);
  }
  return opts;
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace spanner
}  // namespace cloud
}  // namespace google
