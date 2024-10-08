// Copyright 2024 Google LLC
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
// source: google/cloud/privilegedaccessmanager/v1/privilegedaccessmanager.proto

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PRIVILEGEDACCESSMANAGER_V1_PRIVILEGED_ACCESS_MANAGER_CONNECTION_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PRIVILEGEDACCESSMANAGER_V1_PRIVILEGED_ACCESS_MANAGER_CONNECTION_H

#include "google/cloud/privilegedaccessmanager/v1/internal/privileged_access_manager_retry_traits.h"
#include "google/cloud/privilegedaccessmanager/v1/privileged_access_manager_connection_idempotency_policy.h"
#include "google/cloud/backoff_policy.h"
#include "google/cloud/future.h"
#include "google/cloud/internal/retry_policy_impl.h"
#include "google/cloud/no_await_tag.h"
#include "google/cloud/options.h"
#include "google/cloud/polling_policy.h"
#include "google/cloud/status_or.h"
#include "google/cloud/stream_range.h"
#include "google/cloud/version.h"
#include <google/cloud/privilegedaccessmanager/v1/privilegedaccessmanager.pb.h>
#include <google/longrunning/operations.grpc.pb.h>
#include <memory>

namespace google {
namespace cloud {
namespace privilegedaccessmanager_v1 {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

/// The retry policy for `PrivilegedAccessManagerConnection`.
class PrivilegedAccessManagerRetryPolicy : public ::google::cloud::RetryPolicy {
 public:
  /// Creates a new instance of the policy, reset to the initial state.
  virtual std::unique_ptr<PrivilegedAccessManagerRetryPolicy> clone() const = 0;
};

/**
 * A retry policy for `PrivilegedAccessManagerConnection` based on counting
 * errors.
 *
 * This policy stops retrying if:
 * - An RPC returns a non-transient error.
 * - More than a prescribed number of transient failures is detected.
 *
 * In this class the following status codes are treated as transient errors:
 * - [`kUnavailable`](@ref google::cloud::StatusCode)
 */
class PrivilegedAccessManagerLimitedErrorCountRetryPolicy
    : public PrivilegedAccessManagerRetryPolicy {
 public:
  /**
   * Create an instance that tolerates up to @p maximum_failures transient
   * errors.
   *
   * @note Disable the retry loop by providing an instance of this policy with
   *     @p maximum_failures == 0.
   */
  explicit PrivilegedAccessManagerLimitedErrorCountRetryPolicy(
      int maximum_failures)
      : impl_(maximum_failures) {}

  PrivilegedAccessManagerLimitedErrorCountRetryPolicy(
      PrivilegedAccessManagerLimitedErrorCountRetryPolicy&& rhs) noexcept
      : PrivilegedAccessManagerLimitedErrorCountRetryPolicy(
            rhs.maximum_failures()) {}
  PrivilegedAccessManagerLimitedErrorCountRetryPolicy(
      PrivilegedAccessManagerLimitedErrorCountRetryPolicy const& rhs) noexcept
      : PrivilegedAccessManagerLimitedErrorCountRetryPolicy(
            rhs.maximum_failures()) {}

  int maximum_failures() const { return impl_.maximum_failures(); }

  bool OnFailure(Status const& status) override {
    return impl_.OnFailure(status);
  }
  bool IsExhausted() const override { return impl_.IsExhausted(); }
  bool IsPermanentFailure(Status const& status) const override {
    return impl_.IsPermanentFailure(status);
  }
  std::unique_ptr<PrivilegedAccessManagerRetryPolicy> clone() const override {
    return std::make_unique<
        PrivilegedAccessManagerLimitedErrorCountRetryPolicy>(
        maximum_failures());
  }

  // This is provided only for backwards compatibility.
  using BaseType = PrivilegedAccessManagerRetryPolicy;

 private:
  google::cloud::internal::LimitedErrorCountRetryPolicy<
      privilegedaccessmanager_v1_internal::PrivilegedAccessManagerRetryTraits>
      impl_;
};

/**
 * A retry policy for `PrivilegedAccessManagerConnection` based on elapsed time.
 *
 * This policy stops retrying if:
 * - An RPC returns a non-transient error.
 * - The elapsed time in the retry loop exceeds a prescribed duration.
 *
 * In this class the following status codes are treated as transient errors:
 * - [`kUnavailable`](@ref google::cloud::StatusCode)
 */
class PrivilegedAccessManagerLimitedTimeRetryPolicy
    : public PrivilegedAccessManagerRetryPolicy {
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
  explicit PrivilegedAccessManagerLimitedTimeRetryPolicy(
      std::chrono::duration<DurationRep, DurationPeriod> maximum_duration)
      : impl_(maximum_duration) {}

  PrivilegedAccessManagerLimitedTimeRetryPolicy(
      PrivilegedAccessManagerLimitedTimeRetryPolicy&& rhs) noexcept
      : PrivilegedAccessManagerLimitedTimeRetryPolicy(rhs.maximum_duration()) {}
  PrivilegedAccessManagerLimitedTimeRetryPolicy(
      PrivilegedAccessManagerLimitedTimeRetryPolicy const& rhs) noexcept
      : PrivilegedAccessManagerLimitedTimeRetryPolicy(rhs.maximum_duration()) {}

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
  std::unique_ptr<PrivilegedAccessManagerRetryPolicy> clone() const override {
    return std::make_unique<PrivilegedAccessManagerLimitedTimeRetryPolicy>(
        maximum_duration());
  }

  // This is provided only for backwards compatibility.
  using BaseType = PrivilegedAccessManagerRetryPolicy;

 private:
  google::cloud::internal::LimitedTimeRetryPolicy<
      privilegedaccessmanager_v1_internal::PrivilegedAccessManagerRetryTraits>
      impl_;
};

/**
 * The `PrivilegedAccessManagerConnection` object for
 * `PrivilegedAccessManagerClient`.
 *
 * This interface defines virtual methods for each of the user-facing overload
 * sets in `PrivilegedAccessManagerClient`. This allows users to inject custom
 * behavior (e.g., with a Google Mock object) when writing tests that use
 * objects of type `PrivilegedAccessManagerClient`.
 *
 * To create a concrete instance, see `MakePrivilegedAccessManagerConnection()`.
 *
 * For mocking, see
 * `privilegedaccessmanager_v1_mocks::MockPrivilegedAccessManagerConnection`.
 */
class PrivilegedAccessManagerConnection {
 public:
  virtual ~PrivilegedAccessManagerConnection() = 0;

