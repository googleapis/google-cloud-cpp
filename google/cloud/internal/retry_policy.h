// Copyright 2018 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_RETRY_POLICY_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_RETRY_POLICY_H

#include "google/cloud/status.h"
#include "google/cloud/version.h"
#include <chrono>
#include <memory>

namespace google {
namespace cloud {
inline namespace GOOGLE_CLOUD_CPP_NS {
namespace internal {

enum class Idempotency { kIdempotent, kNonIdempotent };

/**
 * Define the interface for retry policies.
 */
class RetryPolicy {
 public:
  virtual ~RetryPolicy() = default;

  //@{
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
  virtual bool OnFailure(Status const&) = 0;
  virtual bool IsExhausted() const = 0;
  virtual bool IsPermanentFailure(Status const&) const = 0;
  //@}
};

/**
 * Trait based RetryPolicy.
 *
 * @tparam StatusType the type used to represent success/failures.
 * @tparam RetryablePolicy the policy to decide if a status represents a
 *     permanent failure.
 */
template <typename RetryableTraitsP>
class TraitBasedRetryPolicy : public RetryPolicy {
 public:
  ///@{
  /**
   * @name type traits
   */
  /// The traits describing which errors are permanent failures
  using RetryableTraits = RetryableTraitsP;

  /// The status type used by the retry policy
  using StatusType = ::google::cloud::Status;
  ///@}

  ~TraitBasedRetryPolicy() override = default;

  virtual std::unique_ptr<TraitBasedRetryPolicy> clone() const = 0;

  bool IsPermanentFailure(Status const& status) const override {
    return RetryableTraits::IsPermanentFailure(status);
  }

  bool OnFailure(Status const& status) override {
    if (RetryableTraits::IsPermanentFailure(status)) {
      return false;
    }
    OnFailureImpl();
    return !IsExhausted();
  }

 protected:
  virtual void OnFailureImpl() = 0;
};

/**
 * Implement a simple "count errors and then stop" retry policy.
 *
 * @tparam StatusType the type used to represent success/failures.
 * @tparam RetryablePolicy the policy to decide if a status represents a
 *     permanent failure.
 */
template <typename RetryablePolicy>
class LimitedErrorCountRetryPolicy
    : public TraitBasedRetryPolicy<RetryablePolicy> {
 public:
  using BaseType = TraitBasedRetryPolicy<RetryablePolicy>;

  explicit LimitedErrorCountRetryPolicy(int maximum_failures)
      : failure_count_(0), maximum_failures_(maximum_failures) {}

  LimitedErrorCountRetryPolicy(LimitedErrorCountRetryPolicy&& rhs) noexcept
      : LimitedErrorCountRetryPolicy(rhs.maximum_failures_) {}
  LimitedErrorCountRetryPolicy(LimitedErrorCountRetryPolicy const& rhs) noexcept
      : LimitedErrorCountRetryPolicy(rhs.maximum_failures_) {}

  std::unique_ptr<BaseType> clone() const override {
    return std::unique_ptr<BaseType>(
        new LimitedErrorCountRetryPolicy(maximum_failures_));
  }
  bool IsExhausted() const override {
    return failure_count_ > maximum_failures_;
  }

 protected:
  void OnFailureImpl() override { ++failure_count_; }

 private:
  int failure_count_;
  int maximum_failures_;
};

/**
 * Implement a simple "keep trying for this time" retry policy.
 *
 * @tparam StatusType the type used to represent success/failures.
 * @tparam RetryablePolicy the policy to decide if a status represents a
 *     permanent failure.
 */
template <typename RetryablePolicy>
class LimitedTimeRetryPolicy : public TraitBasedRetryPolicy<RetryablePolicy> {
 public:
  using BaseType = TraitBasedRetryPolicy<RetryablePolicy>;

  /**
   * Constructor given a `std::chrono::duration<>` object.
   *
   * @tparam DurationRep a placeholder to match the `Rep` tparam for @p
   *     duration's type. The semantics of this template parameter are
   *     documented in `std::chrono::duration<>` (in brief, the underlying
   *     arithmetic type used to store the number of ticks), for our purposes it
   *     is simply a formal parameter.
   * @tparam DurationPeriod a placeholder to match the `Period` tparam for @p
   *     duration's type. The semantics of this template parameter are
   *     documented in `std::chrono::duration<>` (in brief, the length of the
   *     tick in seconds, expressed as a `std::ratio<>`), for our purposes it is
   *     simply a formal parameter.
   * @param maximum_duration the maximum time allowed before the policy expires,
   *     while the application can express this time in any units they desire,
   *     the class truncates to milliseconds.
   */
  template <typename DurationRep, typename DurationPeriod>
  explicit LimitedTimeRetryPolicy(
      std::chrono::duration<DurationRep, DurationPeriod> maximum_duration)
      : maximum_duration_(std::chrono::duration_cast<std::chrono::milliseconds>(
            maximum_duration)),
        deadline_(std::chrono::system_clock::now() + maximum_duration_) {}

  LimitedTimeRetryPolicy(LimitedTimeRetryPolicy&& rhs) noexcept
      : LimitedTimeRetryPolicy(rhs.maximum_duration_) {}
  LimitedTimeRetryPolicy(LimitedTimeRetryPolicy const& rhs)
      : LimitedTimeRetryPolicy(rhs.maximum_duration_) {}

  std::unique_ptr<BaseType> clone() const override {
    return std::unique_ptr<BaseType>(
        new LimitedTimeRetryPolicy(maximum_duration_));
  }
  bool IsExhausted() const override {
    return std::chrono::system_clock::now() >= deadline_;
  }

  std::chrono::system_clock::time_point deadline() const { return deadline_; }

 protected:
  void OnFailureImpl() override {}

 private:
  std::chrono::milliseconds maximum_duration_;
  std::chrono::system_clock::time_point deadline_;
};

}  // namespace internal
}  // namespace GOOGLE_CLOUD_CPP_NS
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_RETRY_POLICY_H
