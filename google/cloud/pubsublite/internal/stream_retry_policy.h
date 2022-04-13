// Copyright 2022 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUBLITE_INTERNAL_STREAM_RETRY_POLICY_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUBLITE_INTERNAL_STREAM_RETRY_POLICY_H

#include "google/cloud/internal/retry_policy.h"
#include "google/cloud/status.h"
#include <functional>
#include <unordered_set>

namespace google {
namespace cloud {
namespace pubsublite_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

class StreamRetryPolicy : public google::cloud::internal::RetryPolicy {
 public:
  bool OnFailure(Status const& s) override {
    return retryable_codes_.find(s.code()) != retryable_codes_.end();
  }

  bool IsExhausted() const override { return false; }

  bool IsPermanentFailure(Status const& s) const override {
    return retryable_codes_.find(s.code()) == retryable_codes_.end();
  }

 private:
  class StatusCodeHash {
   public:
    std::size_t operator()(StatusCode const& code) const {
      return hash_(static_cast<std::uint8_t>(code));
    }

   private:
    std::hash<std::uint8_t> hash_;
  };
  std::unordered_set<StatusCode, StatusCodeHash> retryable_codes_{
      StatusCode::kDeadlineExceeded, StatusCode::kAborted,
      StatusCode::kInternal,         StatusCode::kUnavailable,
      StatusCode::kUnknown,          StatusCode::kResourceExhausted};
};

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace pubsublite_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUBLITE_INTERNAL_STREAM_RETRY_POLICY_H