  virtual Options options() { return Options{}; }

  virtual StatusOr<
      google::cloud::privilegedaccessmanager::v1::CheckOnboardingStatusResponse>
  CheckOnboardingStatus(google::cloud::privilegedaccessmanager::v1::
                            CheckOnboardingStatusRequest const& request);

  virtual StreamRange<google::cloud::privilegedaccessmanager::v1::Entitlement>
  ListEntitlements(
      google::cloud::privilegedaccessmanager::v1::ListEntitlementsRequest
          request);

  virtual StreamRange<google::cloud::privilegedaccessmanager::v1::Entitlement>
  SearchEntitlements(
      google::cloud::privilegedaccessmanager::v1::SearchEntitlementsRequest
          request);

  virtual StatusOr<google::cloud::privilegedaccessmanager::v1::Entitlement>
  GetEntitlement(
      google::cloud::privilegedaccessmanager::v1::GetEntitlementRequest const&
          request);

  virtual future<
      StatusOr<google::cloud::privilegedaccessmanager::v1::Entitlement>>
  CreateEntitlement(google::cloud::privilegedaccessmanager::v1::
                        CreateEntitlementRequest const& request);

  virtual StatusOr<google::longrunning::Operation> CreateEntitlement(
      NoAwaitTag, google::cloud::privilegedaccessmanager::v1::
                      CreateEntitlementRequest const& request);

  virtual future<
      StatusOr<google::cloud::privilegedaccessmanager::v1::Entitlement>>
  CreateEntitlement(google::longrunning::Operation const& operation);

  virtual future<
      StatusOr<google::cloud::privilegedaccessmanager::v1::Entitlement>>
  DeleteEntitlement(google::cloud::privilegedaccessmanager::v1::
                        DeleteEntitlementRequest const& request);

  virtual StatusOr<google::longrunning::Operation> DeleteEntitlement(
      NoAwaitTag, google::cloud::privilegedaccessmanager::v1::
                      DeleteEntitlementRequest const& request);

  virtual future<
      StatusOr<google::cloud::privilegedaccessmanager::v1::Entitlement>>
  DeleteEntitlement(google::longrunning::Operation const& operation);

