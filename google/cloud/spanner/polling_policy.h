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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_POLLING_POLICY_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_POLLING_POLICY_H

#include "google/cloud/spanner/backoff_policy.h"
#include "google/cloud/spanner/retry_policy.h"
#include "google/cloud/spanner/version.h"
#include "google/cloud/status.h"

namespace google {
namespace cloud {
namespace spanner {
inline namespace SPANNER_CLIENT_NS {

/**
 * Control the Cloud Spanner C++ client library behavior with respect to polling
 * on long running operations.
 *
 * Some operations in Cloud Spanner return a `google.longrunning.Operation`
 * object. As their name implies, these objects represent requests that may take
 * a long time to complete, in the case of Cloud Spanner some operations may
 * take tens of seconds or even 30 minutes to complete.
 *
 * The Cloud Spanner C++ client library models these long running operations
 * as a `google::cloud::future<StatusOr<T>>`, where `T` represents the final
 * result of the operation. In the background, the library polls the service
 * until the operation completes (or fails) and then satisfies the future.
 *
 * This class defines the interface for policies that control the behavior of
 * this polling loop.
 *
 * @see https://aip.dev/151 for more information on long running operations.
 */
class PollingPolicy {
 public:
  virtual ~PollingPolicy() = default;

  /**
   * Return a copy of the current policy.
   *
   * This function is called at the beginning of the polling loop. Policies that
   * are based on relative time should restart their timers when this function
   * is called.
   */
  virtual std::unique_ptr<PollingPolicy> clone() const = 0;

  /**
   * A callback to indicate that a polling attempt failed.
   *
   * This is called when a polling request fails. Note that this callback is not
   * invoked when the polling request succeeds with "operation not done".
   *
   * @return true if the failure should be treated as transient and the polling
   *     loop should continue.
   */
  virtual bool OnFailure(google::cloud::Status const& status) = 0;

  /**
   * How long should the polling loop wait before trying again.
   */
  virtual std::chrono::milliseconds WaitPeriod() = 0;
};

/**
 * Combine a RetryPolicy and a BackoffPolicy to create simple polling policies.
 */
template <typename Retry = LimitedTimeRetryPolicy,
          typename Backoff = ExponentialBackoffPolicy>
class GenericPollingPolicy : public PollingPolicy {
 public:
  GenericPollingPolicy(Retry retry_policy, Backoff backoff_policy)
      : retry_policy_(std::move(retry_policy)),
        backoff_policy_(std::move(backoff_policy)) {}

  //@{
  std::unique_ptr<PollingPolicy> clone() const override {
    return std::unique_ptr<PollingPolicy>(new GenericPollingPolicy(*this));
  }

  bool OnFailure(google::cloud::Status const& status) override {
    return retry_policy_.OnFailure(status);
  }

  std::chrono::milliseconds WaitPeriod() override {
    return backoff_policy_.OnCompletion();
  }
  //@}

 private:
  Retry retry_policy_;
  Backoff backoff_policy_;
};

}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_POLLING_POLICY_H
