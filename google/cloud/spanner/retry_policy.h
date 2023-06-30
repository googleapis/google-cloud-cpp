// Copyright 2019 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_RETRY_POLICY_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_RETRY_POLICY_H

#include "google/cloud/spanner/internal/status_utils.h"
#include "google/cloud/spanner/version.h"
#include "google/cloud/internal/retry_policy_impl.h"
#include "google/cloud/status.h"
#include "absl/strings/match.h"

namespace google {
namespace cloud {
namespace spanner_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

/// Define the gRPC status code semantics for retrying requests.
struct SafeGrpcRetry {
  static inline bool IsOk(google::cloud::Status const& status) {
    return status.ok();
  }
  static inline bool IsTransientFailure(google::cloud::Status const& status) {
    return status.code() == StatusCode::kUnavailable ||
           status.code() == StatusCode::kResourceExhausted ||
           internal::IsTransientInternalError(status);
  }
  static inline bool IsPermanentFailure(google::cloud::Status const& status) {
    return !IsOk(status) && !IsTransientFailure(status);
  }
};

/// Define the gRPC status code semantics for rerunning transactions.
struct SafeTransactionRerun {
  static inline bool IsOk(google::cloud::Status const& status) {
    return status.ok();
  }
  static inline bool IsTransientFailure(google::cloud::Status const& status) {
    return status.code() == StatusCode::kAborted || IsSessionNotFound(status);
  }
  static inline bool IsPermanentFailure(google::cloud::Status const& status) {
    return !IsOk(status) && !IsTransientFailure(status);
  }
};
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace spanner_internal

namespace spanner {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

/// The base class for the Spanner library retry policies.
class RetryPolicy : public google::cloud::RetryPolicy {
 public:
  /// Create a new instance with the initial configuration, as-if no errors had
  /// been processed.
  virtual std::unique_ptr<RetryPolicy> clone() const = 0;
};

/// A retry policy that stops the retry loop after some prescribed time.
class LimitedTimeRetryPolicy : public RetryPolicy {
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
  explicit LimitedTimeRetryPolicy(
      std::chrono::duration<DurationRep, DurationPeriod> maximum_duration)
      : impl_(maximum_duration) {}

  std::unique_ptr<RetryPolicy> clone() const override {
    return std::make_unique<LimitedTimeRetryPolicy>(impl_.maximum_duration());
  }
  bool OnFailure(Status const& s) override { return impl_.OnFailure(s); }
  bool IsExhausted() const override { return impl_.IsExhausted(); }
  bool IsPermanentFailure(Status const& s) const override {
    return impl_.IsPermanentFailure(s);
  }

  // Not very useful, but needed for backwards compatibility.
  using BaseType = RetryPolicy;

 private:
  google::cloud::internal::LimitedTimeRetryPolicy<
      spanner_internal::SafeGrpcRetry>
      impl_;
};

/// A retry policy that limits the number of times a request can fail.
class LimitedErrorCountRetryPolicy : public RetryPolicy {
 public:
  /// Constructor given the maximum number of failures.
  explicit LimitedErrorCountRetryPolicy(int maximum_failures)
      : impl_(maximum_failures) {}

  std::unique_ptr<RetryPolicy> clone() const override {
    return std::make_unique<LimitedErrorCountRetryPolicy>(
        impl_.maximum_failures());
  }
  bool OnFailure(Status const& s) override { return impl_.OnFailure(s); }
  bool IsExhausted() const override { return impl_.IsExhausted(); }
  bool IsPermanentFailure(Status const& s) const override {
    return impl_.IsPermanentFailure(s);
  }

  // Not very useful, but needed for backwards compatibility.
  /// The maximum number of failures tolerated by this policy.
  int maximum_failures() const { return impl_.maximum_failures(); }

 private:
  google::cloud::internal::LimitedErrorCountRetryPolicy<
      spanner_internal::SafeGrpcRetry>
      impl_;
};

/// The base class for the Spanner library transaction rerun policies.
class TransactionRerunPolicy : public google::cloud::RetryPolicy {
 public:
  /// Create a new instance with the initial configuration, as-if no errors had
  /// been processed.
  virtual std::unique_ptr<TransactionRerunPolicy> clone() const = 0;
};

/// A retry policy that stops the retry loop after some prescribed time.
class LimitedTimeTransactionRerunPolicy : public TransactionRerunPolicy {
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
  explicit LimitedTimeTransactionRerunPolicy(
      std::chrono::duration<DurationRep, DurationPeriod> maximum_duration)
      : impl_(maximum_duration) {}

  std::unique_ptr<TransactionRerunPolicy> clone() const override {
    return std::make_unique<LimitedTimeTransactionRerunPolicy>(
        impl_.maximum_duration());
  }
  bool OnFailure(Status const& s) override { return impl_.OnFailure(s); }
  bool IsExhausted() const override { return impl_.IsExhausted(); }
  bool IsPermanentFailure(Status const& s) const override {
    return impl_.IsPermanentFailure(s);
  }

  // Not very useful, but needed for backwards compatibility.
  using BaseType = RetryPolicy;

 private:
  google::cloud::internal::LimitedTimeRetryPolicy<
      spanner_internal::SafeTransactionRerun>
      impl_;
};

/// A retry policy that limits the number of times a request can fail.
class LimitedErrorCountTransactionRerunPolicy : public TransactionRerunPolicy {
 public:
  /// Constructor given the maximum number of failures.
  explicit LimitedErrorCountTransactionRerunPolicy(int maximum_failures)
      : impl_(maximum_failures) {}

  std::unique_ptr<TransactionRerunPolicy> clone() const override {
    return std::make_unique<LimitedErrorCountTransactionRerunPolicy>(
        impl_.maximum_failures());
  }
  bool OnFailure(Status const& s) override { return impl_.OnFailure(s); }
  bool IsExhausted() const override { return impl_.IsExhausted(); }
  bool IsPermanentFailure(Status const& s) const override {
    return impl_.IsPermanentFailure(s);
  }

  // Not very useful, but needed for backwards compatibility.
  /// The maximum number of failures tolerated by this policy.
  int maximum_failures() const { return impl_.maximum_failures(); }

 private:
  google::cloud::internal::LimitedErrorCountRetryPolicy<
      spanner_internal::SafeTransactionRerun>
      impl_;
};

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace spanner
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_RETRY_POLICY_H
