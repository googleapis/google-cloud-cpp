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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_INTERNAL_EXACTLY_ONCE_POLICIES_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_INTERNAL_EXACTLY_ONCE_POLICIES_H

#include "google/cloud/pubsub/backoff_policy.h"
#include "google/cloud/pubsub/retry_policy.h"
#include "google/cloud/pubsub/version.h"
#include <chrono>
#include <string>

namespace google {
namespace cloud {
namespace pubsub_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

/// A retry policy suitable to retry ack/nack messages when exactly-once is
/// enabled.
class ExactlyOnceRetryPolicy : public google::cloud::internal::RetryPolicy {
 public:
  explicit ExactlyOnceRetryPolicy(std::string ack_id);
  ~ExactlyOnceRetryPolicy() override = default;

  bool OnFailure(Status const&) override;
  bool IsExhausted() const override;
  bool IsPermanentFailure(Status const&) const override;

 private:
  std::string const ack_id_;
  std::chrono::steady_clock::time_point deadline_;
};

/// Build a backoff policy suitable to retry ack/nack messages when exactly-once
/// is enabled.
std::unique_ptr<pubsub::BackoffPolicy> ExactlyOnceBackoffPolicy();

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace pubsub_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_INTERNAL_EXACTLY_ONCE_POLICIES_H
