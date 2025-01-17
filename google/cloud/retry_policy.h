// Copyright 2023 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_RETRY_POLICY_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_RETRY_POLICY_H

#include "google/cloud/status.h"
#include "google/cloud/version.h"

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

/**
 * Define the interface for retry policies.
 */
class RetryPolicy {
 public:
  virtual ~RetryPolicy() = default;

  ///@{
  /**
   * @name Control retry loop duration.
   *
   * This functions are typically used in a retry loop, where they control
   * whether to continue, whether a failure should be retried, and finally
   * how to format the error message.
   *
   * @code
   * std::unique_ptr<RetryPolicy> policy = ....;
   * Status status;
   * while (!policy->IsExhausted()) {
   *   auto response = try_rpc();  // typically `response` is StatusOr<T>
   *   if (response.ok()) return response;
   *   status = std::move(response).status();
   *   if (!policy->OnFailure(response->status())) {
   *     if (policy->IsPermanentFailure(response->status()) {
   *       return StatusModifiedToSayPermanentFailureCausedTheProblem(status);
   *     }
   *     return StatusModifiedToSayPolicyExhaustionCausedTheProblem(status);
   *   }
   *   // sleep, which may exhaust the policy, even if it was not exhausted in
   *   // the last call.
   * }
   * return StatusModifiedToSayPolicyExhaustionCausedTheProblem(status);
   * @endcode
   */

  /// Return true if the retry loop should continue after @p status.
  virtual bool OnFailure(Status const& status) = 0;

  /// Return true if the retry policy should stop as the retry limit has been
  /// reached.
  virtual bool IsExhausted() const = 0;

  /// Return true if @p status is treated as a permanent (and therefore
  /// non-retryable) error.
  virtual bool IsPermanentFailure(Status const& status) const = 0;
  ///@}

  /// Creates a new instance of the policy, reset to the initial state.
  virtual std::unique_ptr<RetryPolicy> clone() const = 0;
};

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_RETRY_POLICY_H
