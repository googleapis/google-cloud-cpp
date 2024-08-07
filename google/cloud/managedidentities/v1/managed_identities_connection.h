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

// Generated by the Codegen C++ plugin.
// If you make any local changes, they will be lost.
// source: google/cloud/managedidentities/v1/managed_identities_service.proto

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_MANAGEDIDENTITIES_V1_MANAGED_IDENTITIES_CONNECTION_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_MANAGEDIDENTITIES_V1_MANAGED_IDENTITIES_CONNECTION_H

#include "google/cloud/managedidentities/v1/internal/managed_identities_retry_traits.h"
#include "google/cloud/managedidentities/v1/managed_identities_connection_idempotency_policy.h"
#include "google/cloud/backoff_policy.h"
#include "google/cloud/future.h"
#include "google/cloud/internal/retry_policy_impl.h"
#include "google/cloud/no_await_tag.h"
#include "google/cloud/options.h"
#include "google/cloud/polling_policy.h"
#include "google/cloud/status_or.h"
#include "google/cloud/stream_range.h"
#include "google/cloud/version.h"
#include <google/cloud/managedidentities/v1/managed_identities_service.pb.h>
#include <google/longrunning/operations.grpc.pb.h>
#include <memory>

namespace google {
namespace cloud {
namespace managedidentities_v1 {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

/// The retry policy for `ManagedIdentitiesServiceConnection`.
class ManagedIdentitiesServiceRetryPolicy
    : public ::google::cloud::RetryPolicy {
 public:
  /// Creates a new instance of the policy, reset to the initial state.
  virtual std::unique_ptr<ManagedIdentitiesServiceRetryPolicy> clone()
      const = 0;
};

/**
 * A retry policy for `ManagedIdentitiesServiceConnection` based on counting
 * errors.
 *
 * This policy stops retrying if:
 * - An RPC returns a non-transient error.
 * - More than a prescribed number of transient failures is detected.
 *
 * In this class the following status codes are treated as transient errors:
 * - [`kUnavailable`](@ref google::cloud::StatusCode)
 */
class ManagedIdentitiesServiceLimitedErrorCountRetryPolicy
    : public ManagedIdentitiesServiceRetryPolicy {
 public:
  /**
   * Create an instance that tolerates up to @p maximum_failures transient
   * errors.
   *
   * @note Disable the retry loop by providing an instance of this policy with
   *     @p maximum_failures == 0.
   */
  explicit ManagedIdentitiesServiceLimitedErrorCountRetryPolicy(
      int maximum_failures)
      : impl_(maximum_failures) {}

  ManagedIdentitiesServiceLimitedErrorCountRetryPolicy(
      ManagedIdentitiesServiceLimitedErrorCountRetryPolicy&& rhs) noexcept
      : ManagedIdentitiesServiceLimitedErrorCountRetryPolicy(
            rhs.maximum_failures()) {}
  ManagedIdentitiesServiceLimitedErrorCountRetryPolicy(
      ManagedIdentitiesServiceLimitedErrorCountRetryPolicy const& rhs) noexcept
      : ManagedIdentitiesServiceLimitedErrorCountRetryPolicy(
            rhs.maximum_failures()) {}

  int maximum_failures() const { return impl_.maximum_failures(); }

  bool OnFailure(Status const& status) override {
    return impl_.OnFailure(status);
  }
  bool IsExhausted() const override { return impl_.IsExhausted(); }
  bool IsPermanentFailure(Status const& status) const override {
    return impl_.IsPermanentFailure(status);
  }
  std::unique_ptr<ManagedIdentitiesServiceRetryPolicy> clone() const override {
    return std::make_unique<
        ManagedIdentitiesServiceLimitedErrorCountRetryPolicy>(
        maximum_failures());
  }

  // This is provided only for backwards compatibility.
  using BaseType = ManagedIdentitiesServiceRetryPolicy;

 private:
  google::cloud::internal::LimitedErrorCountRetryPolicy<
      managedidentities_v1_internal::ManagedIdentitiesServiceRetryTraits>
      impl_;
};

/**
 * A retry policy for `ManagedIdentitiesServiceConnection` based on elapsed
 * time.
 *
 * This policy stops retrying if:
 * - An RPC returns a non-transient error.
 * - The elapsed time in the retry loop exceeds a prescribed duration.
 *
 * In this class the following status codes are treated as transient errors:
 * - [`kUnavailable`](@ref google::cloud::StatusCode)
 */
class ManagedIdentitiesServiceLimitedTimeRetryPolicy
    : public ManagedIdentitiesServiceRetryPolicy {
 public:
  /**
   * Constructor given a `std::chrono::duration<>` object.
   *
   * @tparam DurationRep a placeholder to match the `Rep` tparam for @p
   *     duration's type. The semantics of this template parameter are
   *     documented in `std::chrono::duration<>`. In brief, the underlying
   *     arithmetic type used to store the number of ticks. For our purposes it
   *     is simply a formal parameter.
   * @tparam DurationPeriod a placeholder to match the `Period` tparam for @p
   *     duration's type. The semantics of this template parameter are
   *     documented in `std::chrono::duration<>`. In brief, the length of the
   *     tick in seconds, expressed as a `std::ratio<>`. For our purposes it is
   *     simply a formal parameter.
   * @param maximum_duration the maximum time allowed before the policy expires.
   *     While the application can express this time in any units they desire,
   *     the class truncates to milliseconds.
   *
   * @see https://en.cppreference.com/w/cpp/chrono/duration for more information
   *     about `std::chrono::duration`.
   */
  template <typename DurationRep, typename DurationPeriod>
  explicit ManagedIdentitiesServiceLimitedTimeRetryPolicy(
      std::chrono::duration<DurationRep, DurationPeriod> maximum_duration)
      : impl_(maximum_duration) {}

  ManagedIdentitiesServiceLimitedTimeRetryPolicy(
      ManagedIdentitiesServiceLimitedTimeRetryPolicy&& rhs) noexcept
      : ManagedIdentitiesServiceLimitedTimeRetryPolicy(rhs.maximum_duration()) {
  }
  ManagedIdentitiesServiceLimitedTimeRetryPolicy(
      ManagedIdentitiesServiceLimitedTimeRetryPolicy const& rhs) noexcept
      : ManagedIdentitiesServiceLimitedTimeRetryPolicy(rhs.maximum_duration()) {
  }

  std::chrono::milliseconds maximum_duration() const {
    return impl_.maximum_duration();
  }

  bool OnFailure(Status const& status) override {
    return impl_.OnFailure(status);
  }
  bool IsExhausted() const override { return impl_.IsExhausted(); }
  bool IsPermanentFailure(Status const& status) const override {
    return impl_.IsPermanentFailure(status);
  }
  std::unique_ptr<ManagedIdentitiesServiceRetryPolicy> clone() const override {
    return std::make_unique<ManagedIdentitiesServiceLimitedTimeRetryPolicy>(
        maximum_duration());
  }

  // This is provided only for backwards compatibility.
  using BaseType = ManagedIdentitiesServiceRetryPolicy;

 private:
  google::cloud::internal::LimitedTimeRetryPolicy<
      managedidentities_v1_internal::ManagedIdentitiesServiceRetryTraits>
      impl_;
};

/**
 * The `ManagedIdentitiesServiceConnection` object for
 * `ManagedIdentitiesServiceClient`.
 *
 * This interface defines virtual methods for each of the user-facing overload
 * sets in `ManagedIdentitiesServiceClient`. This allows users to inject custom
 * behavior (e.g., with a Google Mock object) when writing tests that use
 * objects of type `ManagedIdentitiesServiceClient`.
 *
 * To create a concrete instance, see
 * `MakeManagedIdentitiesServiceConnection()`.
 *
 * For mocking, see
 * `managedidentities_v1_mocks::MockManagedIdentitiesServiceConnection`.
 */
class ManagedIdentitiesServiceConnection {
 public:
  virtual ~ManagedIdentitiesServiceConnection() = 0;

