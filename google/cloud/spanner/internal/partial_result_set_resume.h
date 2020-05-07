// Copyright 2019 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_INTERNAL_PARTIAL_RESULT_SET_RESUME_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_INTERNAL_PARTIAL_RESULT_SET_RESUME_H

#include "google/cloud/spanner/backoff_policy.h"
#include "google/cloud/spanner/internal/partial_result_set_reader.h"
#include "google/cloud/spanner/retry_policy.h"
#include <functional>
#include <memory>

namespace google {
namespace cloud {
namespace spanner {
inline namespace SPANNER_CLIENT_NS {
namespace internal {

/// Create a new PartialResultSetReader given a resume token value.
using PartialResultSetReaderFactory =
    std::function<std::unique_ptr<PartialResultSetReader>(std::string)>;

enum class Idempotency {
  kNotIdempotent,
  kIdempotent,
};

/**
 * A PartialResultSetReader that resumes the streaming RPC on retryable errors.
 */
class PartialResultSetResume : public PartialResultSetReader {
 public:
  PartialResultSetResume(PartialResultSetReaderFactory factory,
                         Idempotency is_idempotent,
                         std::unique_ptr<RetryPolicy> retry_policy,
                         std::unique_ptr<BackoffPolicy> backoff_policy)
      : factory_(std::move(factory)),
        is_idempotent_(is_idempotent),
        retry_policy_prototype_(std::move(retry_policy)),
        backoff_policy_prototype_(std::move(backoff_policy)),
        child_(factory_(last_resume_token_)) {}

  ~PartialResultSetResume() override = default;

  void TryCancel() override;
  optional<google::spanner::v1::PartialResultSet> Read() override;
  Status Finish() override;

 private:
  PartialResultSetReaderFactory factory_;
  Idempotency is_idempotent_;
  std::unique_ptr<RetryPolicy> retry_policy_prototype_;
  std::unique_ptr<BackoffPolicy> backoff_policy_prototype_;
  std::string last_resume_token_;
  std::unique_ptr<PartialResultSetReader> child_;
  optional<Status> last_status_;
};

}  // namespace internal
}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_INTERNAL_PARTIAL_RESULT_SET_RESUME_H
