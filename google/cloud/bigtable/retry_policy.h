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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_RETRY_POLICY_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_RETRY_POLICY_H

#include "google/cloud/bigtable/internal/retry_traits.h"
#include "google/cloud/internal/retry_policy_impl.h"
#include "google/cloud/status.h"

namespace google {
namespace cloud {
namespace bigtable {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

/// The retry policy for `bigtable::DataConnection`.
class DataRetryPolicy : public google::cloud::RetryPolicy {};

/**
 * A retry policy for `bigtable::DataConnection` based on counting errors.
 *
 * This policy stops retrying if:
 * - An RPC returns a non-transient error.
 * - More than a prescribed number of transient failures is detected.
 *
 * In this class the following status codes are treated as transient errors:
 * - [`kAborted`](@ref google::cloud::StatusCode)
 * - [`kUnavailable`](@ref google::cloud::StatusCode)
 * - [`kInternal`](@ref google::cloud::StatusCode) if the error message
 *   indicates this was caused by a connection reset.
 */
class DataLimitedErrorCountRetryPolicy : public DataRetryPolicy {
 public:
  /**
   * Create an instance that tolerates up to @p maximum_failures transient
   * errors.
   *
   * @note Disable the retry loop by providing an instance of this policy with
   *     @p maximum_failures == 0.
   */
  explicit DataLimitedErrorCountRetryPolicy(int maximum_failures)
      : impl_(maximum_failures) {}

  DataLimitedErrorCountRetryPolicy(
      DataLimitedErrorCountRetryPolicy&& rhs) noexcept
      : DataLimitedErrorCountRetryPolicy(rhs.maximum_failures()) {}
  DataLimitedErrorCountRetryPolicy(
      DataLimitedErrorCountRetryPolicy const& rhs) noexcept
      : DataLimitedErrorCountRetryPolicy(rhs.maximum_failures()) {}

  int maximum_failures() const { return impl_.maximum_failures(); }

  bool OnFailure(Status const& s) override { return impl_.OnFailure(s); }
  bool IsExhausted() const override { return impl_.IsExhausted(); }
  bool IsPermanentFailure(Status const& s) const override {
    return impl_.IsPermanentFailure(s);
  }
  std::unique_ptr<google::cloud::RetryPolicy> clone() const override {
    return std::make_unique<DataLimitedErrorCountRetryPolicy>(
        impl_.maximum_failures());
  }

  // This is provided only for backwards compatibility.
  using BaseType = RetryPolicy;

 private:
  google::cloud::internal::LimitedErrorCountRetryPolicy<
      bigtable_internal::SafeGrpcRetry>
      impl_;
};

/**
 * A retry policy for `bigtable::DataConnection` based on elapsed time.
 *
 * This policy stops retrying if:
 * - An RPC returns a non-transient error.
 * - The elapsed time in the retry loop exceeds a prescribed duration.
 *
 * In this class the following status codes are treated as transient errors:
 * - [`kAborted`](@ref google::cloud::StatusCode)
 * - [`kUnavailable`](@ref google::cloud::StatusCode)
 * - [`kInternal`](@ref google::cloud::StatusCode) if the error message
 *   indicates this was caused by a connection reset.
 */
class DataLimitedTimeRetryPolicy : public DataRetryPolicy {
 public:
  /**
   * Constructor given a `std::chrono::duration<>` object.
   *
   * @tparam DurationRep a placeholder to match the `Rep` tparam for
   *     @p maximum_duration's type. The semantics of this template parameter
   *     are documented in `std::chrono::duration<>`. In brief, the underlying
   *     arithmetic type used to store the number of ticks. For our purposes it
   *     is simply a formal parameter.
   * @tparam DurationPeriod a placeholder to match the `Period` tparam for
   *     @p maximum_duration's type. The semantics of this template parameter
   *     are documented in `std::chrono::duration<>`. In brief, the length of
   *     the tick in seconds, expressed as a `std::ratio<>`. For our purposes it
   *     is simply a formal parameter.
   * @param maximum_duration the maximum time allowed before the policy expires,
   *     while the application can express this time in any units they desire,
   *     the class truncates to milliseconds.
   *
   * @see https://en.cppreference.com/w/cpp/chrono/duration for more details
   *     about `std::chrono::duration`.
   */
  template <typename DurationRep, typename DurationPeriod>
  explicit DataLimitedTimeRetryPolicy(
      std::chrono::duration<DurationRep, DurationPeriod> maximum_duration)
      : impl_(maximum_duration) {}

  DataLimitedTimeRetryPolicy(DataLimitedTimeRetryPolicy&& rhs) noexcept
      : DataLimitedTimeRetryPolicy(rhs.maximum_duration()) {}
  DataLimitedTimeRetryPolicy(DataLimitedTimeRetryPolicy const& rhs) noexcept
      : DataLimitedTimeRetryPolicy(rhs.maximum_duration()) {}

  std::chrono::milliseconds maximum_duration() const {
    return impl_.maximum_duration();
  }

  bool OnFailure(Status const& s) override { return impl_.OnFailure(s); }
  bool IsExhausted() const override { return impl_.IsExhausted(); }
  bool IsPermanentFailure(Status const& s) const override {
    return impl_.IsPermanentFailure(s);
  }
  std::unique_ptr<google::cloud::RetryPolicy> clone() const override {
    return std::make_unique<DataLimitedTimeRetryPolicy>(
        impl_.maximum_duration());
  }

  // This is provided only for backwards compatibility.
  using BaseType = RetryPolicy;

 private:
  google::cloud::internal::LimitedTimeRetryPolicy<
      bigtable_internal::SafeGrpcRetry>
      impl_;
};

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_RETRY_POLICY_H