  virtual Options options() { return Options{}; }

  virtual future<StatusOr<google::cloud::managedidentities::v1::Domain>>
  CreateMicrosoftAdDomain(google::cloud::managedidentities::v1::
                              CreateMicrosoftAdDomainRequest const& request);

  virtual StatusOr<google::longrunning::Operation> CreateMicrosoftAdDomain(
      NoAwaitTag, google::cloud::managedidentities::v1::
                      CreateMicrosoftAdDomainRequest const& request);

  virtual future<StatusOr<google::cloud::managedidentities::v1::Domain>>
  CreateMicrosoftAdDomain(google::longrunning::Operation const& operation);

  virtual StatusOr<
      google::cloud::managedidentities::v1::ResetAdminPasswordResponse>
  ResetAdminPassword(
      google::cloud::managedidentities::v1::ResetAdminPasswordRequest const&
          request);

  virtual StreamRange<google::cloud::managedidentities::v1::Domain> ListDomains(
      google::cloud::managedidentities::v1::ListDomainsRequest request);

  virtual StatusOr<google::cloud::managedidentities::v1::Domain> GetDomain(
      google::cloud::managedidentities::v1::GetDomainRequest const& request);

  virtual future<StatusOr<google::cloud::managedidentities::v1::Domain>>
  UpdateDomain(
      google::cloud::managedidentities::v1::UpdateDomainRequest const& request);

