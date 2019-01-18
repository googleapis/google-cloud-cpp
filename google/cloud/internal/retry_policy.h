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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_RETRY_POLICY_H_
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_RETRY_POLICY_H_

#include "google/cloud/version.h"
#include <chrono>
#include <memory>

namespace google {
namespace cloud {
inline namespace GOOGLE_CLOUD_CPP_NS {
namespace internal {
/**
 * Define the interface for retry policies.
 *
 * @tparam StatusType the type used to represent success/failures.
 * @tparam RetryablePolicy the policy to decide if a status represents a
 *     permanent failure.
 */
template <typename StatusType, typename RetryablePolicy>
class RetryPolicy {
 public:
  virtual ~RetryPolicy() = default;

  virtual std::unique_ptr<RetryPolicy> clone() const = 0;

  bool OnFailure(StatusType const& status) {
    if (RetryablePolicy::IsPermanentFailure(status)) {
      return false;
    }
    OnFailureImpl();
    return !IsExhausted();
  }
  virtual bool IsExhausted() const = 0;

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
template <typename StatusType, typename RetryablePolicy>
class LimitedErrorCountRetryPolicy
    : public RetryPolicy<StatusType, RetryablePolicy> {
 public:
  using BaseType = RetryPolicy<StatusType, RetryablePolicy>;

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
template <typename StatusType, typename RetryablePolicy>
class LimitedTimeRetryPolicy : public RetryPolicy<StatusType, RetryablePolicy> {
 public:
  using BaseType = RetryPolicy<StatusType, RetryablePolicy>;

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

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_RETRY_POLICY_H_
