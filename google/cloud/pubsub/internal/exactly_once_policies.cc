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

#include "google/cloud/pubsub/internal/exactly_once_policies.h"
#include "absl/memory/memory.h"

namespace google {
namespace cloud {
namespace pubsub_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

auto constexpr kMaximumRetryTime = std::chrono::minutes(10);
auto constexpr kInitialBackoff = std::chrono::seconds(1);
auto constexpr kMaximumBackoff = std::chrono::minutes(1);

std::unique_ptr<pubsub::BackoffPolicy> ExactlyOnceBackoffPolicy() {
  return absl::make_unique<pubsub::ExponentialBackoffPolicy>(
      kInitialBackoff, kMaximumBackoff, 2.0);
}

ExactlyOnceRetryPolicy::ExactlyOnceRetryPolicy(std::string ack_id)
    : ack_id_(std::move(ack_id)),
      deadline_(std::chrono::steady_clock::now() + kMaximumRetryTime) {}

bool ExactlyOnceRetryPolicy::OnFailure(Status const& status) {
  return !IsExhausted() && !IsPermanentFailure(status);
}

bool ExactlyOnceRetryPolicy::IsExhausted() const {
  return std::chrono::steady_clock::now() >= deadline_;
}

bool ExactlyOnceRetryPolicy::IsPermanentFailure(Status const& status) const {
  auto const code = status.code();
  if (ExactlyOnceRetryable(code)) return false;
  auto const& metadata = status.error_info().metadata();
  return !std::any_of(metadata.begin(), metadata.end(), [this](auto const& kv) {
    return kv.first == ack_id_ && kv.second.rfind("TRANSIENT_FAILURE_", 0) == 0;
  });
}

bool ExactlyOnceRetryable(StatusCode code) {
  // Of these, `kDeadlineExceeded` might be controversial.  There is no (as of
  // this writing) mechanism for applications to set a deadline on these
  // requests. One can infer that any deadline error is due to an internal
  // deadline and therefore retryable.
  return (code == StatusCode::kDeadlineExceeded ||
          code == StatusCode::kResourceExhausted ||
          code == StatusCode::kAborted || code == StatusCode::kInternal ||
          code == StatusCode::kUnavailable);
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace pubsub_internal
}  // namespace cloud
}  // namespace google
