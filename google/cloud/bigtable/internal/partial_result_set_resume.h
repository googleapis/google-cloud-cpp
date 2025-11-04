// Copyright 2025 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INTERNAL_PARTIAL_RESULT_SET_RESUME_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INTERNAL_PARTIAL_RESULT_SET_RESUME_H

#include "google/cloud/bigtable/internal/partial_result_set_reader.h"
#include "google/cloud/bigtable/rpc_backoff_policy.h"
#include "google/cloud/bigtable/rpc_retry_policy.h"
#include "google/cloud/bigtable/version.h"
#include "google/cloud/backoff_policy.h"
#include "absl/types/optional.h"
#include <functional>
#include <memory>
#include <string>

namespace google {
namespace cloud {
namespace bigtable_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

/// Create a new PartialResultSetReader given a resume token value.
using PartialResultSetReaderFactory =
    std::function<std::unique_ptr<PartialResultSetReader>(std::string)>;

/**
 * A PartialResultSetReader that resumes the streaming RPC on retryable errors.
 */
class PartialResultSetResume : public PartialResultSetReader {
 public:
  PartialResultSetResume(
      PartialResultSetReaderFactory factory,
      google::cloud::Idempotency idempotency,
      std::unique_ptr<bigtable::RPCRetryPolicy> retry_policy,
      std::unique_ptr<bigtable::RPCBackoffPolicy> backoff_policy)
      : factory_(std::move(factory)),
        idempotency_(idempotency),
        retry_policy_prototype_(std::move(retry_policy)),
        backoff_policy_prototype_(std::move(backoff_policy)),
        reader_(factory_(std::string{})) {}

  ~PartialResultSetResume() override = default;

  void TryCancel() override;
  bool Read(absl::optional<std::string> const& resume_token,
            UnownedPartialResultSet& result) override;
  Status Finish() override;

 private:
  PartialResultSetReaderFactory factory_;
  google::cloud::Idempotency idempotency_;
  std::unique_ptr<bigtable::RPCRetryPolicy> retry_policy_prototype_;
  std::unique_ptr<bigtable::RPCBackoffPolicy> backoff_policy_prototype_;
  std::unique_ptr<PartialResultSetReader> reader_;
  absl::optional<Status> last_status_;
};

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INTERNAL_PARTIAL_RESULT_SET_RESUME_H
