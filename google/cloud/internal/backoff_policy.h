// Copyright 2018 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_BACKOFF_POLICY_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_BACKOFF_POLICY_H

#include "google/cloud/internal/random.h"
#include "google/cloud/internal/throw_delegate.h"
#include "google/cloud/version.h"
#include "absl/types/optional.h"
#include <chrono>
#include <memory>

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {
/**
 * Define the interface for backoff policies.
 *
 * The client libraries need to hide partial and temporary failures from the
 * application. Exponential backoff is generally considered a best practice when
 * retrying operations. However, the details of how exponential backoff is
 * implemented and tuned varies widely. We need to give the users enough
 * flexibility, and also provide sensible default implementations.
 *
 * The client library receives an object of this type, and clones a new instance
 * for each operation. That is, the application provides the library with a
 * [Prototype](https://en.wikipedia.org/wiki/Prototype_pattern) of the policy
 * that will be applied to each operation.
 *
 * [Truncated Exponential
 * Backoff](https://cloud.google.com/storage/docs/exponential-backoff) in the
 * Google Cloud Storage documentation.
 *
 */
class BackoffPolicy {
 public:
  virtual ~BackoffPolicy() = default;

  /**
   * Return a new copy of this object.
   *
   * Typically implemented as
   * @code
   *   return std::unique_ptr<BackoffPolicy>(new Foo(*this));
   * @endcode
   */
  virtual std::unique_ptr<BackoffPolicy> clone() const = 0;

  /**
   * Handle an operation completion.
   *
   * This function is typically called when an operation has failed (if it had
   * succeeded there is no reason to retry and backoff). The decision to retry
   * the operation is handled by other policies. This separates the concerns
   * of how much to retry vs. how much delay to put between retries.
   *
   * @return the delay to wait before the next retry attempt.
   */
  virtual std::chrono::milliseconds OnCompletion() = 0;
};

/**
 * Implements a truncated exponential backoff with randomization policy and a
 * minimum delay.
 *
 * This policy implements the truncated exponential backoff policy for
 * retrying operations. After a request fails, and subject to a separate
 * retry policy, the client library will wait for an initial delay after
 * the specified minimum delay before trying again. If the second attempt fails
 * the delay time is increased, using a scaling factor. The delay time begins at
 * the minimum delay. The delay time growth stops at a
 * maximum delay time. The policy also randomizes the delay each time, to
 * avoid the [thundering herd
 * problem](https://en.wikipedia.org/wiki/Thundering_herd_problem).
 */
class ExponentialBackoffPolicy : public BackoffPolicy {
 public:
  /**
   * Constructor for an exponential backoff policy.
   *
   * Define the initial delay, maximum delay, and scaling factor for an instance
   * of the policy. While the constructor accepts `std::chrono::duration`
   * objects at any resolution, the data is kept internally in microseconds.
   * Sub-microsecond delays seem unnecessarily precise for this application.
   *
   * @code
   * using namespace std::chrono_literals; // C++14
   * auto r1 = ExponentialBackoffPolicy<S,T>(10ms, 500ms, 1.618);
   * auto r2 = ExponentialBackoffPolicy<S,T>(10min, 10min + 2s, 1.002);
   * @endcode
   *
   * @param initial_delay the longest possible delay after the first
   *     (unsuccessful) operation and the minimum value for the delay between
   *     operations.
   * @param maximum_delay the maximum value for the delay between operations.
   * @param scaling how fast does the delay increase between iterations.
   *
   * @tparam Rep1 a placeholder to match the Rep tparam for
   *     @p initial_delay's type. The semantics of this template parameter
   *     are documented in `std::chrono::duration<>` (in brief, the underlying
   *     arithmetic type used to store the number of ticks). For our purposes,
   *     it is simply a formal parameter.
   * @tparam Period1 a placeholder to match the Period tparam for
   *     @p initial_delay's type. The semantics of this template parameter
   *     are documented in `std::chrono::duration<>` (in brief, the underlying
   *     arithmetic type used to store the number of ticks). For our purposes,
   *     it is simply a formal parameter.
   * @tparam Rep2 similar formal parameter for the type of @p maximum_delay.
   * @tparam Period2 similar formal parameter for the type of @p maximum_delay.
   *
   * @see
   * [std::chrono::duration<>](http://en.cppreference.com/w/cpp/chrono/duration)
   *     for more details.
   */
  template <typename Rep1, typename Period1, typename Rep2, typename Period2>
  ExponentialBackoffPolicy(std::chrono::duration<Rep1, Period1> initial_delay,
                           std::chrono::duration<Rep2, Period2> maximum_delay,
                           double scaling)
      : ExponentialBackoffPolicy(initial_delay, initial_delay * scaling,
                                 maximum_delay, scaling, scaling) {}

  /**
   * Constructor for an exponential backoff policy that supports full jitter.
   *
   * Define a policy with a customizable delay intervals and scaling factors.
   * While the constructor accepts `std::chrono::duration` objects at any
   * resolution, the data is kept internally in microseconds. Sub-microsecond
   * delays seem unnecessarily precise for this application.
   *
   * @code
   * using namespace std::chrono_literals; // C++14
   * auto r1 = ExponentialBackoffPolicy<S,T>(0ms, 10ms, 500ms, 1.0, 1.618);
   * @endcode
   *
   * @param minimum_delay the minimum value for the delay between operations.
   * @param initial_delay_upper_bound the longest possible delay to wait after
   *     the first (unsuccessful) operation.
   * @param maximum_delay the maximum value for the delay between operations.
   * @param scaling_lower_bound how fast the delay's lower bound increases
   *     between iterations.
   * @param scaling_upper_bound how fast the delay's upper bound increases
   *     between iterations.
   *
   * @tparam Rep1 a placeholder to match the Rep tparam for
   *     @p minimum_delay's type. The semantics of this template
   *     parameter are documented in `std::chrono::duration<>` (in brief, the
   *     underlying arithmetic type used to store the number of ticks). For our
   *     purposes, it is simply a formal parameter.
   * @tparam Period1 a placeholder to match the Period tparam for
   *     @p minimum_delay's type. The semantics of this template
   *     parameter are documented in `std::chrono::duration<>` (in brief, the
   *     underlying arithmetic type used to store the number of ticks). For our
   *     purposes, it is simply a formal parameter.
   * @tparam Rep2 similar formal parameter for the type of
   *    @p initial_delay_upper_bound.
   * @tparam Period2 similar formal parameter for the type of
   *    @p initial_delay_upper_bound.
   * @tparam Rep3 similar formal parameter for the type of @p maximum_delay.
   * @tparam Period3 similar formal parameter for the type of @p maximum_delay.
   *
   * @see
   * [std::chrono::duration<>](http://en.cppreference.com/w/cpp/chrono/duration)
   *     for more details.
   */
  template <typename Rep1, typename Period1, typename Rep2, typename Period2,
            typename Rep3, typename Period3>
  ExponentialBackoffPolicy(
      std::chrono::duration<Rep1, Period1> minimum_delay,
      std::chrono::duration<Rep2, Period2> initial_delay_upper_bound,
      std::chrono::duration<Rep3, Period3> maximum_delay,
      double scaling_lower_bound, double scaling_upper_bound)
      : minimum_delay_(minimum_delay),
        initial_delay_upper_bound_(initial_delay_upper_bound),
        maximum_delay_(maximum_delay),
        scaling_lower_bound_(scaling_lower_bound),
        scaling_upper_bound_(scaling_upper_bound),
        current_delay_start_(minimum_delay_),
        current_delay_end_(initial_delay_upper_bound_) {
    if (initial_delay_upper_bound_ < minimum_delay_) {
      google::cloud::internal::ThrowInvalidArgument(
          "initial delay upper bound must be >= minimum delay");
    }
    if (scaling_lower_bound_ < 1.0) {
      google::cloud::internal::ThrowInvalidArgument(
          "scaling lower bound factor must be >= 1.0");
    }
    if (scaling_upper_bound_ <= 1.0) {
      google::cloud::internal::ThrowInvalidArgument(
          "scaling upper bound factor must be > 1.0");
    }
    if (scaling_lower_bound > scaling_upper_bound) {
      google::cloud::internal::ThrowInvalidArgument(
          "scaling lower bound must be <= scaling upper bound");
    }
  }

  // Do not copy the PRNG, we get two benefits:
  //  - This works around a bug triggered by MSVC + `absl::optional` (we do not
  //    know specifically which one is at fault)
  //  - We want uncorrelated data streams for each copy anyway.
  ExponentialBackoffPolicy(ExponentialBackoffPolicy const& rhs) noexcept
      : minimum_delay_(rhs.minimum_delay_),
        initial_delay_upper_bound_(rhs.initial_delay_upper_bound_),
        maximum_delay_(rhs.maximum_delay_),
        scaling_lower_bound_(rhs.scaling_lower_bound_),
        scaling_upper_bound_(rhs.scaling_upper_bound_),
        current_delay_start_(rhs.current_delay_start_),
        current_delay_end_(rhs.current_delay_end_) {}

  std::unique_ptr<BackoffPolicy> clone() const override;
  std::chrono::milliseconds OnCompletion() override;

 private:
  using DoubleMicroseconds = std::chrono::duration<double, std::micro>;
  DoubleMicroseconds minimum_delay_;
  DoubleMicroseconds initial_delay_upper_bound_;
  DoubleMicroseconds maximum_delay_;
  double scaling_lower_bound_;
  double scaling_upper_bound_;
  DoubleMicroseconds current_delay_start_;
  DoubleMicroseconds current_delay_end_;
  absl::optional<DefaultPRNG> generator_;
};

}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_BACKOFF_POLICY_H