  virtual future<
      StatusOr<google::cloud::privilegedaccessmanager::v1::Entitlement>>
  UpdateEntitlement(google::cloud::privilegedaccessmanager::v1::
                        UpdateEntitlementRequest const& request);

  virtual StatusOr<google::longrunning::Operation> UpdateEntitlement(
      NoAwaitTag, google::cloud::privilegedaccessmanager::v1::
                      UpdateEntitlementRequest const& request);

  virtual future<
      StatusOr<google::cloud::privilegedaccessmanager::v1::Entitlement>>
  UpdateEntitlement(google::longrunning::Operation const& operation);

  virtual StreamRange<google::cloud::privilegedaccessmanager::v1::Grant>
  ListGrants(
      google::cloud::privilegedaccessmanager::v1::ListGrantsRequest request);

  virtual StreamRange<google::cloud::privilegedaccessmanager::v1::Grant>
  SearchGrants(
      google::cloud::privilegedaccessmanager::v1::SearchGrantsRequest request);

  virtual StatusOr<google::cloud::privilegedaccessmanager::v1::Grant> GetGrant(
      google::cloud::privilegedaccessmanager::v1::GetGrantRequest const&
          request);

  virtual StatusOr<google::cloud::privilegedaccessmanager::v1::Grant>
  CreateGrant(
      google::cloud::privilegedaccessmanager::v1::CreateGrantRequest const&
          request);

  virtual StatusOr<google::cloud::privilegedaccessmanager::v1::Grant>
  ApproveGrant(
      google::cloud::privilegedaccessmanager::v1::ApproveGrantRequest const&
          request);

  virtual StatusOr<google::cloud::privilegedaccessmanager::v1::Grant> DenyGrant(
      google::cloud::privilegedaccessmanager::v1::DenyGrantRequest const&
          request);

  virtual future<StatusOr<google::cloud::privilegedaccessmanager::v1::Grant>>
  RevokeGrant(
      google::cloud::privilegedaccessmanager::v1::RevokeGrantRequest const&
          request);

  virtual StatusOr<google::longrunning::Operation> RevokeGrant(
      NoAwaitTag,
      google::cloud::privilegedaccessmanager::v1::RevokeGrantRequest const&
          request);

  virtual future<StatusOr<google::cloud::privilegedaccessmanager::v1::Grant>>
  RevokeGrant(google::longrunning::Operation const& operation);

  virtual StreamRange<google::cloud::location::Location> ListLocations(
      google::cloud::location::ListLocationsRequest request);

  virtual StatusOr<google::cloud::location::Location> GetLocation(
      google::cloud::location::GetLocationRequest const& request);

  virtual StreamRange<google::longrunning::Operation> ListOperations(
      google::longrunning::ListOperationsRequest request);

  virtual StatusOr<google::longrunning::Operation> GetOperation(
      google::longrunning::GetOperationRequest const& request);

  virtual Status DeleteOperation(
      google::longrunning::DeleteOperationRequest const& request);
};

/**
 * A factory function to construct an object of type
 * `PrivilegedAccessManagerConnection`.
 *
 * The returned connection object should not be used directly; instead it
 * should be passed as an argument to the constructor of
 * PrivilegedAccessManagerClient.
 *
 * The optional @p options argument may be used to configure aspects of the
 * returned `PrivilegedAccessManagerConnection`. Expected options are any of the
 * types in the following option lists:
 *
 * - `google::cloud::CommonOptionList`
 * - `google::cloud::GrpcOptionList`
 * - `google::cloud::UnifiedCredentialsOptionList`
 * -
 * `google::cloud::privilegedaccessmanager_v1::PrivilegedAccessManagerPolicyOptionList`
 *
 * @note Unexpected options will be ignored. To log unexpected options instead,
 *     set `GOOGLE_CLOUD_CPP_ENABLE_CLOG=yes` in the environment.
 *
 * @param options (optional) Configure the `PrivilegedAccessManagerConnection`
 * created by this function.
 */
std::shared_ptr<PrivilegedAccessManagerConnection>
MakePrivilegedAccessManagerConnection(Options options = {});

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace privilegedaccessmanager_v1
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PRIVILEGEDACCESSMANAGER_V1_PRIVILEGED_ACCESS_MANAGER_CONNECTION_H
