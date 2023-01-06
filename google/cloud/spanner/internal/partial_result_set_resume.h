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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_INTERNAL_PARTIAL_RESULT_SET_RESUME_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_INTERNAL_PARTIAL_RESULT_SET_RESUME_H

#include "google/cloud/spanner/backoff_policy.h"
#include "google/cloud/spanner/internal/partial_result_set_reader.h"
#include "google/cloud/spanner/retry_policy.h"
#include "google/cloud/spanner/version.h"
#include "google/cloud/backoff_policy.h"
#include "google/cloud/internal/retry_policy.h"
#include "absl/types/optional.h"
#include <functional>
#include <memory>
#include <string>

namespace google {
namespace cloud {
namespace spanner_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

/// Create a new PartialResultSetReader given a resume token value.
using PartialResultSetReaderFactory =
    std::function<std::unique_ptr<PartialResultSetReader>(std::string)>;

/**
 * A PartialResultSetReader that resumes the streaming RPC on retryable errors.
 */
class PartialResultSetResume : public PartialResultSetReader {
 public:
  PartialResultSetResume(PartialResultSetReaderFactory factory,
                         google::cloud::Idempotency idempotency,
                         std::unique_ptr<spanner::RetryPolicy> retry_policy,
                         std::unique_ptr<spanner::BackoffPolicy> backoff_policy)
      : factory_(std::move(factory)),
        idempotency_(idempotency),
        retry_policy_prototype_(std::move(retry_policy)),
        backoff_policy_prototype_(std::move(backoff_policy)),
        child_(factory_(std::string{})) {}

  ~PartialResultSetResume() override = default;

  void TryCancel() override;
  absl::optional<PartialResultSet> Read(
      absl::optional<std::string> const& resume_token) override;
  Status Finish() override;

 private:
  PartialResultSetReaderFactory factory_;
  google::cloud::Idempotency idempotency_;
  std::unique_ptr<spanner::RetryPolicy> retry_policy_prototype_;
  std::unique_ptr<spanner::BackoffPolicy> backoff_policy_prototype_;
  std::unique_ptr<PartialResultSetReader> child_;
  absl::optional<Status> last_status_;
};

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace spanner_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_INTERNAL_PARTIAL_RESULT_SET_RESUME_H