  virtual StatusOr<google::longrunning::Operation> UpdateDomain(
      NoAwaitTag,
      google::cloud::managedidentities::v1::UpdateDomainRequest const& request);

  virtual future<StatusOr<google::cloud::managedidentities::v1::Domain>>
  UpdateDomain(google::longrunning::Operation const& operation);

  virtual future<StatusOr<google::cloud::managedidentities::v1::OpMetadata>>
  DeleteDomain(
      google::cloud::managedidentities::v1::DeleteDomainRequest const& request);

  virtual StatusOr<google::longrunning::Operation> DeleteDomain(
      NoAwaitTag,
      google::cloud::managedidentities::v1::DeleteDomainRequest const& request);

  virtual future<StatusOr<google::cloud::managedidentities::v1::OpMetadata>>
  DeleteDomain(google::longrunning::Operation const& operation);

  virtual future<StatusOr<google::cloud::managedidentities::v1::Domain>>
  AttachTrust(
      google::cloud::managedidentities::v1::AttachTrustRequest const& request);

  virtual StatusOr<google::longrunning::Operation> AttachTrust(
      NoAwaitTag,
      google::cloud::managedidentities::v1::AttachTrustRequest const& request);

  virtual future<StatusOr<google::cloud::managedidentities::v1::Domain>>
  AttachTrust(google::longrunning::Operation const& operation);

  virtual future<StatusOr<google::cloud::managedidentities::v1::Domain>>
  ReconfigureTrust(
      google::cloud::managedidentities::v1::ReconfigureTrustRequest const&
          request);

  virtual StatusOr<google::longrunning::Operation> ReconfigureTrust(
      NoAwaitTag,
      google::cloud::managedidentities::v1::ReconfigureTrustRequest const&
          request);

  virtual future<StatusOr<google::cloud::managedidentities::v1::Domain>>
  ReconfigureTrust(google::longrunning::Operation const& operation);

  virtual future<StatusOr<google::cloud::managedidentities::v1::Domain>>
  DetachTrust(
      google::cloud::managedidentities::v1::DetachTrustRequest const& request);

  virtual StatusOr<google::longrunning::Operation> DetachTrust(
      NoAwaitTag,
      google::cloud::managedidentities::v1::DetachTrustRequest const& request);

  virtual future<StatusOr<google::cloud::managedidentities::v1::Domain>>
  DetachTrust(google::longrunning::Operation const& operation);

  virtual future<StatusOr<google::cloud::managedidentities::v1::Domain>>
  ValidateTrust(
      google::cloud::managedidentities::v1::ValidateTrustRequest const&
          request);

  virtual StatusOr<google::longrunning::Operation> ValidateTrust(
      NoAwaitTag,
      google::cloud::managedidentities::v1::ValidateTrustRequest const&
          request);

  virtual future<StatusOr<google::cloud::managedidentities::v1::Domain>>
  ValidateTrust(google::longrunning::Operation const& operation);
};

/**
 * A factory function to construct an object of type
 * `ManagedIdentitiesServiceConnection`.
 *
 * The returned connection object should not be used directly; instead it
 * should be passed as an argument to the constructor of
 * ManagedIdentitiesServiceClient.
 *
 * The optional @p options argument may be used to configure aspects of the
 * returned `ManagedIdentitiesServiceConnection`. Expected options are any of
 * the types in the following option lists:
 *
 * - `google::cloud::CommonOptionList`
 * - `google::cloud::GrpcOptionList`
 * - `google::cloud::UnifiedCredentialsOptionList`
 * -
 * `google::cloud::managedidentities_v1::ManagedIdentitiesServicePolicyOptionList`
 *
 * @note Unexpected options will be ignored. To log unexpected options instead,
 *     set `GOOGLE_CLOUD_CPP_ENABLE_CLOG=yes` in the environment.
 *
 * @param options (optional) Configure the `ManagedIdentitiesServiceConnection`
 * created by this function.
 */
std::shared_ptr<ManagedIdentitiesServiceConnection>
MakeManagedIdentitiesServiceConnection(Options options = {});

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace managedidentities_v1
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_MANAGEDIDENTITIES_V1_MANAGED_IDENTITIES_CONNECTION_H
